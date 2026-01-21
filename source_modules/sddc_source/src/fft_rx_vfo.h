#pragma once
#include <signal_path/signal_path.h>
#include <fftw3.h>
#include <cstring>
#include <algorithm>
#include <volk/volk.h>

#include <thread>
#include <vector>

#include "pffft/pf_mixer.h"
#include "kaiser.h"

#define NDECIDX 7 // Support decimation ratios: 2,4,8,16,32,64,128

#define MAX_THREADS 5
constexpr int fftSize = 8192;
constexpr int halfFft = fftSize / 2;

namespace dsp::channel {
    /**
     * FFT-based RX VFO using overlap-save method for high-performance processing
     * - Input: Real (float) samples
     * - Output: Complex (IQ) samples
     * - Uses FFTW for efficient FFT/IFFT operations
     * - Minimum decimation rate of 2
     * - Optimized for high sample rates (up to 64M samples/s)
     *
     * The overlap-save method performs:
     * 1. Real-to-complex FFT on input blocks
     * 2. Frequency domain shift and filtering
     * 3. Complex IFFT with decimation
     */
    class FFTRxVFO : public Processor<int16_t, complex_t> {
        using base_type = Processor<int16_t, complex_t>;

    public:
        FFTRxVFO() {}

        ~FFTRxVFO() override {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            cleanup();
        }

        void init(stream<int16_t>* in, float gain) {
            _inSamplerate = 64000000;
            _outSamplerate = 0;

            _mtunebin = fftSize / 2 / 4;
            GainScale = gain;

            filter = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * halfFft);                  // halfFft
            ADCinTime = (float*)fftwf_malloc(1024 * 1024 * sizeof(float) + sizeof(float) * halfFft); // large enough buffer
            // make sure initial overlap region is zeroed to avoid reading uninitialized samples
            memset(ADCinTime, 0, sizeof(float) * halfFft);

            base_type::init(in);

            setOutSamplerate(128000000, 64000000);


            plan_t2f_r2c = fftwf_plan_dft_r2c_1d(2 * halfFft, ADCinTime, NULL, FFTW_PATIENT);
            for (int i = 0; i < NDECIDX; i++)
                plans_f2t_c2c[i] = fftwf_plan_dft_1d(halfFft / (1 << i), NULL, NULL, FFTW_BACKWARD, FFTW_PATIENT);
        }

        void setGainFactor(float gain) {
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            if (GainScale != gain) {
                GainScale = gain;
                generateFreqFilter(GainScale, _decimationIndex);
            }
        }

        void setInSamplerate(double inSamplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);

            if (_inSamplerate == inSamplerate) {
                return;
            }

            base_type::tempStop();

            _inSamplerate = inSamplerate;
            _decimationIndex = (int)(log2(_inSamplerate / _outSamplerate));
            if (_decimationIndex < 1) {
                _decimationIndex = 1;
                _outSamplerate = _inSamplerate / 2.0;
            }

            _decimationIndex -= 1; // index starts from zero

            // create each filters
            generateFreqFilter(GainScale, _decimationIndex);

            base_type::tempStart();
        }

        void setOutSamplerate(double outSamplerate, double bandwidth) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);

            if (_outSamplerate == outSamplerate) {
                return;
            }
            base_type::tempStop();
            _outSamplerate = outSamplerate;
            _decimationIndex = (int)(log2(_inSamplerate / _outSamplerate));
            if (_decimationIndex < 1) {
                _decimationIndex = 1;
                _outSamplerate = _inSamplerate / 2.0;
            }

            _decimationIndex -= 1; // index starts from zero
            generateFreqFilter(GainScale, _decimationIndex);
            base_type::tempStart();
        }

        void setBufferSize(int size) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            base_type::out.setBufferSize(size / 2); // each 2 real samples become 1 complex sample, and decimation
            fftwf_free(ADCinTime);
            ADCinTime = (float*)fftwf_malloc(size * sizeof(float) + sizeof(float) * halfFft); // large enough buffer
            // make sure initial overlap region is zeroed to avoid reading uninitialized samples
            memset(ADCinTime, 0, sizeof(float) * halfFft);
            base_type::tempStart();
        }

        void setOffset(double offset) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            if (offset > _inSamplerate)
                return;

            _lsb = offset < 0;

            if (offset < 0)
                offset = -offset;

            offset = offset / (_inSamplerate / 2.0f);
            // align to 1/4 of halfft
            _mtunebin = int(offset * halfFft / 4) * 4; // mtunebin step 4 bin  ?
            // handle the small freq drift
            float delta = ((float)_mtunebin / halfFft) - offset;
            float fc = delta * (1 << _decimationIndex); // ret increases with higher decimation
            // DbgPrintf("offset %f mtunebin %d delta %f (%f)\n", offset, this->mtunebin, delta, ret);
            if (_lsb) fc = -fc;

            if (this->fc != fc) {
                stateFineTune = shift_limited_unroll_C_sse_init(fc, 0.0F);
                this->fc = fc;
            }
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            stateFineTune = shift_limited_unroll_C_sse_init(fc, 0.0F);
            base_type::tempStart();
        }

        void start() override {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::start();

            shutdown = false;
            thread_count = std::clamp<unsigned>(std::thread::hardware_concurrency() - 1, 1, MAX_THREADS);

            for (int thread_id = 0; thread_id < thread_count; thread_id++) {
                workerThreads.emplace_back([this, thread_id]() {
                    size_t local_batch = 0;
                    auto ADCinFreq = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * (halfFft + 1)); // 1024+1
                    auto inFreqTmp = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * (halfFft));     // 1024

                    while (true) {
                        {
                            std::unique_lock<std::mutex> lock(m);

                            cv_work.wait(lock, [&] {
                                return shutdown || batch_id != local_batch;
                            });

                            if (shutdown) {
                                // exit
                                break;
                            }
                        }

                        local_batch = batch_id;

                        int workPerThread = (currentFftPerBuf + int(workerThreads.size()) - 1) / int(workerThreads.size());
                        int start_k = thread_id * workPerThread;
                        int stop_k = std::min(start_k + workPerThread, currentFftPerBuf);
                        int index = _decimationIndex;
                        processrange(start_k, stop_k, base_type::out.writeBuf, ADCinFreq, inFreqTmp, plan_t2f_r2c, plans_f2t_c2c[index]);

                        {
                            std::lock_guard<std::mutex> lock(m);
                            if (++completed == thread_count)
                                cv_done.notify_one();
                        }
                    }

                    fftwf_free(ADCinFreq);
                    fftwf_free(inFreqTmp);
                });
            }
        }

        void stop() override {
            {
                std::lock_guard<std::mutex> lock(m);
                shutdown = true;
                ++batch_id;
            }
            cv_work.notify_all();

            for (auto& th : workerThreads) {
                if (th.joinable()) {
                    th.join();
                }
            }
            workerThreads.clear();
        }

        void processrange(int start_k, int stop_k, complex_t* out, fftwf_complex* ADCinFreq, fftwf_complex* inFreqTmp, fftwf_plan plan_t2f_r2c, fftwf_plan plan_f2t_c2c) {
            int decimate;
            int mtunebin;
            bool lsb;

            decimate = _decimationIndex;
            mtunebin = _mtunebin;
            lsb = _lsb;

            // holds the FFT size for the current decimation level
            int mfft = halfFft / (1 << decimate); // = halfFft / 2^mdecimation

            // Calculate the parameters for the first half
            size_t shift_count = std::min(mfft / 2, halfFft - mtunebin);
            auto source = &ADCinFreq[mtunebin];
            // Calculate the parameters for the second half
            auto start = std::max(0, mfft / 2 - mtunebin);
            auto source2 = &ADCinFreq[mtunebin - mfft / 2];
            auto dest = &inFreqTmp[mfft / 2];
            auto filter2 = &filter[halfFft - mfft / 2];

            const int output_step = 3 * mfft / 4;
            for (int k = start_k; k < stop_k; k++) {
                // core of fast convolution including filter and decimation
                //   main part is 'overlap-scrap' (IMHO better name for 'overlap-save'), see
                //   https://en.wikipedia.org/wiki/Overlap%E2%80%93save_method
                // FFT first stage: time to frequency, real to complex
                // 'full' transformation size: 2 * halfFft
                fftwf_execute_dft_r2c(plan_t2f_r2c, ADCinTime + (3 * halfFft / 2) * k, ADCinFreq);

                // result now in ADCinFreq[]
                // circular shift (mixing in full bins) and low/bandpass filtering (complex multiplication)
                {
                    // circular shift tune fs/2 first half array into inFreqTmp[]
                    shift_freq(inFreqTmp, source, filter, shift_count);
                    if (mfft / 2 != shift_count)
                        memset(inFreqTmp[shift_count], 0, sizeof(*inFreqTmp) * (mfft / 2 - shift_count));

                    // circular shift tune fs/2 second half array
                    shift_freq(&dest[start], &source2[start], &filter2[start], mfft / 2 - start);
                    if (start != 0)
                        memset(inFreqTmp[mfft / 2], 0, sizeof(*inFreqTmp) * start);
                }
                // result now in inFreqTmp[]

                fftwf_execute_dft(plan_f2t_c2c, inFreqTmp, (fftwf_complex*)inFreqTmp); //  c2c decimation

                // postprocessing
                if (lsb) // lower sideband
                {
                    // mirror just by negating the imaginary Q of complex I/Q
                    if (k == 0) {
                        copy<true>(out, &inFreqTmp[mfft / 4], mfft / 2);
                    }
                    else {
                        copy<true>(out + mfft / 2 + (3 * mfft / 4) * (k - 1), &inFreqTmp[0], (3 * mfft / 4));
                    }
                }
                else // upper sideband
                {
                    if (k == 0) {
                        copy<false>(out, &inFreqTmp[mfft / 4], mfft / 2);
                    }
                    else {
                        copy<false>(out + mfft / 2 + (3 * mfft / 4) * (k - 1), &inFreqTmp[0], (3 * mfft / 4));
                    }
                }
                // result now in this->obuffers[]
            }
        }

        inline int process(int count, const int16_t* in, complex_t* out) {
            int decimate;

            {
                std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
                decimate = _decimationIndex;
            }

            // holds the FFT size for the current decimation level
            int mfft = halfFft / (1 << decimate); // = halfFft / 2^mdecimation

            // when arriving here, we have 'count' new samples in 'in'
            // and we have halffft samples overlapped from last time in ADCInTime

            // Convert input into ADCInTime buffer
            convert_float(in, &ADCinTime[halfFft], count);

            base_type::_in->flush();

            // calcuate how many times we should run fft/ifft pair
            // we can estimate how many output samples we can get
            const int fftPerBuf = count / (3 * halfFft / 2) + 1; // number of ffts per buffer with 256|768 overlap

            {
                std::lock_guard<std::mutex> lock(m);
                completed = 0;
                ++batch_id;
                this->currentFftPerBuf = fftPerBuf;
            }

            cv_work.notify_all();

            {
                std::unique_lock<std::mutex> lock(m);
                cv_done.wait(lock, [this] {
                    return completed == thread_count;
                });
            }

            // save last overlap samples for next time
            memmove(ADCinTime, &ADCinTime[count], sizeof(*ADCinTime) * halfFft);

            const int output_step = 3 * mfft / 4;
            int len = mfft / 2 + (fftPerBuf - 1) * output_step;
            if (this->fc != 0.0f) {
                shift_limited_unroll_C_sse_inp_c((complexf*)out, len, &stateFineTune);
            }

            return len;
        }

        int run() override {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int outCount = process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            if (outCount > 0) {
                if (!base_type::out.swap(outCount)) { return -1; }
            }
            return outCount;
        }

    protected:
        static inline void convert_float(const int16_t* input, float* output, int count) {
            volk_16i_s32f_convert_32f(output, input, 1.0f, count);
        }

        static inline void shift_freq(fftwf_complex* dest, const fftwf_complex* source1, const fftwf_complex* source2, size_t count) {
            // Use VOLK for complex multiplication
            volk_32fc_x2_multiply_32fc((lv_32fc_t*)(dest), (lv_32fc_t*)(source1), (lv_32fc_t*)(source2), (uint32_t)count);
        }

        template <bool flip>
        static inline void copy(complex_t* dest, const fftwf_complex* source, size_t count) {
            if constexpr (!flip) {
                memcpy(dest, source, count * sizeof(fftwf_complex));
            }
            else {
                // VOLK does not provide a direct function to negate imaginary part, so use a loop
                for (size_t i = 0; i < count; i++) {
                    dest[i].re = source[i][0];
                    dest[i].im = -source[i][1];
                }
            }
        }

        void generateFreqFilter(float gain, int index) {
            fftwf_plan filterplan_t2f_c2c; // time to frequency fft
            fftwf_complex* pfilterht;      // time filter ht

            pfilterht = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * halfFft); // halfFft
            filterplan_t2f_c2c = fftwf_plan_dft_1d(halfFft, pfilterht, filter, FFTW_FORWARD, FFTW_ESTIMATE);

            float* pht = new float[halfFft / 4 + 1];
            const float Astop = 120.0f;
            const float relPass = 0.82f; // 82% of Nyquist should be usable
            const float relStop = 1.1f;  // 'some' alias back into transition band is OK

            {
                // @todo: have dynamic bandpass filter size - depending on decimation
                //   to allow same stopband-attenuation for all decimations
                float Bw = 64.0f / (1 << index); // bandwidth relative to fs=64MHz
                // Bw *= 0.8f;  // easily visualize Kaiser filter's response
                KaiserWindow(halfFft / 4 + 1, Astop, relPass * Bw / 128.0f, relStop * Bw / 128.0f, pht);

                float gainadj = gain * 4096.0f / (float)(halfFft * 2); // reference is FFTN_R_ADC == 4096

                for (int t = 0; t < halfFft; t++) {
                    pfilterht[t][0] = pfilterht[t][1] = 0.0F;
                }

                for (int t = 0; t < (halfFft / 4 + 1); t++) {
                    pfilterht[halfFft - 1 - t][0] = gainadj * pht[t];
                }

                fftwf_execute_dft(filterplan_t2f_c2c, pfilterht, filter);
            }
            delete[] pht;
            fftwf_destroy_plan(filterplan_t2f_c2c);
            fftwf_free(pfilterht);
        }

        void cleanup() {
            if (filter != nullptr) {
                fftwf_free(filter);
                filter = nullptr;
            }

            if (ADCinTime != nullptr) {
                fftwf_free(ADCinTime);
                ADCinTime = nullptr;
            }
        }

    private:
        // fftwf_complex* filterHw[NDECIDX]; // Hw complex to each decimation ratio
        fftwf_complex* filter = nullptr;

        float* ADCinTime; // point to each threads input buffers [nftt]

        // Hardware scale factor
        float GainScale;

        // Parameters
        double _inSamplerate;
        double _outSamplerate;
        double _bandwidth;
        double _offset;
        int _blockSize;
        int _overlap;
        int _outputSize;
        int _decimationIndex = 1; // the index of decimation, log of 2
        bool _lsb;
        int _mtunebin;

        int currentFftPerBuf;
        std::mutex m;
        std::condition_variable cv_work;
        std::condition_variable cv_done;
        std::size_t completed = 0;
        std::size_t batch_id = 0;
        bool shutdown = false;
        int thread_count;

        std::vector<std::thread> workerThreads;

        fftwf_plan plans_f2t_c2c[NDECIDX];
        fftwf_plan plan_t2f_r2c; // fftw plan buffers Freq to Time complex to complex per decimation ratio

        float fc;
        shift_limited_unroll_C_sse_data_t stateFineTune;
    };
}

#pragma once
#include <assert.h>

#include "dsp/channel/frequency_xlator.h"
#include "dsp/multirate/rational_resampler.h"

namespace dsp::channel {
    class PFRxVFO : public Processor<int16_t, complex_t> {
        using base_type = Processor<int16_t, complex_t>;

    public:
        PFRxVFO() {
        }

        PFRxVFO(stream<int16_t>* in, double inSamplerate, double outSamplerate, double bandwidth, double offset) { init(in, inSamplerate, outSamplerate, bandwidth, offset); }

        ~PFRxVFO() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            taps::free(ftaps);
        }

        void init(stream<int16_t>* in, float gain) {
            gainFactor = gain;
            init(in, 64000000.0, 32000000, 20000000.0, 0.0);
        }

        void init(stream<int16_t>* in, double inSamplerate, double outSamplerate, double bandwidth, double offset) {
            _inSamplerate = inSamplerate;
            _outSamplerate = outSamplerate;
            _bandwidth = bandwidth;
            _offset = offset;
            filterNeeded = (_bandwidth != _outSamplerate);
            ftaps.taps = NULL;

            xlator.init(NULL, -(_offset - _inSamplerate / 4), _inSamplerate / 2);
            resamp.init(NULL, _inSamplerate / 2, _outSamplerate);
            generateTaps();
            filter.init(NULL, ftaps);

            base_type::init(in);
        }

        void setInSamplerate(double inSamplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _inSamplerate = inSamplerate;
            xlator.setOffset(-(_offset - _inSamplerate / 4), _inSamplerate / 2);
            resamp.setInSamplerate(_inSamplerate / 2);
            base_type::tempStart();
        }

        void setOutSamplerate(double outSamplerate, double bandwidth) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _outSamplerate = outSamplerate;
            _bandwidth = bandwidth;
            filterNeeded = (_bandwidth != _outSamplerate);
            resamp.setOutSamplerate(_outSamplerate);
            if (filterNeeded) {
                generateTaps();
                filter.setTaps(ftaps);
            }
            base_type::tempStart();
        }

        void setBandwidth(double bandwidth) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            std::lock_guard<std::mutex> lck2(filterMtx);
            _bandwidth = bandwidth;
            filterNeeded = (_bandwidth != _outSamplerate);
            if (filterNeeded) {
                generateTaps();
                filter.setTaps(ftaps);
            }
        }

        void setOffset(double offset) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _offset = offset;
            xlator.setOffset(-(_offset - _inSamplerate / 4), _inSamplerate / 2);
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            xlator.reset();
            resamp.reset();
            filter.reset();
            base_type::tempStart();
        }

        void setGainFactor(float gain) {
            gainFactor = gain;
        }

        void setAntiAlias(bool enable) {
            anti_alias = enable;
        }

        // Fast FIR for half FIR filter
        template <bool flip>
            static inline float fir_core(const int16_t* p) {
                float ret = ((p[-5] - p[5]) * -H0 + (p[-3] - p[3]) * -H2 + p[0] * H5);
            if (flip)
                return -ret;
            else
                return ret;
        }

        template <bool flip>
            static inline float fir_core_q(const int16_t* p) {
                float ret = ((p[-4] - p[6]) * -H0 + (p[-2] - p[4]) * -H2 + p[1] * H5);

            if (flip)
                return -ret;
            else
                return ret;
        }

        // 
        void convert_real_to_complex_filtered(const int16_t* real_in,
                                           complex_t* iq_out,
                                           int n_real) {
            const float scale = 1.0f / 32768.0f;
            const int n_iq = n_real / 2;
            assert(n_real % 4 == 0);

            int16_t header_buf[20];
            memcpy(header_buf, prev_samples, 10 * sizeof(int16_t));
            memcpy(header_buf + 10, real_in, 10 * sizeof(int16_t));

            // proess first 5 IQ samples from header_buf
            for (int i = 0; i < 5; i++) {
                const int16_t* p = &header_buf[i * 2 + 5];
                if (i & 1) {
                    iq_out[i].re = fir_core<true>(p);
                    iq_out[i].im = fir_core_q<false>(p);
                }
                else {
                    iq_out[i].re = fir_core<false>(p);
                    iq_out[i].im = fir_core_q<true>(p);
                }
            }

            for (int i = 5; i < n_iq - 3; i += 2) {
                const int16_t* p0 = &real_in[i * 2];
                
                iq_out[i].re = fir_core<true>(p0);
                iq_out[i].im = fir_core_q<false>(p0);

                const int16_t* p1 = p0 + 2;
                iq_out[i + 1].re = fir_core<false>(p1);
                iq_out[i + 1].im = fir_core_q<true>(p1);
            }

            memcpy(prev_samples, real_in + (n_real - 10), 10 * sizeof(int16_t));
        }

        /**
         * Fused Real-to-IQ conversion.
         * Input: 64MHz real (int16_t)
         * Output: 32MHz complex_t (float)
         * Bandwidth: 32MHz span (-16MHz to +16MHz)
         */
        void convert_real_to_complex_fused(const int16_t* real_in,
                                           complex_t* iq_out,
                                           int n_real) {
            const float scale = 1.0f / 32768.0f;

            assert(n_real % 4 == 0);
            // Process 4 real samples to produce 2 complex samples per iteration
            // assume n_real is multiple of 4
            for (int i = 0; i < n_real / 4; ++i) {
                int in_idx = i * 4;
                int out_idx = i * 2;

                // Pattern: [I=x0, Q=-x1], [I=-x2, Q=x3]

                // Complex Sample 0
                iq_out[out_idx].im = (float)real_in[in_idx] * scale;      // I =  x[0]
                iq_out[out_idx].re = (float)(-real_in[in_idx + 1]) * scale; // Q = -x[1]

                // Complex Sample 1
                iq_out[out_idx + 1].im = (float)(-real_in[in_idx + 2]) * scale; // I = -x[2]
                iq_out[out_idx + 1].re = (float)real_in[in_idx + 3] * scale;  // Q =  x[3]
            }
        }

        inline int process(int count, const int16_t* _in, complex_t* out) {

            const complex_t* in = out;

            if (anti_alias)
                convert_real_to_complex_filtered(_in, out, count);
            else
                convert_real_to_complex_fused(_in, out, count);
            count /= 2; // Each 2 real samples become 1 complex sample

            xlator.process(count, in, out);
            in = out;

            count = resamp.process(count, in, out);

            if (filterNeeded) {
                std::lock_guard<std::mutex> lck(filterMtx);
                filter.process(count, out, out);
            }

            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int outCount = process(count, base_type::_in->readBuf, out.writeBuf);

            // Swap if some data was generated
            base_type::_in->flush();
            if (outCount) {
                if (!out.swap(outCount)) { return -1; }
            }
            return outCount;
        }

    protected:
        void generateTaps() {
            taps::free(ftaps);
            double filterWidth = _bandwidth / 2.0;
            ftaps = taps::lowPass(filterWidth, filterWidth * 0.1, _outSamplerate);
        }

        FrequencyXlator xlator;
        multirate::RationalResampler<complex_t> resamp;
        filter::FIR<complex_t, float> filter;
        tap<float> ftaps;
        bool filterNeeded;

        double _inSamplerate;
        double _outSamplerate;
        double _bandwidth;
        double _offset;

        stream<complex_t> iq_samples;

        float gainFactor;
        complex_t result[STREAM_BUFFER_SIZE];

        // 11-taps coefficients for a simple half FIR low-pass filter with cutoff at 0.5
        static constexpr int TAPS_NUM = 11;
        bool anti_alias = true;
        int16_t prev_samples[TAPS_NUM - 1] = {0};
        static constexpr float H0 = 0.053720f / 32768.0f, H2 = -0.091576f / 32768.0f, H4 = 0.313132f / 32768.0f, H5 = 0.5f / 32768.0f;

        std::mutex filterMtx;
    };
}
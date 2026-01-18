#pragma once
#include "dsp/channel/frequency_xlator.h"
#include "dsp/multirate/rational_resampler.h"

namespace dsp::channel {
    class PFRxVFO : public Processor<int16_t, complex_t> {
        using base_type = Processor<int16_t, complex_t>;

    public:
        PFRxVFO() {
            dsp::buffer::clear<float>(nullBuffer, STREAM_BUFFER_SIZE);
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

            // Process 4 real samples to produce 2 complex samples per iteration
            // assume n_real is multiple of 4
            for (int i = 0; i < n_real / 4; ++i) {
                int in_idx = i * 4;
                int out_idx = i * 2;

                // Pattern: [I=x0, Q=-x1], [I=-x2, Q=x3]

                // Complex Sample 0
                iq_out[out_idx].im = (float)real_in[in_idx] * scale;      // I =  x[0]
                iq_out[out_idx].re = -(float)real_in[in_idx + 1] * scale; // Q = -x[1]

                // Complex Sample 1
                iq_out[out_idx + 1].im = -(float)real_in[in_idx + 2] * scale; // I = -x[2]
                iq_out[out_idx + 1].re = (float)real_in[in_idx + 3] * scale;  // Q =  x[3]
            }
        }

        inline int process(int count, const int16_t* _in, complex_t* out) {

            const complex_t* in = result;

            convert_real_to_complex_fused(_in, result, count);
            count /= 2; // Each 2 real samples become 1 complex sample

            xlator.process(count, in, out);
            in = out;

            if (_inSamplerate / 2 != _outSamplerate) {
                count = resamp.process(count, in, out);
            }

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
        float fbuffer[STREAM_BUFFER_SIZE];
        float nullBuffer[STREAM_BUFFER_SIZE];
        complex_t result[STREAM_BUFFER_SIZE];

        std::mutex filterMtx;
    };
}
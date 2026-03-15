#pragma once
#include "../processor.h"
#include <math.h>

namespace dsp::loop {
    template <class T>
    class Gain : public Processor<T, T> {
        using base_type = Processor<T, T>;
    public:
        Gain() {}

        Gain(stream<T>* in, double gain) { init(in, gain); }

        Gain(stream<T>* in, double gainDb, bool dbMode) {
            if (dbMode) {
                initDb(in, gainDb);
            }
            else {
                init(in, gainDb);
            }
        }

        void init(stream<T>* in, double gain) {
            _gain = gain;
            _initGain = gain;

            base_type::init(in);
        }

        void initDb(stream<T>* in, double gainDb) {
            init(in, dbToLinear(gainDb));
        }

        void setGain(double gain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _gain = gain;
        }

        void setGainDb(double gainDb) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _gain = dbToLinear(gainDb);
        }

        void setInitGain(double gain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _initGain = gain;
        }

        void setInitGainDb(double gainDb) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _initGain = dbToLinear(gainDb);
        }

        float gain() const {
            return _gain;
        }

        float gainDb() const {
            return 20.0f * log10f(_gain);
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _gain = _initGain;
        }

        inline int process(int count, T* in, T* out) {
            for (int i = 0; i < count; i++) {
                out[i] = in[i] * _gain;
            }
            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    protected:
        static float dbToLinear(double gainDb) {
            return powf(10.0f, gainDb / 20.0f);
        }

        float _gain = 1.0f;
        float _initGain = 1.0f;
    };
}
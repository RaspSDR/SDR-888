#pragma once
#include "../processor.h"
#include <cmath>

namespace dsp::loop {
    template <class T>
    class IFAGC : public Processor<T, T> {
        using base_type = Processor<T, T>;
    public:
        IFAGC() {}

        IFAGC(stream<T>* in, double initialGain, double maxGain, double distortionRate) {
            init(in, initialGain, maxGain, distortionRate);
        }

        /// @brief Initialize the IFAGC processor with input stream and parameters
        /// @param in: input stream
        /// @param initialGain: suggest value = 1.0
        /// @param maxGain: suggest value = 100000.0
        /// @param distortionRate: suggest value = 0.0001
        void init(stream<T>* in, double initialGain, double maxGain, double distortionRate) {
            _initialGain = initialGain;
            _maxGain = maxGain;
            _distortionRate = distortionRate;
            resetGain();
            base_type::init(in);
        }

        void setInitialGain(double initialGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _initialGain = initialGain;
        }

        void setMaxGain(double maxGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _maxGain = maxGain;
        }

        void setDistortionRate(double distortionRate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _distortionRate = distortionRate;
        }

        void resetGain() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _currentGain = _initialGain;
        }

        inline int process(int count, T* in, T* out) {
            for (int i = 0; i < count; i++) {
                // Apply current gain
                T scaled = in[i] * (T)_currentGain;
                out[i] = scaled;

                // Calculate signal power (norm)
                float signalPower = 0.0f;
                if constexpr (std::is_same_v<T, complex_t>) {
                    signalPower = std::norm(scaled);
                }
                else if constexpr (std::is_same_v<T, float>) {
                    signalPower = scaled * scaled;
                }

                // Tisserand-Berviller error function
                // Target level = 1.0
                float z = 1.0f + (float)_distortionRate * (1.0f - signalPower);
                _currentGain *= z;

                // Protect against NaN/Inf
                if (!std::isfinite(_currentGain)) {
                    resetGain();
                }
                else if (_currentGain > _maxGain) {
                    _currentGain = _maxGain;
                }
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
        double _initialGain = 1.0;
        double _maxGain = 100.0;
        double _distortionRate = 0.01;

        double _currentGain = 1.0;
    };
}

#pragma once
#include "../processor.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <cstring>

// Auto Notch Filter (ANF) ported from WDSP's anf.c/anf.h (NR0V).
// Works on stereo audio streams using per-channel adaptive NLMS.

namespace dsp::noise_reduction {
    class AutoNotch : public Processor<stereo_t, stereo_t> {
        using base_type = Processor<stereo_t, stereo_t>;
    public:
        AutoNotch() {}

        AutoNotch(stream<stereo_t>* in, int taps, int delay, float gain, float leakage) {
            init(in, taps, delay, gain, leakage);
        }

        void init(stream<stereo_t>* in, int taps, int delay, float gain, float leakage) {
            _bufSize = 2048;
            _mask = _bufSize - 1;

            _wL.assign(_bufSize, 0.0f);
            _wR.assign(_bufSize, 0.0f);
            _dL.assign(_bufSize, 0.0f);
            _dR.assign(_bufSize, 0.0f);

            _pos = 0;

            setParams(taps, delay, gain, leakage);
            resetState();
            base_type::init(in);
        }

        void setParams(int taps, int delay, float gain, float leakage) {
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);

            _taps = std::clamp(taps, 1, _bufSize);
            _delay = std::clamp(delay, 0, _bufSize - 1);
            _twoMu = std::max(gain, 0.0f);
            _gamma = std::max(leakage, 0.0f);

            _lidxL = _lidxR = _lidxInit;
            updateNgamma();
            resetState();
        }

        void setTaps(int taps) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _taps = std::clamp(taps, 1, _bufSize);
            resetState();
        }

        void setDelay(int delay) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _delay = std::clamp(delay, 0, _bufSize - 1);
            resetState();
        }

        void setGain(float gain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _twoMu = std::max(gain, 0.0f);
            resetState();
        }

        void setLeakage(float leakage) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _gamma = std::max(leakage, 0.0f);
            updateNgamma();
            resetState();
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            resetState();
        }

        inline int process(int count, stereo_t* in, stereo_t* out) {
            for (int i = 0; i < count; i++) {
                _dL[_pos] = in[i].l;
                _dR[_pos] = in[i].r;

                float yL = 0.0f, yR = 0.0f;
                float sigmaL = 0.0f, sigmaR = 0.0f;

                for (int j = 0; j < _taps; j++) {
                    int idx = (_pos + j + _delay) & _mask;
                    float dL = _dL[idx];
                    float dR = _dR[idx];

                    yL += _wL[j] * dL;
                    yR += _wR[j] * dR;

                    sigmaL += dL * dL;
                    sigmaR += dR * dR;
                }

                float invSigL = 1.0f / (sigmaL + 1e-10f);
                float invSigR = 1.0f / (sigmaR + 1e-10f);

                float errL = _dL[_pos] - yL;
                float errR = _dR[_pos] - yR;

                out[i].l = errL;
                out[i].r = errR;

                float nelL = std::fabs(errL * (1.0f - _twoMu * sigmaL * invSigL));
                float nevL = std::fabs(_dL[_pos] - (1.0f - _twoMu * _ngammaL) * yL - _twoMu * errL * sigmaL * invSigL);
                float nelR = std::fabs(errR * (1.0f - _twoMu * sigmaR * invSigR));
                float nevR = std::fabs(_dR[_pos] - (1.0f - _twoMu * _ngammaR) * yR - _twoMu * errR * sigmaR * invSigR);

                if (nevL < nelL) {
                    _lidxL += _lincr; if (_lidxL > _lidxMax) _lidxL = _lidxMax;
                } else {
                    _lidxL -= _ldecr; if (_lidxL < _lidxMin) _lidxL = _lidxMin;
                }

                if (nevR < nelR) {
                    _lidxR += _lincr; if (_lidxR > _lidxMax) _lidxR = _lidxMax;
                } else {
                    _lidxR -= _ldecr; if (_lidxR < _lidxMin) _lidxR = _lidxMin;
                }

                updateNgamma();

                float c0L = 1.0f - _twoMu * _ngammaL;
                float c0R = 1.0f - _twoMu * _ngammaR;
                float c1L = _twoMu * errL * invSigL;
                float c1R = _twoMu * errR * invSigR;

                for (int j = 0; j < _taps; j++) {
                    int idx = (_pos + j + _delay) & _mask;
                    _wL[j] = c0L * _wL[j] + c1L * _dL[idx];
                    _wR[j] = c0R * _wR[j] + c1R * _dR[idx];
                }

                _pos = (_pos + _mask) & _mask;
            }

            return count;
        }

    DEFAULT_PROC_RUN;

    protected:
        void resetState() {
            std::fill(_dL.begin(), _dL.end(), 0.0f);
            std::fill(_dR.begin(), _dR.end(), 0.0f);
            std::fill(_wL.begin(), _wL.end(), 0.0f);
            std::fill(_wR.begin(), _wR.end(), 0.0f);
            _pos = 0;
            _lidxL = _lidxR = _lidxInit;
            updateNgamma();
        }

        inline void updateNgamma() {
            float l4L = _lidxL * _lidxL; l4L *= l4L;
            float l4R = _lidxR * _lidxR; l4R *= l4R;
            _ngammaL = _gamma * l4L * _denMult;
            _ngammaR = _gamma * l4R * _denMult;
        }

        int _taps = 64;
        int _delay = 16;
        float _twoMu = 0.0001f;
        float _gamma = 0.10f;

        float _lidxInit = 1.0f;
        float _lidxMin = 0.1f;
        float _lidxMax = 5.0f;
        float _ngammaL = 0.0f, _ngammaR = 0.0f;
        float _denMult = 1.0f;
        float _lincr = 0.001f;
        float _ldecr = 0.0001f;

        float _lidxL = 1.0f, _lidxR = 1.0f;

        std::vector<float> _wL, _wR;
        std::vector<float> _dL, _dR;

        int _pos = 0;
        int _mask = 0;
        int _bufSize = 2048;

    };
}

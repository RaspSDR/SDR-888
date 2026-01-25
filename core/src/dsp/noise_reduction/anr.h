#pragma once
#include "../processor.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <mutex>

// Adaptive Noise Reduction (ANR) aligned with WDSP implementation.
// Based on NR0V's ALE-style NLMS with dynamic leakage control
// (lidx / ngamma adaptation).

namespace dsp::noise_reduction {
    class ANR : public Processor<stereo_t, stereo_t> {
        using base_type = Processor<stereo_t, stereo_t>;
    public:
        ANR() {}

        void init(stream<stereo_t>* in, int intensity) {
            // Mirror WDSP defaults: fixed power-of-two delay line (2048)
            _bufSize = 2048;
            _mask = _bufSize - 1;
            _maxTaps = 128;
            _maxDelay = 128;

            _wL.assign(_maxTaps, 0.0f);
            _wR.assign(_maxTaps, 0.0f);
            _dL.assign(_bufSize, 0.0f);
            _dR.assign(_bufSize, 0.0f);

            _pos = 0;

            setIntensity(intensity);
            resetState();
            base_type::init(in);
        }

        void setIntensity(int intensity) {
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _intensity = std::clamp(intensity, 0, 30);
            if (_intensity <= 0) return;

            float norm = (float)_intensity / 30.0f;

            // Parameter mapping inspired by WDSP defaults
            _taps = (_intensity < 8) ? 32 : (_intensity < 18 ? 64 : 128);
            _delay = 16 + (int)(48.0f * norm);                 // 16..64
            _twoMu = 0.00012f + (0.00138f * norm);             // 1.2e-4..1.5e-3
            _gamma = 0.00005f + (0.00045f * norm);             // 5e-5..5e-4
            _residualBlend = 0.10f + 0.30f * norm;             // bleed residual back to keep loudness
            _lidxInit = 1.0f;
            _lidxMin = 0.1f;
            _lidxMax = 5.0f;
            _denMult = 1.0f;
            _lincr = 0.0005f + (0.0025f * norm);
            _ldecr = 0.00005f + (0.00045f * norm);
            // refresh adaptive leakage state
            _lidxL = _lidxR = _lidxInit;
            updateNgamma();
            resetWeights();
        }

        inline int process(int count, stereo_t* in, stereo_t* out) {
            if (_intensity <= 0) {
                memcpy(out, in, count * sizeof(stereo_t));
                return count;
            }

            for (int i = 0; i < count; i++) {
                // push sample into delay line
                _dL[_pos] = in[i].l;
                _dR[_pos] = in[i].r;

                int start = (_pos + _delay) & _mask;

                float yL = 0.0f, yR = 0.0f;
                float sigmaL = 0.0f, sigmaR = 0.0f;

                for (int j = 0; j < _taps; j++) {
                    int idx = (start + j) & _mask;
                    float dL = _dL[idx];
                    float dR = _dR[idx];
                    yL += _wL[j] * dL;
                    yR += _wR[j] * dR;
                    sigmaL += dL * dL;
                    sigmaR += dR * dR;
                }

                // guard against div/0
                float invSigL = 1.0f / (sigmaL + 1e-10f);
                float invSigR = 1.0f / (sigmaR + 1e-10f);

                float errL = _dL[_pos] - yL;
                float errR = _dR[_pos] - yR;

                // envelope comparison drives leakage index
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
                    int idx = (start + j) & _mask;
                    _wL[j] = c0L * _wL[j] + c1L * _dL[idx];
                    _wR[j] = c0R * _wR[j] + c1R * _dR[idx];
                }

                // Light loudness compensation: blend residual, then match envelope
                float yMixL = yL + _residualBlend * errL;
                float yMixR = yR + _residualBlend * errR;

                _envInL = (_envInL * 0.995f) + (std::fabs(_dL[_pos]) * 0.005f);
                _envInR = (_envInR * 0.995f) + (std::fabs(_dR[_pos]) * 0.005f);
                _envOutL = (_envOutL * 0.995f) + (std::fabs(yMixL) * 0.005f);
                _envOutR = (_envOutR * 0.995f) + (std::fabs(yMixR) * 0.005f);

                float tgtGainL = (_envOutL > 1e-6f) ? std::clamp(_envInL / _envOutL, 0.5f, 3.0f) : 1.0f;
                float tgtGainR = (_envOutR > 1e-6f) ? std::clamp(_envInR / _envOutR, 0.5f, 3.0f) : 1.0f;
                _gainL = (_gainL * 0.99f) + (tgtGainL * 0.01f);
                _gainR = (_gainR * 0.99f) + (tgtGainR * 0.01f);

                out[i].l = yMixL * _gainL;
                out[i].r = yMixR * _gainR;

                // decrement index modulo buffer size
                _pos = (_pos + _mask) & _mask;
            }
            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            base_type::ctrlMtx.lock();
            process(count, base_type::_in->readBuf, base_type::out.writeBuf);
            base_type::ctrlMtx.unlock();

            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    protected:
        void resetWeights() {
            std::fill(_wL.begin(), _wL.end(), 0.0f);
            std::fill(_wR.begin(), _wR.end(), 0.0f);
            _pos = 0;
        }

        void resetState() {
            std::fill(_dL.begin(), _dL.end(), 0.0f);
            std::fill(_dR.begin(), _dR.end(), 0.0f);
            resetWeights();
            _lidxL = _lidxR = _lidxInit;
            updateNgamma();
            _envInL = _envInR = 0.0f;
            _envOutL = _envOutR = 0.0f;
            _gainL = _gainR = 1.0f;
        }

        inline void updateNgamma() {
            // ngamma = gamma * lidx^4 * den_mult
            float l4L = _lidxL * _lidxL; l4L *= l4L;
            float l4R = _lidxR * _lidxR; l4R *= l4R;
            _ngammaL = _gamma * l4L * _denMult;
            _ngammaR = _gamma * l4R * _denMult;
        }

        int _intensity = 0;
        int _taps = 32;
        int _delay = 32;
        float _twoMu = 0.0005f;
        float _gamma = 0.0001f;
        float _residualBlend = 0.10f;
        float _lidxInit = 1.0f;
        float _lidxMin = 0.1f;
        float _lidxMax = 5.0f;
        float _lincr = 0.001f;
        float _ldecr = 0.0001f;
        float _denMult = 1.0f;

        float _lidxL = 1.0f, _lidxR = 1.0f;
        float _ngammaL = 0.0f, _ngammaR = 0.0f;

        float _envInL = 0.0f, _envInR = 0.0f;
        float _envOutL = 0.0f, _envOutR = 0.0f;
        float _gainL = 1.0f, _gainR = 1.0f;

        std::vector<float> _wL, _wR;
        std::vector<float> _dL, _dR;

        int _pos = 0, _mask = 0, _maxTaps = 128, _maxDelay = 128, _bufSize = 2048;
    };
}
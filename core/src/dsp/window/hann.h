#pragma once
#include "cosine.h"

namespace dsp::window {
    // Coherent gain (DC gain) of the Hann window
    constexpr double HANN_COHERENT_GAIN = 0.5;

    inline double hann(double n, double N) {
        const double coefs[] = { 0.5, 0.5 };
        return cosine(n, N, coefs, sizeof(coefs) / sizeof(double));
    }
}
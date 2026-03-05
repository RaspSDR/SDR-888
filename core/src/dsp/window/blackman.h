#pragma once
#include "cosine.h"

namespace dsp::window {
    // Coherent gain (DC gain) of the Blackman window
    constexpr double BLACKMAN_COHERENT_GAIN = 0.42;

    inline double blackman(double n, double N) {
        const double coefs[] = { 0.42, 0.5, 0.08 };
        return cosine(n, N, coefs, sizeof(coefs) / sizeof(double));
    }
}
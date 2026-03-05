#pragma once
#include "cosine.h"

namespace dsp::window {
    // Coherent gain (DC gain) of the Nuttall window
    constexpr double NUTTALL_COHERENT_GAIN = 0.355768;

    inline double nuttall(double n, double N) {
        const double coefs[] = { 0.355768, 0.487396, 0.144232, 0.012604 };
        return cosine(n, N, coefs, sizeof(coefs) / sizeof(double));
    }
}
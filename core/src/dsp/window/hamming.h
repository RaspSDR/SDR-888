#pragma once
#include "cosine.h"

namespace dsp::window {
    // Coherent gain (DC gain) of the Hamming window
    constexpr double HAMMING_COHERENT_GAIN = 0.54;

    inline double hamming(double n, double N) {
        const double coefs[] = { 0.54, 0.46 };
        return cosine(n, N, coefs, sizeof(coefs) / sizeof(double));
    }
}
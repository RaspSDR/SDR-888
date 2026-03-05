#pragma once
#include <math.h>
#include "cosine.h"

namespace dsp::window {
    // Coherent gain (DC gain) of the Blackman-Harris window
    constexpr double BLACKMAN_HARRIS_COHERENT_GAIN = 0.35875;

    inline double blackmanHarris(double n, double N) {
        const double coefs[] = { 0.35875, 0.48829, 0.14128, 0.01168 };
        return cosine(n, N, coefs, sizeof(coefs) / sizeof(double));
    }
}
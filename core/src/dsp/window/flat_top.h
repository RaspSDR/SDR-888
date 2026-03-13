#pragma once
#include "cosine.h"

namespace dsp::window {
    // Coherent gain (DC gain) of the 5-term flat-top window
    constexpr double FLAT_TOP_COHERENT_GAIN = 0.21557895;

    inline double flatTop(double n, double N) {
        const double coefs[] = { 0.21557895, 0.41663158, 0.277263158, 0.083578947, 0.006947368 };
        return cosine(n, N, coefs, sizeof(coefs) / sizeof(double));
    }
}
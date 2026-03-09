/// Handle for Alex's custom GPIO control on RX888PRO
#pragma once
#include <stdint.h>

// Alex band definitions (Hz)
struct AlexBand {
    const char* name;
    uint64_t minFreq;
    uint64_t maxFreq;
    uint8_t controlBits;
};

// Standard Alex bandpass filter configuration
static const AlexBand ALEX_BANDS[] = {
    {"160m",  1800000ULL,  2000000ULL, 0b000},  // 1.8-2.0 MHz
    {"80m",   3500000ULL,  4000000ULL, 0b001},  // 3.5-4.0 MHz
    {"40m",   7000000ULL,  7300000ULL, 0b010},  // 7.0-7.3 MHz
    {"30m",  10100000ULL, 10150000ULL, 0b011},  // 10.1-10.15 MHz
    {"20m",  14000000ULL, 14350000ULL, 0b100},  // 14.0-14.35 MHz
    {"17m",  18068000ULL, 18168000ULL, 0b101},  // 18.068-18.168 MHz
    {"15m",  21000000ULL, 21450000ULL, 0b110},  // 21.0-21.45 MHz
    {"10m",  28000000ULL, 29700000ULL, 0b111},  // 28.0-29.7 MHz
    // Extended Alex (if supported)
    {"6m",   50000000ULL, 54000000ULL, 0b111}   // Shared with 10m or use extended logic
};

uint8_t getAlexBandControlBits(uint64_t freq) {
    for (const auto& band : ALEX_BANDS) {
        if (freq >= band.minFreq && freq <= band.maxFreq) {
            return band.controlBits;
        }
    }

    return 0b111; // use 111 for out-of-band frequencies (bypass)
}
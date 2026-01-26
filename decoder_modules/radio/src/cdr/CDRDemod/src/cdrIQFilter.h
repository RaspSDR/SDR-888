#ifndef CDR_IQFILTER_H
#define CDR_IQFILTER_H

#define _USE_Liquid_ 0

#include <cdrDemod.h>

#if _USE_Liquid_
#include <liquid.h>
#endif

#define LENGTH_Spectrum1 256
#define MASK_Spectrum1   0x1FF
extern  float Coefficients_Spectrum1 [LENGTH_Spectrum1];

#define LENGTH_Spectrum2 146
#define MASK_Spectrum2   0x1FF
extern  float Coefficients_Spectrum2 [LENGTH_Spectrum2];

#define LENGTH_Spectrum9 128
#define MASK_Spectrum9   0xFF
extern  float Coefficients_Spectrum9 [LENGTH_Spectrum9];

#define LENGTH_Spectrum10 128
#define MASK_Spectrum10   0xFF
extern  float Coefficients_Spectrum10 [LENGTH_Spectrum10];

#define LENGTH_Spectrum22 256
#define MASK_Spectrum22   0x1FF
extern  float Coefficients_Spectrum22 [LENGTH_Spectrum22];

#define LENGTH_Spectrum23 256
#define MASK_Spectrum23   0x1FF
extern  float Coefficients_Spectrum23 [LENGTH_Spectrum23];

typedef struct firFilter
{   
    float* coe;
    int filterLength;

    cdr_complex* histroyCircle;
    int historyLength;
    int historyMask;

    int circleStart;
#if _USE_Liquid_
    firfilt_crcf liquid_FirFilter;
#endif
}firFilter;

firFilter* IQFilter_Init (void);

void IQFilter_Release (firFilter* filter);

void IQFilter_Reset (firFilter* filter);

int IQFilter_Process (firFilter* filter, cdr_complex* input, cdr_complex* output, int length);

void IQFilter_SetSpectrumMode(firFilter* handle, enum SpectrumType spectrumMode);

#endif
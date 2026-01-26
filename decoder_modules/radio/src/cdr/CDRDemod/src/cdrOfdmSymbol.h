#ifndef CDR_OFDMSYMBOL_H 
#define CDR_OFDMSYMBOL_H

#include <cdrDemod.h>

#include "cdrFFTW.h"

typedef struct ofdmConst
{
    int T_u;       
    int T_cp;     
    int T_s;            

    int S_N;       

    cdrFFTW *fft_handle;
}ofdmConst;

ofdmConst* OfdmConst_Init (enum TransmissionMode transMode, enum SpectrumType specMode);

void OfdmConst_Release(ofdmConst* handle);

void OfdmConst_Reset (ofdmConst* handle, enum TransmissionMode transMode, enum SpectrumType specMode);

#endif
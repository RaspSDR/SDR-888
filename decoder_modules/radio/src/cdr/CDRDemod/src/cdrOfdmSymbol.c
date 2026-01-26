#include "cdrOfdmSymbol.h"

#include "cdrOfdmConst.h"
#include "cdrOFDMDemod.h"

#include <stdlib.h>

static void configOfdmConst (ofdmConst* handle, enum TransmissionMode transMode, enum SpectrumType specMode)
{
    int T_u, T_cp, S_N;
    
    switch (transMode)
    {
        case TransMode2:
            T_u  = OFDM_TuLength_Mode2;
            T_cp = OFDM_TcpLength_Mode2;
            S_N  = OFDM_SN_Mode2;
            break;
        case TransMode3:
            T_u  = OFDM_TuLength_Mode3;
            T_cp = OFDM_TcpLength_Mode3;
            S_N  = OFDM_SN_Mode3;
            break;
        case TransMode1:    
        default:
            T_u  = OFDM_TuLength_Mode1;
            T_cp = OFDM_TcpLength_Mode1;
            S_N  = OFDM_SN_Mode1;
            break;
    }
    handle->T_u  = T_u;
    handle->T_cp = T_cp;
    handle->S_N  = S_N;
    handle->T_s  = T_u + T_cp;

    handle->fft_handle = cdr_FFTW_Init (T_u, 1);
    return;
}

ofdmConst* OfdmConst_Init (enum TransmissionMode transMode, enum SpectrumType specMode)
{
    ofdmConst *handle = (ofdmConst*) malloc (sizeof(ofdmConst));

    configOfdmConst (handle, transMode, specMode);

    return handle;
}

void OfdmConst_Release(ofdmConst* handle)
{
    cdr_FFTW_Release (handle->fft_handle);
    
    free (handle);
}

void OfdmConst_Reset (ofdmConst* handle, enum TransmissionMode transMode, enum SpectrumType specMode)
{
    configOfdmConst (handle, transMode, specMode);
}


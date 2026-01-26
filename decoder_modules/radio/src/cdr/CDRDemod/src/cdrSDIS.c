#include "cdrSDIS.h"

#include "cdrSysInfo.h"
#include "cdrUtility.h"
#include "cdrModulation.h"
#include "cdrInterleave.h"
#include "cdrViterbiCodec.h"
#include "cdrScramble.h"

#include <stdlib.h>

#define SDIS_NO_ERROR  cdr_NO_ERROR
#define SDIS_VITERBI_FAIL -1037

#define SDIS_BitsDemod_Max (SDIS_Symbols_Mode1 * 2 * 6)

#define SDIS_BitsDeCode_Max (SDIS_BitsDemod_Max/4-6)

#define SDIS_Bytes_Max (SDIS_BitsDeCode_Max/8+1)

static void DecodeSDIS_ResizeBuffer(sdisHandle* handle, cdrSysInfo* sysInfo)
{
    int demodBits_length;
    int base_symbols;
    int modulation_scale, band_scale;

    switch (sysInfo->TransMode)
    {        
        case TransMode2:
            base_symbols = SDIS_Symbols_Mode2;
            break;
        case TransMode3:
            base_symbols = SDIS_Symbols_Mode3;
            break;
        case TransMode1: 
        default:
            base_symbols = SDIS_Symbols_Mode1;
            break;
    }
    switch (sysInfo->SpectrumMode)
    {
        case SpecMode2:    
        case SpecMode10:
        case SpecMode23:
            band_scale = 2;
            break;
        case SpecMode1:    
        case SpecMode9:
        case SpecMode22:
        default:
            band_scale = 1;
            break;
    }
    switch (sysInfo->SDISModType)
    {        
        case QAM16:
            modulation_scale = 4;
            handle->constellation->constellationIQ = mod_16QAM;
            handle->constellation->bitsPerSymbol   = modulation_scale;
            break;
        case QAM64:
            modulation_scale = 6;
            handle->constellation->constellationIQ = mod_64QAM;
            handle->constellation->bitsPerSymbol   = modulation_scale;
            break;
        case QPSK: 
        default:
            modulation_scale = 2;
            handle->constellation->constellationIQ = mod_QPSK;
            handle->constellation->bitsPerSymbol   = modulation_scale;
            break;
    }
    
    handle->N_I  = band_scale;    
    
    demodBits_length = base_symbols * modulation_scale * band_scale;
    if (handle->demodBits->totalSize < demodBits_length)
    {
        cdr_byteArray_Release (handle->demodBits);
        cdr_byteArray_Release (handle->sdisBytes);
        ReleaseDeInterleaveTable (handle->deInterleaveTable);
        ReleaseDeConvolutionPath (handle->deConvolutionPath);

        handle->demodBits         = cdr_byteArray_Init (demodBits_length);
        handle->sdisBytes         = cdr_byteArray_Init ((demodBits_length/4-6)/8+1);
        handle->deInterleaveTable = GenerateDeInterleaveTable (demodBits_length);
        handle->deConvolutionPath = GenerateDeConvolutionPath (demodBits_length);
    }

    return;
}

sdisHandle* DecodeSDIS_Init(void)
{
    sdisHandle* handle = (sdisHandle*) malloc (sizeof (sdisHandle));

    handle->constellation    = (ConstellationInfo*) malloc (sizeof (ConstellationInfo));
    handle->N_I              = 1;
    handle->demodBits        = cdr_byteArray_Init   (SDIS_Symbols_Mode1*2);
    handle->sdisBytes        = cdr_byteArray_Init (((SDIS_Symbols_Mode1*2)/4-6)/8+1);

    handle->deInterleaveTable = GenerateDeInterleaveTable (SDIS_Symbols_Mode1*2);
    handle->deConvolutionPath = GenerateDeConvolutionPath (SDIS_Symbols_Mode1*2);

    return handle;
}

void DecodeSDIS_Release(sdisHandle* handle)
{
    free (handle->constellation);
    
    cdr_byteArray_Release (handle->demodBits);
    cdr_byteArray_Release (handle->sdisBytes);
    
    ReleaseDeInterleaveTable (handle->deInterleaveTable);
    ReleaseDeConvolutionPath (handle->deConvolutionPath);

    free (handle);

    return;
}

int DecodeSDIS_Process(sdisHandle* handle, cdr_complexFrame* SDISSymbols, cdrSysInfo* sysInfo)
{
    int error;
    int lastError = SDIS_NO_ERROR;

    DecodeSDIS_ResizeBuffer (handle, sysInfo);

    cdr_complexArray *symbols = SDISSymbols->dataHandle;

    DeConstellation(symbols, handle->demodBits, handle->constellation);    

    DeInterleaveByTable_byte(handle->demodBits, handle->deInterleaveTable);

    error = DecodeConvolution(handle->demodBits, handle->deConvolutionPath);
    lastError = ErrorHandling (error, SDIS_VITERBI_FAIL, lastError);
    if (error < SDIS_NO_ERROR)
    {
        ;
    }

    DeScramble(handle->demodBits, handle->demodBits);

    BitsToBytes(handle->demodBits, handle->sdisBytes);

    return lastError;
}

cdr_byteArray* DecodeSDIS_GetBytes (sdisHandle* handle)
{
    return handle->sdisBytes;
}
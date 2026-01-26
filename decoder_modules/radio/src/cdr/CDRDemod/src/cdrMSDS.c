#include "cdrMSDS.h"

#include "cdrSysInfo.h"
#include "cdrInterleave.h"
#include "cdrModulation.h"
#include "cdrLDPCCodec.h"
#include "cdrViterbiCodec.h"
#include "cdrScramble.h"
#include "cdrUtility.h"

#include <stdlib.h>

#define MSDS_NO_ERROR  cdr_NO_ERROR
#define MSDS_LDPC_FAIL -1036


#define MSDS_BitsDemod (MSDS_Symbols_Mode1*2)

#define MSDS_BitsDecode (MSDS_BitsDemod*3/4)

#define MSDS_Bytes (MSDS_BitsDecode/8)


static void ExchangeData (cdr_complexFrame* input, int N_I, int reverse)
{
    if (N_I==1)
    {
        return;
    }

    cdr_complex *data_j_s, *temp_l_0;
    cdr_complex temp;

    int l, j;             
    int delta_j_l;
    int band_size;        

    int numOfLines = input->numOfLines / N_I;    

    for (int i=0; i<numOfLines; i++)
    {   
        band_size = input->linePosition[i]->subLength;

        l = 0; 
        j = (i * (N_I-1) + l) % N_I;
        delta_j_l = reverse ? (l-j) : (j-l);

        for (int k=0; k<band_size;k++)
        {
            l=0;
            temp_l_0 = input->linePosition[i]->handle + k;   
            for (int s=0; s<N_I; s++)
            {
                j = (delta_j_l + l + N_I) % N_I;

                data_j_s = input->linePosition[j * numOfLines + i]->handle + k;

                temp = *(data_j_s);           
                *(data_j_s) = *temp_l_0;      
                *temp_l_0 = temp;                
                l = j;                        
            }
        }
    }

    return;
}

static void DecodeMSDS_ResizeBuffer(msdsHandle* handle, cdrSysInfo* sysInfo)
{
    int interleaveLength, demodSymbols_Length, demodBits_Length;
    int base_symbols;
    int modulation_scale, band_scale, ldpc_scale, ldpc_divide;

    switch (sysInfo->TransMode)
    {        
        case TransMode2:
            base_symbols = MSDS_Symbols_Mode2;
            break;
        case TransMode3:
            base_symbols = MSDS_Symbols_Mode3;
            break;
        case TransMode1: 
        default:
            base_symbols = MSDS_Symbols_Mode1;
            break;
    }
    interleaveLength = base_symbols;

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
    handle->N_I = band_scale;

    switch (sysInfo->MSDSModType)
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
    
    switch (sysInfo->LDPCRate1)
    {
        case Rate1_4:
            ldpc_scale  = 1;
            ldpc_divide = 4;
            break;
        case Rate1_3:
            ldpc_scale  = 1;
            ldpc_divide = 3;
            break;
        case Rate1_2:
            ldpc_scale  = 1;
            ldpc_divide = 2;
            break;
        case Rate3_4:
        default:
            ldpc_scale  = 3;
            ldpc_divide = 4;
            break;
    }

    demodSymbols_Length = base_symbols * band_scale;
    if (interleaveLength != handle->deInterleaveTable->N_mux)
    {
        ReleaseDeInterleaveTable (handle->deInterleaveTable);    
        handle->deInterleaveTable   = GenerateDeInterleaveTable (demodSymbols_Length);
    }

    demodBits_Length = base_symbols * band_scale * modulation_scale;
    if (handle->deModBits->totalSize < demodBits_Length)
    {
        cdr_byteArray_Release (handle->deModBits);
        cdr_byteArray_Release (handle->msdsBytes);

        handle->deModBits  = cdr_byteArray_Init (demodBits_Length);
        handle->msdsBytes  = cdr_byteArray_Init (demodBits_Length*ldpc_scale/ldpc_divide/8+1);
    }

    return;
}

msdsHandle* DecodeMSDS_Init(void)
{
    msdsHandle* handle = (msdsHandle*) malloc (sizeof (msdsHandle));

    handle->N_I                 = 1;
    handle->constellation       =  (ConstellationInfo*) malloc (sizeof (ConstellationInfo)); 
    handle->deModBits           = cdr_byteArray_Init  (MSDS_Symbols_Mode1*2);                      
    handle->msdsBytes           = cdr_byteArray_Init(((MSDS_Symbols_Mode1*2)*3/4)/8+1);           
    handle->deInterleaveTable   = GenerateDeInterleaveTable (MSDS_Symbols_Mode1);
    return handle;
}

void DecodeMSDS_Release(msdsHandle* handle)
{
    free (handle->constellation);

    ReleaseDeInterleaveTable (handle->deInterleaveTable);
    
    cdr_byteArray_Release (handle->deModBits);
    cdr_byteArray_Release (handle->msdsBytes);

    free (handle);

    return;
}

int DecodeMSDS_Process(msdsHandle* handle, cdr_complexFrame* msdsSymbols, cdrSysInfo* sysInfo)
{
    int error;
    int lastError = MSDS_NO_ERROR;

    DecodeMSDS_ResizeBuffer (handle, sysInfo);

    cdr_complexArray *symbols = msdsSymbols->dataHandle;

    ExchangeData (msdsSymbols, handle->N_I, 0);
    
    DeInterleaveByTable_complex32(symbols, handle->deInterleaveTable);

    ExchangeData (msdsSymbols, handle->N_I, 1);

    DeConstellation(symbols, handle->deModBits, handle->constellation);

    error = LdpcDecoder(handle->deModBits, handle->deModBits, sysInfo->LDPCRate1);
    lastError = ErrorHandling (error, MSDS_LDPC_FAIL, lastError);
    if (error < MSDS_NO_ERROR)
    {
        return lastError;
    }

    DeScramble(handle->deModBits, handle->deModBits);

    BitsToBytes(handle->deModBits, handle->msdsBytes);

    return lastError;
}

cdr_byteArray* MSDS_GetDecodeBytes(msdsHandle* handle)
{
    return handle->msdsBytes;
}


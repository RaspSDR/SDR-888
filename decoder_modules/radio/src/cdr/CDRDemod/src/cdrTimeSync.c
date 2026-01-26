#include "cdrTimeSync.h"

#include "cdrOfdmConst.h"
#include "cdrOFDMDemod.h"
#include "cdrUtility.h"
#include "cdrFFTW.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void genPb (syncConst* syncInfo,  cdr_complex *freq_Domain)
{   
    int fft_Length     = syncInfo->T_b;
    int numOfHalfBands = syncInfo->N_I_x2;
    double phase_step  = M_PI * syncInfo->m / syncInfo->N_zc;
    
    int halfband_index = 0;
    int min_Index = syncInfo->validSyncBins[halfband_index][0];
    int max_Index = syncInfo->validSyncBins[halfband_index][1];

    int Pb_index=0;
    for (int k=0; k<fft_Length; k++)
    {
        if (k < min_Index)
        {
            freq_Domain [k] = cdr_complex_Zero;
        }
        else if (k < max_Index)
        {
            float phase = (float)(Pb_index*(Pb_index+1)*phase_step);
            if ((Pb_index&0x01) == 0)
                freq_Domain[k] = cosf(phase) - sinf(phase)*I;
            else
                freq_Domain[k] = cosf(phase) + sinf(phase)*I;
            
            Pb_index++;
        }
        else if (k == max_Index)
        {
            freq_Domain [k] = cdr_complex_Zero;

            halfband_index++;
            if (halfband_index < numOfHalfBands)
            {
                min_Index = syncInfo->validSyncBins[halfband_index][0];
                max_Index = syncInfo->validSyncBins[halfband_index][1];
            }
            else
            {
                min_Index = fft_Length;
                max_Index = fft_Length;
            }
        }
    }
}

static void genSyncSymbol (syncConst* syncInfo)
{
    int fft_Length = syncInfo->T_b;
    int T_Bcp      = syncInfo->T_Bcp;
    int T_Sync     =  syncInfo->T_Sync;

    syncInfo->Sync_Symbols = (cdr_complex*) malloc (sizeof(cdr_complex)* (T_Sync));

    cdrFFTW* fft_handle;
    cdr_complex *sync_FreqDomain = syncInfo->Sync_Symbols;
    cdr_complex *sync_TimeDomain = syncInfo->Sync_Symbols;

    genPb (syncInfo, sync_FreqDomain);

    fft_handle = cdr_FFTW_Init(fft_Length, 0);
    cdr_FFTW_ProccessShifted(fft_handle, sync_FreqDomain, sync_FreqDomain);
    cdr_FFTW_Release(fft_handle);

    ArrayCopy_complex (sync_FreqDomain, sync_TimeDomain+fft_Length, fft_Length-T_Bcp);
}

static void configSyncConst (syncConst* handle, enum TransmissionMode transMode, enum SpectrumType specMode)
{
    int T_b, T_Bcp;
    int L, m, N_zc;
    const int (*validSyncBins)[2];   

    switch (transMode)
    {
        case TransMode2:
            T_Bcp = SYNC_BCPLength_Mode2;
            T_b   = SYNC_NbLength_Mode2;
            L     = SYNC_L_Mode2;
            m     = SYNC_m_Mode2;
            N_zc  = SYNC_Nzc_Mode2;
            switch (specMode)
            {
                case SpecMode1:
                    validSyncBins = SyncBin_Mode2_Spec1;
                    break;
                case SpecMode2:
                    validSyncBins = SyncBin_Mode2_Spec2;
                    break;
                case SpecMode10:
                    validSyncBins = SyncBin_Mode2_Spec10;
                    break;
                case SpecMode22:
                    validSyncBins = SyncBin_Mode2_Spec22;
                    break;
                case SpecMode23:
                    validSyncBins = SyncBin_Mode2_Spec23;
                    break;
                default:
                case SpecMode9:
                    validSyncBins = SyncBin_Mode2_Spec9;
                    break;
            }
            break;
        case TransMode3:
            T_Bcp = SYNC_BCPLength_Mode3;
            T_b   = SYNC_NbLength_Mode3;
            L     = SYNC_L_Mode3;
            m     = SYNC_m_Mode3;
            N_zc  = SYNC_Nzc_Mode3;
            switch (specMode)
            {
                case SpecMode1:
                    validSyncBins = SyncBin_Mode3_Spec1;
                    break;
                case SpecMode2:
                    validSyncBins = SyncBin_Mode3_Spec2;
                    break;
                case SpecMode10:
                    validSyncBins = SyncBin_Mode3_Spec10;
                    break;
                case SpecMode22:
                    validSyncBins = SyncBin_Mode3_Spec22;
                    break;
                case SpecMode23:
                    validSyncBins = SyncBin_Mode3_Spec23;
                    break;
                default:
                case SpecMode9:
                    validSyncBins = SyncBin_Mode3_Spec9;
                    break;
            }
            break;
        case TransMode1:    
        default:
            T_Bcp = SYNC_BCPLength_Mode1;
            T_b   = SYNC_NbLength_Mode1;
            L     = SYNC_L_Mode1;
            m     = SYNC_m_Mode1;
            N_zc  = SYNC_Nzc_Mode1;
            switch (specMode)
            {
                case SpecMode1:
                    validSyncBins = SyncBin_Mode1_Spec1;
                    break;
                case SpecMode2:
                    validSyncBins = SyncBin_Mode1_Spec2;
                    break;
                case SpecMode10:
                    validSyncBins = SyncBin_Mode1_Spec10;
                    break;
                case SpecMode22:
                    validSyncBins = SyncBin_Mode1_Spec22;
                    break;
                case SpecMode23:
                    validSyncBins = SyncBin_Mode1_Spec23;
                    break;
                default:
                case SpecMode9:
                    validSyncBins = SyncBin_Mode1_Spec9;
                    break;
            }
            break;
    }
    
    handle->N_I_x2 = getNumOfHalfBand (specMode);
    handle->T_Bcp  = T_Bcp;
    handle->T_b    = T_b;
    handle->T_B    = T_b + T_b + T_Bcp;
    handle->T_Sync = T_b + T_b - T_Bcp;

    handle->L      = L;
    handle->m      = m;
    handle->N_zc   = N_zc;
    handle->validSyncBins = validSyncBins;

    genSyncSymbol (handle);
}

static void genFreqDomain (syncConst* handle)
{
    handle->zeroPadding       = handle->T_Sync - 1;
    handle->dataLength        = SubFrameLength;
    handle->fftSize           = handle->dataLength + handle->zeroPadding;
    handle->sync_buffer       = (cdr_complex*) malloc (sizeof(cdr_complex) * handle->fftSize);
    handle->data_buffer       = (cdr_complex*) malloc (sizeof(cdr_complex) * handle->fftSize);
    handle->zeroPaddingBuffer = handle->data_buffer + handle->dataLength;

    handle->fft_handle  = cdr_FFTW_Init (handle ->fftSize, 1);

    ArrraySetZero_complex(handle->sync_buffer, handle ->fftSize);
    for (int i = 0; i < handle->T_Sync; i++)
    {
        handle ->sync_buffer[i] = conjf (handle->Sync_Symbols[handle->T_Sync -1 - i]);
    }

    cdr_FFTW_Proccess (handle->fft_handle, handle->sync_buffer, handle->sync_buffer);

    return;
}


syncConst* SyncConst_Init (enum TransmissionMode transMode, enum SpectrumType specMode)
{
    syncConst *handle = (syncConst*) malloc (sizeof(syncConst));

    configSyncConst (handle, transMode, specMode);

    genFreqDomain (handle);

    return handle;
}

void SyncConst_Release(syncConst* handle)
{
    cdr_FFTW_Release (handle->fft_handle);

    free (handle->sync_buffer);
    free (handle->data_buffer);

    free (handle->Sync_Symbols);
    free (handle);
}

void SyncConst_Reset (syncConst* handle, enum TransmissionMode transMode, enum SpectrumType specMode)
{
    free (handle->Sync_Symbols);
    configSyncConst (handle, transMode, specMode);
}

void SyncConst_ProcessCorrelation (syncConst* handle, cdr_complex *inputData, float *peak, int *peakIndex)
{
    ArrayCopy_complex    (inputData, handle->data_buffer, handle->dataLength);
    ArrraySetZero_complex(handle->zeroPaddingBuffer, handle->zeroPadding);

    cdr_FFTW_Proccess (handle->fft_handle, handle->data_buffer, handle->data_buffer);

    ArrrayMulti_complex(handle->sync_buffer, handle->data_buffer, handle->data_buffer, handle->fftSize);

    ArrrayConjunction_complex (handle->data_buffer, handle->fftSize);
    cdr_FFTW_Proccess (handle->fft_handle, handle->data_buffer, handle->data_buffer);

    ArrrayAmplitudePeak_complex (handle->data_buffer, handle->fftSize, peak, peakIndex);
    *peakIndex -= handle->T_Sync - 1;
}

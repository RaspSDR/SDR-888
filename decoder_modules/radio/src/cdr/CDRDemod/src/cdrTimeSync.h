#ifndef CDR_TIMESYNC_H 
#define CDR_TIMESYNC_H

#include <cdrDemod.h>
#include "cdrFFTW.h"

typedef struct syncConst
{
    int T_Bcp;                
    int T_b;                  
    int T_B;                          
    int T_Sync;                    
    int N_I_x2;                
    
    cdr_complex *Sync_Symbols;     

    int L;
    int m;
    int N_zc;
    const int (*validSyncBins)[2];            

    cdr_complex *sync_buffer;
    cdr_complex *data_buffer;
    int fftSize;
    int dataLength;
    int zeroPadding;   
    cdr_complex *zeroPaddingBuffer;
    cdrFFTW *fft_handle;

}syncConst;

syncConst* SyncConst_Init (enum TransmissionMode transMode, enum SpectrumType specMode);

void SyncConst_Release(syncConst* handle);

void SyncConst_Reset (syncConst* handle, enum TransmissionMode transMode, enum SpectrumType specMode);

void SyncConst_ProcessCorrelation (syncConst* handle, cdr_complex *inputData, float *peak, int *peakIndex);

#endif
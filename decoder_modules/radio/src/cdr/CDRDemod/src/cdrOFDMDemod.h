#ifndef CDR_OFDM_H 
#define CDR_OFDM_H

#include <cdrDemod.h>
#include "cdrDiscretePilot.h"
#include "cdrTimeSync.h"
#include "cdrOfdmSymbol.h"


#define PROCESS_BUFFER_SIZE (SubFrameLength * 4)
#define HALF_BUFFER_SIZE    (SubFrameLength)

typedef struct ofdmInfo
{
    int             reset;                

    double          timeOffset;             
    float           freqOffset;             
    float           phaseOffset;          

    cdr_complex      *discretePilot;
    float            *pilotPhase;
    cdr_complexFrame *rawData;
    cdr_complexArray *infoData;

    syncConst        *pSyncConst;
    ofdmConst        *pOfdmConst;
    carrierConst     *pCarrierConst;
}ofdmInfo;

typedef struct ofdmDemodHandle
{
    enum TransmissionMode TransMode;
    enum SpectrumType SpectrumMode;

    cdr_complexArray* processBuffer;

    ofdmInfo* demodInfo;

} ofdmDemodHandle;

int getNumOfHalfBand (enum SpectrumType specMode);

ofdmDemodHandle* ofdmDemod_Init(void);

void ofdmDemod_Release(ofdmDemodHandle* handle);

int ofdmDemod_Process(ofdmDemodHandle* handle, cdr_complex* input, int inputLength);

int ofdmDemod_GetBufferSize(ofdmDemodHandle* handle);

void ofdmDemod_Reset(ofdmDemodHandle* handle);

void ofdmDemod_SetTransferMode(ofdmDemodHandle* handle, enum TransmissionMode transferMode);

void ofdmDemod_SetSpectrumMode(ofdmDemodHandle* handle, enum SpectrumType spectrumMode);

cdr_complexArray* ofdmDemod_GetSysInfoIQ(ofdmDemodHandle* handle);

cdr_complexFrame* ofdmDemod_GetDataIQ(ofdmDemodHandle* handle);

double ofdmDemod_GetFrequencyOffset(ofdmDemodHandle* handle);

double ofdmDemod_GetSampleRateOffset(ofdmDemodHandle* handle);

#endif
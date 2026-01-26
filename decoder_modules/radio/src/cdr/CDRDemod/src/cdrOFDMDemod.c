#include "cdrOFDMDemod.h"

#include "cdrOfdmConst.h"
#include "cdrUtility.h"
#include "cdrFFTW.h"
#include "cdrPilot.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define OFDM_FDOMAIN_CORRELATION          1
#define OFDM_NO_ERROR         cdr_NO_ERROR
#define OFDM_DATA_NOT_READY   10011 

int getNumOfHalfBand (enum SpectrumType specMode)
{
    int numOfHalfBand;
    switch (specMode)
    {
        case SpecMode1:
            numOfHalfBand = 2;
            break;
        case SpecMode2:
            numOfHalfBand = 4;
            break;
        case SpecMode10:
            numOfHalfBand = 4;
            break;
        case SpecMode22:
            numOfHalfBand = 2;
            break;
        case SpecMode23:
            numOfHalfBand = 4;
            break;
        case SpecMode9:
        default:
            numOfHalfBand = 2;
            break;
    }
    
    return numOfHalfBand;
}

ofdmInfo* ofdmInfoInit (enum TransmissionMode transMode, enum SpectrumType specMode)
{
    int T_u, S_N, N_v_all;
    int numOfPilotPhase;
    int infoLength_half;
    int numOfHalfBands;

    ofdmInfo *demodInfo   = (ofdmInfo*) malloc (sizeof(ofdmInfo));

    demodInfo->pSyncConst    = SyncConst_Init   (transMode, specMode);
    demodInfo->pOfdmConst    = OfdmConst_Init   (transMode, specMode);
    demodInfo->pCarrierConst = CarrierConst_Init(transMode, specMode);

    T_u = demodInfo->pOfdmConst->T_u;
    S_N = demodInfo->pOfdmConst->S_N;
    N_v_all = demodInfo->pCarrierConst->N_v_all;
    
    infoLength_half = demodInfo->pCarrierConst->infoLength_half;
    numOfHalfBands  = demodInfo->pCarrierConst->numOfHalfBands;
    
    numOfPilotPhase = demodInfo->pCarrierConst->pilotGen.halfBandSize;

 
    demodInfo->discretePilot = (cdr_complex*) malloc (sizeof(cdr_complex) * T_u);
    demodInfo->pilotPhase    = (float*)       malloc (sizeof(float) * numOfPilotPhase);
    demodInfo->rawData       = cdr_complexFrame_Init (S_N * N_v_all, S_N);
    demodInfo->infoData      = cdr_complexArray_Init (S_N * infoLength_half * numOfHalfBands);

    demodInfo->reset = 1;
    demodInfo->timeOffset = 0;
    demodInfo->freqOffset = 0;
    demodInfo->phaseOffset = 0;

    return demodInfo;
}

void ofdmInfoRelease (ofdmInfo *demodInfo)
{
    free(demodInfo->discretePilot);
    free(demodInfo->pilotPhase);
    cdr_complexFrame_Release(demodInfo->rawData);
    cdr_complexArray_Release(demodInfo->infoData);

    SyncConst_Release(demodInfo->pSyncConst);
    OfdmConst_Release(demodInfo->pOfdmConst);
    CarrierConst_Release(demodInfo->pCarrierConst);

    free(demodInfo);
    return;
}

cdr_complex* FindStart(ofdmInfo* demodInfo, cdr_complex* input, int *startOffset)
{
    cdr_complex* subFrameStart;

    float max;
    int maxIndex;

#ifdef OFDM_FDOMAIN_CORRELATION
    SyncConst_ProcessCorrelation (demodInfo->pSyncConst, input, &max, &maxIndex);
#else
    CrossCorrelationAmplitudePeak (demodInfo->pSyncConst->Sync_Symbols, demodInfo->pSyncConst->T_Sync,
                                   input, SubFrameLength, 
                                   &max, &maxIndex);
#endif

    *startOffset  = maxIndex - demodInfo->pSyncConst->T_Bcp;
    if (*startOffset < 0)
    {
        *startOffset += SubFrameLength;
    }
    subFrameStart = input + *startOffset;

    demodInfo->reset      = 0;
    demodInfo->timeOffset = 0.0;

    return subFrameStart;
}

cdr_complex* GetSingleFrame(ofdmInfo* demodInfo, cdr_complex* buffer, int *startOffset)
{
    cdr_complex* timeSyncFrame;
    
    int offsetSamples = (int)(round(demodInfo->timeOffset / 2.0) * 2.0);
    demodInfo->timeOffset -= offsetSamples;

    timeSyncFrame = buffer-offsetSamples;
    *startOffset = -offsetSamples; 
    return timeSyncFrame;
}

cdr_complex* CorrectFreqError(ofdmInfo* demodInfo, cdr_complex* timeSyncFrame)
{
    cdr_complex* freqSyncFrame;
    float freqOffset;
    float *phase;
    int t_BCP = demodInfo->pSyncConst->T_Bcp;
    int t_b = demodInfo->pSyncConst->T_b;
    int t_B = demodInfo->pSyncConst->T_B;

    cdr_complex* syncSignal_1 = timeSyncFrame + t_BCP / 2;
    cdr_complex* syncSignal_2 = timeSyncFrame + t_BCP / 2 + t_b;
    cdr_complex sync_error;
    sync_error = ComputeDotConjProduct (syncSignal_2, syncSignal_1, t_b);
    freqOffset = GetComplexPhase(sync_error) / (2.0 * M_PI * t_b) * SampleRate;
    
    demodInfo->freqOffset = freqOffset;
    phase = &demodInfo->phaseOffset;

    freqSyncFrame = timeSyncFrame + t_B;

    *phase += 2.0 * M_PI * (-freqOffset) / SampleRate * t_B;  

    Modulator(freqSyncFrame, (SubFrameLength-t_B), -freqOffset, SampleRate, phase);

    return freqSyncFrame;
}

cdr_complex* SyncFFT(ofdmInfo* demodInfo, cdr_complex* syncSymbol)
{
    int T_u  = demodInfo->pSyncConst->T_b * 2;
    int t_BCP = demodInfo->pSyncConst->T_Bcp;

    cdr_complex* fftData;
    cdrFFTW* fft_Handle = demodInfo->pOfdmConst->fft_handle;

    fftData = syncSymbol + t_BCP / 2;
    cdr_FFTW_ProccessShifted (fft_Handle, fftData, fftData);
    double freqOffset = t_BCP / 2 - demodInfo->timeOffset;
    float phase = 0;
    Modulator(fftData, T_u, freqOffset, T_u, &phase);

    return fftData;
}

cdr_complex* SymbolFFT(ofdmInfo* demodInfo, cdr_complex* ofdmSymbol)
{
    int T_cp = demodInfo->pOfdmConst->T_cp;
    int T_u  = demodInfo->pOfdmConst->T_u;

    cdr_complex* fftData;
    cdrFFTW* fft_Handle = demodInfo->pOfdmConst->fft_handle;

    fftData = ofdmSymbol + T_cp / 2;
    cdr_FFTW_ProccessShifted (fft_Handle, fftData, fftData);
    double freqOffset = T_cp / 2 - demodInfo->timeOffset;
    float phase = 0;
    Modulator(fftData, T_u, freqOffset, T_u, &phase);

    return fftData;
}

void InterpolationTransferH (cdr_complex* transferH, int pilot_index, int lowLimit, int highLimit)
{
    for (int k=1; k<Pilot_TransferH_Distance; k++ )
    {        
        float scale_far  = (float)(k)                            / (float)Pilot_TransferH_Distance;
        float scale_near = (float)(Pilot_TransferH_Distance - k) / (float)Pilot_TransferH_Distance;
        if (pilot_index!=lowLimit)
            transferH[pilot_index - k] = transferH[pilot_index] * scale_near + transferH[pilot_index - Pilot_TransferH_Distance] * scale_far;
        if (pilot_index!=highLimit)
            transferH[pilot_index + k] = transferH[pilot_index] * scale_near + transferH[pilot_index + Pilot_TransferH_Distance] * scale_far;
    }
}

void GetData(ofdmInfo* demodInfo, cdr_complex* ofdmSymbol, int ofdmSymbolIndex)
{
    cdr_complex *discretePilot = demodInfo->discretePilot;
    
    cdr_complex idealPilot;
    PilotInfo *pilotGen = &demodInfo->pCarrierConst->pilotGen;
    
    int half_bands = demodInfo->pCarrierConst->numOfHalfBands;

    const int (*bandPosition)[2] = demodInfo->pCarrierConst->validSubCarrier;
    int band_start;
    int band_stop;

    int pilot_index;
    const int *pilot_Pos_Start = Pilot_Start_Position [ ofdmSymbolIndex % Pilot_InfoSize];
 
    int info_index;
    const int *info_Pos_Inc; 

    int info_output_index    = (ofdmSymbolIndex==0) ? 0 : demodInfo->infoData->validLength;
    cdr_complex *info_output = demodInfo->infoData->handle + info_output_index;
    
    int data_output_index    = (ofdmSymbolIndex==0) ? 0 : demodInfo->rawData->dataHandle->validLength;
    cdr_complex *data_output = demodInfo->rawData->dataHandle->handle + data_output_index;
    demodInfo->rawData->linePosition[ofdmSymbolIndex]->handle = data_output;

    if (ofdmSymbolIndex % Pilot_InfoSize == 0)
        pilot_Reset (pilotGen);

    for (int i = 0; i<half_bands; i++)
    {
        band_start = bandPosition[i][0];
        band_stop  = bandPosition[i][1];  

        pilot_index = band_start + pilot_Pos_Start[i%2];
        
        info_Pos_Inc = demodInfo->pCarrierConst->infoCarrier[i%2];
        info_index   = band_start + info_Pos_Inc[0];
        
        for ( int j=band_start; j<=band_stop; j++ )
        {
            if (j == pilot_index)
            {
                idealPilot = pilot_GetPilot(pilotGen);
                discretePilot[j] = ofdmSymbol[j] / idealPilot; 

                InterpolationTransferH (discretePilot, j, band_start, band_stop);

                pilot_index += Pilot_ofdmSymbol_Distance;
            }
            else if (j == info_index)
            {
                *info_output = ofdmSymbol[j]/ discretePilot[j];          
                info_output++;
                info_output_index++;

                info_Pos_Inc++;
                info_index += *info_Pos_Inc;
            }
            else
            {
                *data_output = ofdmSymbol[j]/ discretePilot[j];
                data_output++;
                data_output_index++;
            }
        }
    }

    demodInfo->infoData->validLength = info_output_index;

    demodInfo->rawData->dataHandle->validLength  = data_output_index;
    demodInfo->rawData->linePosition[ofdmSymbolIndex]->subLength = data_output - demodInfo->rawData->linePosition[ofdmSymbolIndex]->handle;
}

static float LinearFit(const float *y, int n)
{
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f;
    float float_x;
    float m;

    for (int i = 0; i < n; i++) 
    {
        float_x = (float)i;
        sum_x += float_x;
        sum_y += y[i];
        sum_xy += float_x * y[i];
        sum_x2 += float_x * float_x;
    }

    m = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);

    return m;
}

static void PhaseUnwrap(float *phase, int length)
{
    for (int i = 1; i < length; i++) 
    {
        float diff = phase[i] - phase[i - 1];
        if (diff > M_PI) 
        {
            int n = (int)ceil((diff - M_PI) / (2 * M_PI));
            phase[i] -= 2 * M_PI * n;
        }
        else if (diff < -M_PI) 
        {
            int n = (int)ceil((-diff - M_PI) / (2 * M_PI));
            phase[i] += 2 * M_PI * n;
        }
    }
}

static int GetTimeOffset(ofdmInfo* demodInfo)
{
    float slope, slop_avg;
    float currentOffset;
    int T_u = demodInfo->pOfdmConst->T_u;

    cdr_complex* transferH = demodInfo->discretePilot;
    float* pilotPhase = demodInfo->pilotPhase;

    int half_bands = demodInfo->pCarrierConst->numOfHalfBands;
    int pilot_half = demodInfo->pCarrierConst->pilotGen.halfBandSize;

    const int (*bandPosition)[2] = demodInfo->pCarrierConst->validSubCarrier;
    int band_start;
    int band_stop;

    slop_avg = 0.0f;
    for (int i = 0; i<half_bands; i++)
    {
        int phaseOffset = 0;
        
        band_start = bandPosition[i][0];
        band_stop  = bandPosition[i][1]; 
        
        for ( int j=band_start; j<=band_stop; j+=Pilot_TransferH_Distance )
        {
            *(pilotPhase + phaseOffset) = GetComplexPhase (transferH[j]);
            phaseOffset++;
        }

        PhaseUnwrap (pilotPhase, pilot_half);

        slope = LinearFit(pilotPhase, pilot_half);
        
        slop_avg += slope;
    }

    slop_avg /= half_bands;
    slop_avg /= Pilot_TransferH_Distance;

    currentOffset = slop_avg / (2.0*M_PI) * T_u;

    demodInfo->timeOffset += currentOffset;
    return currentOffset;
}

ofdmDemodHandle* ofdmDemod_Init(void)
{
    ofdmDemodHandle* handle = (ofdmDemodHandle*) malloc (sizeof(ofdmDemodHandle));
    
    handle->TransMode     = TransMode1;
    handle->SpectrumMode  = SpecMode9;
    handle->processBuffer = cdr_complexArray_Init(PROCESS_BUFFER_SIZE);
    handle->demodInfo     = ofdmInfoInit(handle->TransMode, handle->SpectrumMode);
    return handle;
}

void ofdmDemod_Release(ofdmDemodHandle* handle)
{
    cdr_complexArray_Release (handle->processBuffer);
    ofdmInfoRelease(handle->demodInfo);
    free (handle);    
    return;
}

void ofdmDemod_SetSpectrumMode(ofdmDemodHandle* handle, enum SpectrumType spectrumMode)
{
    enum TransmissionMode transMode = handle->TransMode;
    ofdmInfoRelease (handle->demodInfo);
    handle->demodInfo =  ofdmInfoInit (transMode, spectrumMode);
}

void ofdmDemod_SetTransferMode(ofdmDemodHandle* handle, enum TransmissionMode transferMode)
{
    enum SpectrumType specMode = handle->SpectrumMode;
    ofdmInfoRelease (handle->demodInfo);
    handle->demodInfo =  ofdmInfoInit (transferMode, specMode);
}

void ofdmDemod_Reset(ofdmDemodHandle* handle)
{
    handle->demodInfo->reset = 1;
    handle->demodInfo->timeOffset = 0.0;
    handle->demodInfo->freqOffset = 0.0;
    handle->demodInfo->phaseOffset= 0.0;
    return;
}

int ofdmDemod_GetBufferSize(ofdmDemodHandle* handle)
{
    return (handle->processBuffer->totalSize - handle->processBuffer->validLength);
}

int ofdmDemod_Process(ofdmDemodHandle* handle, cdr_complex* input, int inputLength)
{
    int error = OFDM_DATA_NOT_READY;

    cdr_complex* processBuffer = handle->processBuffer->handle;
    ofdmInfo* demodInfo = handle->demodInfo;
    int processEndPoint = handle->processBuffer->validLength;
    int processPosition = 0;
    int remainLength;
    
    cdr_complex* timeSyncFrame;       
    cdr_complex* freqSyncFrame;       
    cdr_complex* ofdmTimeFrame;       
    cdr_complex* ofdmFreqFrame;      

    ArrayCopy_complex (input, (processBuffer+processEndPoint), inputLength);
    processEndPoint += inputLength;
    remainLength = processEndPoint;

    if (remainLength > SubFrameLength)
    {
        int startOffset;
        if (demodInfo->reset)
        {
            timeSyncFrame = FindStart(demodInfo, processBuffer, &startOffset);
        }
        else
        {
            timeSyncFrame = GetSingleFrame(demodInfo, processBuffer, &startOffset);
        }

        processPosition += startOffset;
        remainLength    -= startOffset;

        if (remainLength > SubFrameLength)
        {
            freqSyncFrame = CorrectFreqError(demodInfo, timeSyncFrame);
            int offset = 0;
            int T_s = handle->demodInfo->pOfdmConst->T_s;
            int S_N = handle->demodInfo->pOfdmConst->S_N;
            for (int i = 0; i < S_N; i++)
            {
                ofdmTimeFrame = freqSyncFrame + offset;

                ofdmFreqFrame = SymbolFFT(demodInfo, ofdmTimeFrame);
                
                GetData (demodInfo, ofdmFreqFrame, i);

                offset += T_s;
            }
            GetTimeOffset(demodInfo);
            
            processPosition += SubFrameLength;
            remainLength    -= SubFrameLength;; 
            
            error = OFDM_NO_ERROR;
        }                      
    }

    if (processPosition >0 ) 
    {
        ArrayMove_complex (processBuffer+processPosition, processBuffer, remainLength);
    }    
    
    handle->processBuffer->validLength = remainLength;

    return error;
}

cdr_complexArray* ofdmDemod_GetSysInfoIQ(ofdmDemodHandle* handle)
{
    return handle->demodInfo->infoData;
}

cdr_complexFrame* ofdmDemod_GetDataIQ(ofdmDemodHandle* handle)
{
    return handle->demodInfo->rawData;
}

double ofdmDemod_GetFrequencyOffset(ofdmDemodHandle* handle)
{
    return handle->demodInfo->freqOffset;
}

double ofdmDemod_GetSampleRateOffset(ofdmDemodHandle* handle)
{
    return handle->demodInfo->timeOffset / (SubFrameTime * SampleRate); 
}
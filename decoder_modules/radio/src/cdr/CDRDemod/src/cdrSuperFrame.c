#include "cdrSuperFrame.h"

#include "cdrSysInfo.h"
#include "cdrSDIS.h"
#include "cdrMSDS.h"
#include "cdrOfdmConst.h"
#include "cdrUtility.h"

#include <stdlib.h>

#define SuperFrame_NO_ERROR    cdr_NO_ERROR
#define SuperFrame_NOT_READY   10021

#define SuperFrame_SDISSymbols_Mode1  (SDIS_Symbols_Mode1>>2)
#define SuperFrame_SDISSymbols_Mode2  (SDIS_Symbols_Mode2>>2)
#define SuperFrame_SDISSymbols_Mode3  (SDIS_Symbols_Mode3>>2)

#define SuperFrame_MSDSSymbols_Mode1  (MSDS_Symbols_Mode1>>2)
#define SuperFrame_MSDSSymbols_Mode2  (MSDS_Symbols_Mode2>>2)
#define SuperFrame_MSDSSymbols_Mode3  (MSDS_Symbols_Mode3>>2)

static int LastSubFrame(cdrSysInfo* sysInfo)
{
    return (sysInfo->PhyFrameIdx == (SuperFrame_numOfPhyFrame-1)) && (sysInfo->SubFrameIdx == (SuperFrame_numOfSubFrame-1));
}

static int FirstSubFrame(cdrSysInfo* sysInfo)
{
    return (sysInfo->PhyFrameIdx == 0) && (sysInfo->SubFrameIdx == 0);
}

static int GetLogicalFrameIndex(cdrSysInfo* sysInfo, int* logicalFrameIndex, int* logicalSubFrameIndex)
{
    int frameAllocate;
    int subFrameID;

    frameAllocate = sysInfo->FrameAllocate;
    subFrameID = sysInfo->PhyFrameIdx * SuperFrame_numOfSubFrame + sysInfo->SubFrameIdx;
    switch (frameAllocate)
    {
        case 2:
            *logicalFrameIndex    = ((subFrameID & 0b1000)>>2) + (subFrameID & 0b0001);
            *logicalSubFrameIndex = (subFrameID >> 1) & 0b11;
            break;
        case 3:
            *logicalFrameIndex    = (subFrameID & 0b11);
            *logicalSubFrameIndex = (subFrameID >> 2) & 0b11;
            break;
        case 1:
        default:
            *logicalFrameIndex    = (subFrameID >> 2) & 0b11;
            *logicalSubFrameIndex = (subFrameID & 0b11);
            break;
    }

    return subFrameID;
}

static void GetSDISforEachSubFrame (superFrameHandle* handle, cdr_complexFrame* frameData, int logicalFrameIndex, int logicalSubFrameIndex)
{
    cdr_complex *source;
    int length;

    cdr_complex *destination;
    int offset, lineIndex;

    int N_I = handle->N_I;
    int offset_SubBand = handle->sdis_symbolPerSubBand;
    int lineLoop = handle->sdis_linesPerSubFrame * SuperFrame_numOfSubFrame;

    cdr_complexFrame *sdisLogicFrame = handle->allSDIS [logicalFrameIndex];

    lineIndex   =  logicalSubFrameIndex * handle->sdis_linesPerSubFrame;
    offset      = (logicalSubFrameIndex == 0) ? 0 : ((sdisLogicFrame->dataHandle->validLength)/N_I);
    destination = sdisLogicFrame->dataHandle->handle + offset;

    for (int i=0; i<handle->sdis_linesPerSubFrame; i++)
    {
        source = frameData->linePosition[i]->handle;
        length =  (i!=(handle->sdis_linesPerSubFrame-1)) 
                ? (frameData->linePosition[i]->subLength / N_I)
                : handle->sdis_lastLineValid;
        for (int j=0; j<N_I; j++)
        {
            ArrayCopy_complex (source, destination+j*offset_SubBand, length);
            source+=length;

            sdisLogicFrame->linePosition[lineIndex+j*lineLoop]->handle    = destination+j*offset_SubBand;
            sdisLogicFrame->linePosition[lineIndex+j*lineLoop]->subLength = length;
        }

        destination += length;
        offset      += length;
        lineIndex++;
    }

    sdisLogicFrame->dataHandle->validLength = offset * N_I;
}

static void GetMSDSforEachSubFrame (superFrameHandle* handle, cdr_complexFrame* frameData, int logicalFrameIndex, int logicalSubFrameIndex)
{
    cdr_complex *source;
    int length, subband_step;

    cdr_complex *destination;
    int offset, lineIndex;
    int frameLineIndex;

    int N_I = handle->N_I;
    int offset_SubBand = handle->msds_symbolPerSubBand;
    int lineLoop = handle->msds_linesPerSubFrame * SuperFrame_numOfSubFrame;
    int lineOffset = handle->msds_lineStart;

    cdr_complexFrame *msdsLogicFrame = handle->allMSDS [logicalFrameIndex];
    
    lineIndex   =  logicalSubFrameIndex * handle->msds_linesPerSubFrame;
    offset      = (logicalSubFrameIndex == 0) ? 0 : ((msdsLogicFrame->dataHandle->validLength)/N_I);
    destination = msdsLogicFrame->dataHandle->handle + offset;

    for (int i=0; i<handle->msds_linesPerSubFrame; i++)
    {
        frameLineIndex = lineOffset + i;
        subband_step = frameData->linePosition[frameLineIndex]->subLength / N_I; 
        if (i==0)
        {
            source = frameData->linePosition[frameLineIndex]->handle + handle->msds_N_Offset;
            length = subband_step - handle->msds_N_Offset;
        }
        else
        {
            source = frameData->linePosition[frameLineIndex]->handle;
            length = subband_step;
        }

        for (int j=0; j<N_I; j++)
        {
            ArrayCopy_complex (source, destination+j*offset_SubBand, length);
            source+=subband_step;

            msdsLogicFrame->linePosition[lineIndex+j*lineLoop]->handle    = destination+j*offset_SubBand;
            msdsLogicFrame->linePosition[lineIndex+j*lineLoop]->subLength = length;
        }

        destination += length;
        offset      += length;
        lineIndex++;
    }

    msdsLogicFrame->dataHandle->validLength = offset * N_I;
}

static void SuperFrame_ResizeBuffer(superFrameHandle* handle, cdrSysInfo* sysInfo)
{
    int band_scale, S_N;

    int sdis_symbols, sdis_N, sdis_N_Valid;
    int sdis_FrameLength, sdis_FrameLines;

    int msds_symbols;
    int msds_FrameLength, msds_FrameLines;

    switch (sysInfo->TransMode)
    {        
        case TransMode2:
            sdis_symbols = SDIS_Symbols_Mode2;
            sdis_N       = SDIS_N_Mode2;
            sdis_N_Valid = SDIS_N_Valid_Mode2;
            S_N          = OFDM_SN_Mode2;
            msds_symbols = MSDS_Symbols_Mode2;
            break;
        case TransMode3:
            sdis_symbols = SDIS_Symbols_Mode3;
            sdis_N       = SDIS_N_Mode3;
            sdis_N_Valid = SDIS_N_Valid_Mode3;
            S_N          = OFDM_SN_Mode3;
            msds_symbols = MSDS_Symbols_Mode3;
            break;
        case TransMode1: 
        default:
            sdis_symbols = SDIS_Symbols_Mode1;
            sdis_N       = SDIS_N_Mode1;
            sdis_N_Valid = SDIS_N_Valid_Mode1;
            S_N          = OFDM_SN_Mode1;
            msds_symbols = MSDS_Symbols_Mode1;

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
    handle->N_I  = band_scale;   
    
    handle->sdis_linesPerSubFrame = sdis_N+1;
    handle->sdis_lastLineValid    = sdis_N_Valid;
    handle->sdis_symbolPerSubBand = sdis_symbols;
    sdis_FrameLength = sdis_symbols * band_scale;
    sdis_FrameLines  = (sdis_N+1)   * band_scale * SuperFrame_numOfSubFrame;

    handle->msds_symbolPerSubBand = msds_symbols;
    handle->msds_lineStart        = sdis_N;
    handle->msds_linesPerSubFrame = S_N-sdis_N;
    handle->msds_N_Offset         = sdis_N_Valid;
    msds_FrameLength  = msds_symbols * band_scale;
    msds_FrameLines   = (S_N-sdis_N) * band_scale * SuperFrame_numOfSubFrame;

    if (   handle->allSDIS[0]->dataHandle->totalSize < sdis_FrameLength 
        || handle->allSDIS[0]->numOfLines            < sdis_FrameLines
        || handle->allMSDS[0]->dataHandle->totalSize < msds_FrameLength 
        || handle->allMSDS[0]->numOfLines            < msds_FrameLines )
    {
        for (int i=0;i<SuperFrame_numOfLogicalFrame;i++)
        {
            cdr_complexFrame_Release (handle->allSDIS[i]);
            cdr_complexFrame_Release (handle->allMSDS[i]);

            handle->allSDIS[i] = cdr_complexFrame_Init (sdis_FrameLength, sdis_FrameLines);
            handle->allMSDS[i] = cdr_complexFrame_Init (msds_FrameLength, msds_FrameLines);
        }

        handle->nextSubFrameIndex = 0;
    }

    return;
}

superFrameHandle* SuperFrame_Init(void)
{
    superFrameHandle* handle = (superFrameHandle*) malloc (sizeof(superFrameHandle));

    handle->N_I = 1;
    handle->nextSubFrameIndex = 0;

    for (int i=0; i<SuperFrame_numOfLogicalFrame; i++)
    {   
        handle->allSDIS[i] = cdr_complexFrame_Init (0, 0);
        handle->allMSDS[i] = cdr_complexFrame_Init (0, 0);
    }

    return handle;
}

void SuperFrame_Release(superFrameHandle* handle)
{
    for (int i=0;i<SuperFrame_numOfLogicalFrame;i++)
    {   
        cdr_complexFrame_Release (handle->allSDIS[i]);
        cdr_complexFrame_Release (handle->allMSDS[i]);
    }

    free (handle);

    return;
}

int SuperFrame_Process(superFrameHandle* handle, cdr_complexFrame* frameData, cdrSysInfo* sysInfo)
{
    int logicalFrameIndex, logicalSubFrameIndex;
    int current_subFrameIndex;
    int error = SuperFrame_NOT_READY;

    current_subFrameIndex = GetLogicalFrameIndex(sysInfo, &logicalFrameIndex, &logicalSubFrameIndex);
    
    SuperFrame_ResizeBuffer (handle, sysInfo);
    
    if (current_subFrameIndex == handle->nextSubFrameIndex)
    {
        GetSDISforEachSubFrame(handle, frameData, logicalFrameIndex, logicalSubFrameIndex);

        GetMSDSforEachSubFrame(handle, frameData, logicalFrameIndex, logicalSubFrameIndex);

        handle->nextSubFrameIndex++;
        handle->nextSubFrameIndex %= (SuperFrame_numOfPhyFrame * SuperFrame_numOfSubFrame);

        if (handle->nextSubFrameIndex == 0) 
            error = SuperFrame_NO_ERROR;
    }
    
    return error;
}

cdr_complexFrame* SuperFrame_GetSDIS (superFrameHandle* handle, int frameIndex)
{
    return handle->allSDIS[frameIndex];
}

cdr_complexFrame* SuperFrame_GetMSDS (superFrameHandle* handle, int frameIndex)
{
    return handle->allMSDS[frameIndex];
}

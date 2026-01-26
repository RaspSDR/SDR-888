#ifndef CDR_SUPERFRAME_H
#define CDR_SUPERFRAME_H

#include <cdrDemod.h>
#include "cdrSysInfo.h"

#define SuperFrame_numOfPhyFrame 4

#define SuperFrame_numOfLogicalFrame 4

#define SuperFrame_numOfSubFrame 4

typedef struct  superFrameHandle
{
    int N_I;

    cdr_complexFrame* allSDIS[SuperFrame_numOfLogicalFrame];    
    int sdis_lastLineValid;
    int sdis_symbolPerSubBand;
    int sdis_linesPerSubFrame;

    cdr_complexFrame* allMSDS[SuperFrame_numOfLogicalFrame];
    int msds_symbolPerSubBand;
    int msds_linesPerSubFrame;
    int msds_lineStart;
    int msds_N_Offset;

    int nextSubFrameIndex;
}superFrameHandle;

superFrameHandle* SuperFrame_Init(void);

void SuperFrame_Release(superFrameHandle* handle);

int SuperFrame_Process(superFrameHandle* handle, cdr_complexFrame* frameData, cdrSysInfo* sysInfo);

cdr_complexFrame* SuperFrame_GetSDIS (superFrameHandle* handle, int frameIndex);

cdr_complexFrame* SuperFrame_GetMSDS (superFrameHandle* handle, int frameIndex);

#endif
#ifndef CDR_PILOT_H
#define CDR_PILOT_H

#include <cdrDemod.h>

typedef struct PilotInfo 
{
    int state;
    int currentIndex;
    int totalSize;
    int halfBandSize;
} PilotInfo;

void pilot_Reset (PilotInfo* handle);

cdr_complex pilot_GetPilot (PilotInfo* handle);

#endif
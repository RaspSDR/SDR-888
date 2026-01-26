#include "cdrPilot.h"

#include "cdrModulation.h"

#include <stdlib.h>

#define PILOT_LFSR_WIDTH 11

#define PILOT_TAP_11 0       
#define PILOT_TAP_9  2       
#define PILOT_TAP_FEEDBAK (PILOT_LFSR_WIDTH-1)

#define PILOT_SEED 0b10100101010

#define PILOT_TAP_I 0
#define PILOT_TAP_Q 8

static int GenerateLFSR(int *state)
{
    int feedback = 0;

    feedback = ((*state >> PILOT_TAP_11) & 1) ^ ((*state >> PILOT_TAP_9) & 1);

    *state = (*state >> 1) | (feedback << PILOT_TAP_FEEDBAK);

    return *state;
}

void pilot_Reset (PilotInfo* handle)
{
    handle->state = PILOT_SEED;
    handle->currentIndex = 0;
    return;
}

cdr_complex pilot_GetPilot (PilotInfo* handle)
{
    int i_data = ((handle->state >> PILOT_TAP_I) & 1);
    int q_data = ((handle->state >> PILOT_TAP_Q) & 1);
    int iq  = (q_data << 1) + i_data;
	
    GenerateLFSR (&handle->state);
    handle->currentIndex ++ ;

    return mod_pilot[iq];
}
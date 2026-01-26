#ifndef CDR_DISCRETEPILOT_H 
#define CDR_DISCRETEPILOT_H

#include <cdrDemod.h>
#include "cdrPilot.h"

typedef struct carrierConst
{
    int N_v;                                    
    int N_v_all;                              
    int N_v_half;                            
    int numOfHalfBands;                    

    const int (*validSubCarrier)[2];            

    int infoLength_half;                     
    const int (*infoCarrier)[5];             

    PilotInfo  pilotGen;                    

}carrierConst;

carrierConst* CarrierConst_Init (enum TransmissionMode transMode, enum SpectrumType specMode);

void CarrierConst_Release(carrierConst* handle);

void CarrierConst_Reset (carrierConst* handle, enum TransmissionMode transMode, enum SpectrumType specMode);

#endif
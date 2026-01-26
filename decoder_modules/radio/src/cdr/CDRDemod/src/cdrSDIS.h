#ifndef CDR_DECODE_SDIS_H
#define CDR_DECODE_SDIS_H

#include <cdrDemod.h>
#include "cdrSysInfo.h"
#include "cdrModulation.h"
#include "cdrInterleave.h"
#include "cdrViterbiCodec.h"

#define SDIS_Symbols_Mode1  1704
#define SDIS_Symbols_Mode2  1576
#define SDIS_Symbols_Mode3  1360

#define SDIS_N_Mode1  2
#define SDIS_N_Mode2  3
#define SDIS_N_Mode3  1

#define SDIS_N_Valid_Mode1  0
#define SDIS_N_Valid_Mode2  72
#define SDIS_N_Valid_Mode3  128

typedef struct sdisHandle
{
    ConstellationInfo* constellation; 
    int N_I;
    cdr_byteArray* demodBits;                
    cdr_byteArray* sdisBytes;               

    DeInterleaveTable *deInterleaveTable;    
    DeConvolutionPath* deConvolutionPath;    
    
}sdisHandle;

sdisHandle* DecodeSDIS_Init(void);

void DecodeSDIS_Release(sdisHandle* handle);

int DecodeSDIS_Process(sdisHandle* sdisHandle, cdr_complexFrame* SDISSymbols, cdrSysInfo* sysInfo);

cdr_byteArray* DecodeSDIS_GetBytes (sdisHandle* handle);

#endif
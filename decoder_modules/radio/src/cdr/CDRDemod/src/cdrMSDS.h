#ifndef CDR_DECODE_MSDS_H
#define CDR_DECODE_MSDS_H

#include <cdrDemod.h>
#include "cdrSysInfo.h"
#include "cdrModulation.h"
#include "cdrInterleave.h"

#define MSDS_Symbols_Mode1  46080
#define MSDS_Symbols_Mode2  46080
#define MSDS_Symbols_Mode3  50688

typedef struct msdsHandle
{
    int N_I;
    ConstellationInfo* constellation; 
    cdr_byteArray* deModBits;                     
    cdr_byteArray* msdsBytes;                    

    DeInterleaveTable *deInterleaveTable;     
}msdsHandle;

msdsHandle* DecodeMSDS_Init(void);

void DecodeMSDS_Release(msdsHandle* handle);

int DecodeMSDS_Process(msdsHandle* handle, cdr_complexFrame* msdsSymbols, cdrSysInfo* sysInfo);

cdr_byteArray* MSDS_GetDecodeBytes(msdsHandle* handle);

#endif
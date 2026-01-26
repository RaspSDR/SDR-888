#ifndef CDR_SYSINFO_H
#define CDR_SYSINFO_H

#include <cdrDemod.h>
#include "cdrInterleave.h"
#include "cdrViterbiCodec.h"

typedef struct cdrSysInfo
{
    enum TransmissionMode TransMode;          
    int FixFrequency;                         
    double NextFreq;                          
    enum SubBandFreq FreqOffset;              
    enum SpectrumType SpectrumMode;           
    int PhyFrameIdx;                          
    int SubFrameIdx;                          
    int FrameAllocate;                        
    enum QamType SDISModType;                 
    enum QamType MSDSModType;                 
    enum HierarchicalMod ModeAlpha;           
    int OneLDPCRate;                          
    enum LDPCRate LDPCRate1;                  
    enum LDPCRate LDPCRate2;                  
}cdrSysInfo;

typedef struct cdrSysInfoHandle
{
    cdrSysInfo        *sysInfo;
    cdr_complexArray  *infoSymbols;           
    cdr_byteArray     *demodBits;                
    DeInterleaveTable *deInterleaveTable;    
    DeConvolutionPath* deConvolutionPath;    
}cdrSysInfoHandle;

cdrSysInfoHandle* SysInfo_Init(void);

void SysInfo_Release(cdrSysInfoHandle* sysInfoHandle);

int SysInfo_Process(cdrSysInfoHandle* sysInfoHandle, cdr_complexArray* infoSubFrame, cdrSysInfo* sysInfo);

cdrSysInfo* SysInfo_GetSysInfo(cdrSysInfoHandle* sysInfoHandle);

#endif
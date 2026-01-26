#include "cdrSysInfo.h"

#include "cdrUtility.h"
#include "cdrModulation.h"
#include "cdrInterleave.h"
#include "cdrViterbiCodec.h"

#include <stdlib.h>

#define SysInfo_NO_ERROR  cdr_NO_ERROR
#define SysInfo_CRC_ERROR     -1021
#define SysInfo_VITERBI_FAIL  -1022

#define SysInfo_BitsDecode    48
#define SysInfo_BitsCRC       6

#define SysInfo_CRC_FUNC   0b111101
#define SysInfo_CRC_STATE  0b111111

#define SysInfo_NumOfSymbols 108

#define SysInfo_SymbolsPerOFDM_Mode1 4
#define SysInfo_SymbolsPerOFDM_Mode2 2
#define SysInfo_SymbolsPerOFDM_Mode3 4

#define SysInfo_RepeatPerBand       2
#define SysInfo_RepeatPerSubFrame   2

#define SysInfo_OFDMs_Mode1 27
#define SysInfo_OFDMs_Mode2 54
#define SysInfo_OFDMs_Mode3 27

const ConstellationInfo SysInfo_Constellation = {mod_pilot, 2};

#define SysInfo_BitsDemod  216

static void GetSymbol(cdr_complexArray* infoSubFrame, cdr_complexArray* infoSymbols, cdrSysInfo* sysInfo)
{
    int repeat;
    double repeat_scale;
    int offset_i, offset_j;

    int scaleA;                   
    int scaleB;                
    int repeat_PerOFDM;       
    int scaleC;                
    int scaleD;                   
 
    int band_scale;
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
    
    switch (sysInfo->TransMode)
    {        
        case TransMode2:
            scaleA = SysInfo_SymbolsPerOFDM_Mode2;
            scaleC = SysInfo_OFDMs_Mode2;
            break;
        case TransMode3:
            scaleA = SysInfo_SymbolsPerOFDM_Mode3;
            scaleC = SysInfo_OFDMs_Mode3;
            break;
        case TransMode1: 
        default:
            scaleA = SysInfo_SymbolsPerOFDM_Mode1;
            scaleC = SysInfo_OFDMs_Mode1;
            break;
    }
        
    repeat = SysInfo_RepeatPerBand * SysInfo_RepeatPerSubFrame * band_scale;
    repeat_PerOFDM = SysInfo_RepeatPerBand * band_scale;
    scaleB = scaleA * repeat_PerOFDM;
    scaleD = scaleC * scaleB;
    repeat_scale = 1.0f / repeat;

    for (int i = 0; i < SysInfo_NumOfSymbols; i++)
    {
        infoSymbols->handle[i] = cdr_complex_Zero;

        offset_i = i % scaleA + i / scaleA * scaleB;
        for (int j = 0; j < repeat; j++)
        {
            offset_j = j % repeat_PerOFDM * scaleA + j / repeat_PerOFDM * scaleD;
            infoSymbols->handle[i] += infoSubFrame->handle[offset_i + offset_j];
        }

        infoSymbols->handle[i] *= repeat_scale;
    }

    infoSymbols->validLength = SysInfo_NumOfSymbols;
}

static int InfoCRC(cdr_byteArray* infoBits)
{            
    int crcState = SysInfo_CRC_STATE;
    int temp;
    for (int i = 0; i < SysInfo_BitsDecode; i++)
    {
        temp = (crcState ^ infoBits->handle[i]) & 1;
        crcState = crcState >> 1;
        if (temp)
            crcState = crcState ^ SysInfo_CRC_FUNC;
    }
    return (crcState == 0) ? SysInfo_NO_ERROR : SysInfo_CRC_ERROR;
}

static void DecodeInfo(cdr_byteArray* infoBits, cdrSysInfo* info)
{
    info->FixFrequency  = BitArrayToInt(infoBits,  0, 1);
    info->NextFreq      = BitArrayToInt(infoBits,  1, 9) * 0.1 + 87;
    info->FreqOffset    = BitArrayToInt(infoBits, 10, 3);
    info->SpectrumMode  = BitArrayToInt(infoBits, 13, 6);
    info->PhyFrameIdx   = BitArrayToInt(infoBits, 19, 2);
    info->SubFrameIdx   = BitArrayToInt(infoBits, 21, 2);
    info->FrameAllocate = BitArrayToInt(infoBits, 23, 2);
    info->SDISModType   = BitArrayToInt(infoBits, 25, 2);
    info->MSDSModType   = BitArrayToInt(infoBits, 27, 2);
    info->ModeAlpha     = BitArrayToInt(infoBits, 29, 2);
    info->OneLDPCRate   = BitArrayToInt(infoBits, 31, 1);
    info->LDPCRate1     = BitArrayToInt(infoBits, 32, 2);
    info->LDPCRate2     = BitArrayToInt(infoBits, 34, 2);
}


static void SysInfo_Reset (cdrSysInfoHandle* sysInfoHandle)
{
    sysInfoHandle->sysInfo->TransMode      = TransMode1;          
    sysInfoHandle->sysInfo->FixFrequency      = 0;                   
    sysInfoHandle->sysInfo->NextFreq       = 87.0;                
    sysInfoHandle->sysInfo->FreqOffset     = SubF150k;            
    sysInfoHandle->sysInfo->SpectrumMode   = SpecMode9;           
    sysInfoHandle->sysInfo->PhyFrameIdx    = 0;                   
    sysInfoHandle->sysInfo->SubFrameIdx    = 0;                   
    sysInfoHandle->sysInfo->FrameAllocate  = 0;                   
    sysInfoHandle->sysInfo->SDISModType    = QPSK;                
    sysInfoHandle->sysInfo->MSDSModType    = QPSK;                
    sysInfoHandle->sysInfo->ModeAlpha      = HierarchyNone;       
    sysInfoHandle->sysInfo->OneLDPCRate      = 0;                   
    sysInfoHandle->sysInfo->LDPCRate1      = Rate3_4;             
    sysInfoHandle->sysInfo->LDPCRate2      = Rate3_4;             
}

cdrSysInfoHandle* SysInfo_Init(void)
{
    cdrSysInfoHandle* sysInfoHandle    = (cdrSysInfoHandle*) malloc (sizeof (cdrSysInfoHandle));
    
    sysInfoHandle->infoSymbols       = cdr_complexArray_Init (SysInfo_NumOfSymbols);
    sysInfoHandle->demodBits         = cdr_byteArray_Init    (SysInfo_BitsDemod);
    sysInfoHandle->sysInfo           = (cdrSysInfo*) malloc  (sizeof (cdrSysInfo));  
    sysInfoHandle->deInterleaveTable = GenerateDeInterleaveTable (SysInfo_BitsDemod);
    sysInfoHandle->deConvolutionPath = GenerateDeConvolutionPath (SysInfo_BitsDemod);
    SysInfo_Reset (sysInfoHandle);

    return sysInfoHandle;
}

void SysInfo_Release(cdrSysInfoHandle* sysInfoHandle)
{
    cdr_complexArray_Release (sysInfoHandle->infoSymbols);
    cdr_byteArray_Release    (sysInfoHandle->demodBits);
    ReleaseDeInterleaveTable (sysInfoHandle->deInterleaveTable);
    ReleaseDeConvolutionPath (sysInfoHandle->deConvolutionPath);

    free (sysInfoHandle->sysInfo);

    free (sysInfoHandle);

    return;
}

int SysInfo_Process(cdrSysInfoHandle* sysInfoHandle, cdr_complexArray* infoSubFrame, cdrSysInfo* sysInfo)
{
    int error;
    int lastError = SysInfo_NO_ERROR;
    sysInfo = sysInfoHandle->sysInfo;

    GetSymbol(infoSubFrame, sysInfoHandle->infoSymbols, sysInfo);

    DeConstellation(sysInfoHandle->infoSymbols, sysInfoHandle->demodBits, &SysInfo_Constellation);
    
    DeInterleaveByTable_byte(sysInfoHandle->demodBits, sysInfoHandle->deInterleaveTable);

    error = DecodeConvolution(sysInfoHandle->demodBits, sysInfoHandle->deConvolutionPath);
    lastError = ErrorHandling (error, SysInfo_VITERBI_FAIL, lastError);
    if (error < SysInfo_NO_ERROR) 
    {
        ;
    }

    error = InfoCRC(sysInfoHandle->demodBits);
    lastError = ErrorHandling (error, SysInfo_CRC_ERROR, lastError);
    if (error < SysInfo_NO_ERROR) 
    {
        return error;
    }

    DecodeInfo(sysInfoHandle->demodBits, sysInfoHandle->sysInfo);    
    
    return lastError;
}

cdrSysInfo* SysInfo_GetSysInfo(cdrSysInfoHandle* sysInfoHandle)
{
    return sysInfoHandle->sysInfo;
}

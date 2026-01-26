#include <cdrDemod.h>

#include "cdrIQFilter.h"
#include "cdrOFDMDemod.h"
#include "cdrSysInfo.h"
#include "cdrSuperFrame.h"
#include "cdrSDIS.h"
#include "cdrMSDS.h"
#include "cdrDeMultiplexMSDS.h"
#include "cdrDeMultiplexSDIS.h"
#include "cdrUtility.h"

#include <stdlib.h>
#include <string.h>

#define __IQFILTER__        1
#define __OFDM__            1
#define __SYSINFO__         1
#define __SUPERFRAME__      1
#define __SDIS__            1
#define __MSDS__            1
#define __DEMULTIPLEXMSDS__ 1
#define __DEMULTIPLEXSDIS__ 1

typedef struct cdrDemodCoreHandle
{
    firFilter               *iqFilter;
    ofdmDemodHandle         *ofdmDemod;
    cdrSysInfoHandle        *sysInfo;
    superFrameHandle        *superFrame;
    sdisHandle              *decodeSDIS;
    msdsHandle              *decodeMSDS;
    DeMultiplexMSDSHandle   *deMultiplexMSDS;
    DeMultiplexSDISHandle   *deMultiplexSDIS;

}cdrDemodCoreHandle;

cdrDemodHandle CDRDemodulation_Init(void)
{
    cdrDemodCoreHandle* handle = (cdrDemodCoreHandle*) malloc (sizeof (cdrDemodCoreHandle));

    handle->iqFilter        = IQFilter_Init();
    handle->ofdmDemod       = ofdmDemod_Init();
    handle->sysInfo         = SysInfo_Init();
    handle->superFrame      = SuperFrame_Init();
    handle->decodeSDIS      = DecodeSDIS_Init();
    handle->decodeMSDS      = DecodeMSDS_Init();
    handle->deMultiplexMSDS = DeMultiplexMSDS_Init();
    handle->deMultiplexSDIS = DeMultiplexSDIS_Init();

    return (cdrDemodHandle)handle;
}

void CDRDemodulation_Release(cdrDemodHandle handle)
{
    cdrDemodCoreHandle* demodHandle = (cdrDemodCoreHandle*) handle;

    IQFilter_Release        (demodHandle->iqFilter);
    ofdmDemod_Release       (demodHandle->ofdmDemod);
    SysInfo_Release         (demodHandle->sysInfo);
    SuperFrame_Release      (demodHandle->superFrame);
    DecodeSDIS_Release      (demodHandle->decodeSDIS);
    DecodeMSDS_Release      (demodHandle->decodeMSDS);
    DeMultiplexMSDS_Release (demodHandle->deMultiplexMSDS);
    DeMultiplexSDIS_Release (demodHandle->deMultiplexSDIS);

    free (demodHandle);

    return;
}

void CDRDemodulation_Reset(cdrDemodHandle handle)
{
    cdrDemodCoreHandle* demodHandle = (cdrDemodCoreHandle*) handle;
    ofdmDemod_Reset       (demodHandle->ofdmDemod);
    return;
}

int CDRDemodulation_GetBufferSize(cdrDemodHandle handle)
{   
    cdrDemodCoreHandle* demodHandle = (cdrDemodCoreHandle*) handle;
    return ofdmDemod_GetBufferSize(demodHandle->ofdmDemod);
}

int CDRDemodulation_Process(cdrDemodHandle handle, cdr_complex* iqData, int iqLength)
{
    int error;
    int lastError = cdr_NO_ERROR;

    cdr_complex      *filteredIQ;
    cdr_complexArray *infoIQ;
    cdr_complexFrame *dataIQ;
    cdr_complexFrame *sdisData;
    cdr_complexFrame *msdsData;
    cdr_byteArray    *msdsBytes;
    cdr_byteArray    *sdisBytes;

    cdrSysInfo *sysInfo;

    cdrDemodCoreHandle* demodHandle = (cdrDemodCoreHandle*) handle;

    filteredIQ = iqData;
#if __IQFILTER__
    error = IQFilter_Process(demodHandle->iqFilter, iqData, filteredIQ, iqLength);
    lastError = ErrorHandling (error, error, lastError);
    if (error < cdr_NO_ERROR)
    {
        return error;
    }
#endif

#if __OFDM__
    error = ofdmDemod_Process(demodHandle->ofdmDemod, filteredIQ, iqLength);
    lastError = ErrorHandling (error, error, lastError);
    if (error != cdr_NO_ERROR)
    {
        return error;
    }
#endif

    sysInfo = demodHandle->sysInfo->sysInfo;

    infoIQ = ofdmDemod_GetSysInfoIQ(demodHandle->ofdmDemod);
    dataIQ = ofdmDemod_GetDataIQ(demodHandle->ofdmDemod);
#if __SYSINFO__
    error  = SysInfo_Process(demodHandle->sysInfo, infoIQ, sysInfo);
    lastError = ErrorHandling (error, error, lastError);
    if (error < cdr_NO_ERROR)
    {
        ofdmDemod_Reset(demodHandle->ofdmDemod);
        return error;
    }
#endif

#if __SUPERFRAME__
    error = SuperFrame_Process(demodHandle->superFrame, dataIQ, sysInfo);
    lastError = ErrorHandling (error, error, lastError);
    if (error != cdr_NO_ERROR)
    {        
        return lastError;
    }
#endif

    for (int frameIndex = 0; frameIndex < SuperFrame_numOfLogicalFrame; frameIndex++)
    {
        sdisData = SuperFrame_GetSDIS(demodHandle->superFrame, frameIndex);
#if __SDIS__
        error = DecodeSDIS_Process(demodHandle->decodeSDIS, sdisData, sysInfo);
        lastError = ErrorHandling (error, error, lastError);
        if (error < cdr_NO_ERROR)
        {
            continue;
        }
#endif
        sdisBytes = DecodeSDIS_GetBytes (demodHandle->decodeSDIS);
#if __DEMULTIPLEXSDIS__                    
        error = DeMultiplexSDIS_Process(demodHandle->deMultiplexSDIS, sdisBytes);
        lastError = ErrorHandling (error, error, lastError);
        if (error < cdr_NO_ERROR)
        {
            continue;
        }
#endif
    }

    for (int frameIndex = 0; frameIndex < SuperFrame_numOfLogicalFrame; frameIndex++)
    {
        msdsData = SuperFrame_GetMSDS(demodHandle->superFrame, frameIndex);                    
#if __MSDS__
        error = DecodeMSDS_Process(demodHandle->decodeMSDS, msdsData, sysInfo);
        if (error < cdr_NO_ERROR)
        {
            continue;
        }
#endif

        msdsBytes = MSDS_GetDecodeBytes (demodHandle->decodeMSDS);
#if __DEMULTIPLEXMSDS__
        error = DeMultiplexMSDS_Process(demodHandle->deMultiplexMSDS, msdsBytes, frameIndex);
        if (error < cdr_NO_ERROR)
        {
            continue;
        }
#endif
    }

    return lastError;
}

cdr_byte* CDRDemodulation_GetDraStream (cdrDemodHandle handle, int programeID, int *length)
{
    cdrDemodCoreHandle* demodHandle = (cdrDemodCoreHandle*) handle;
    cdr_byteArray *draStream = DeMultiplexMSDS_GetDraStream (demodHandle->deMultiplexMSDS, programeID);
    *length = draStream->validLength;

    return draStream->handle;
}

int CDRDemodulation_GetNumOfPrograms (cdrDemodHandle handle)
{
    cdrDemodCoreHandle* demodHandle = (cdrDemodCoreHandle*) handle;
    return DeMultiplexMSDS_GetNumOfPrograms (demodHandle->deMultiplexMSDS);
}

enum QamType CDRDemodulation_GetSDISModType (cdrDemodHandle handle)
{
    cdrDemodCoreHandle* demodHandle = (cdrDemodCoreHandle*) handle;
    cdrSysInfo* sysInfo =  SysInfo_GetSysInfo(demodHandle->sysInfo);
    return sysInfo->SDISModType;
}

enum QamType CDRDemodulation_GetMSDSModType (cdrDemodHandle handle)
{
    cdrDemodCoreHandle* demodHandle = (cdrDemodCoreHandle*) handle;
    cdrSysInfo* sysInfo =  SysInfo_GetSysInfo(demodHandle->sysInfo);
    return sysInfo->MSDSModType;
}

enum LDPCRate CDRDemodulation_GetLDPCRate1 (cdrDemodHandle handle)
{
    cdrDemodCoreHandle* demodHandle = (cdrDemodCoreHandle*) handle;
    cdrSysInfo* sysInfo =  SysInfo_GetSysInfo(demodHandle->sysInfo);
    return sysInfo->LDPCRate1;
}

void CDRDemodulation_SetTransferMode(cdrDemodHandle handle, enum TransmissionMode transferMode)
{
    cdrDemodCoreHandle* demodHandle = (cdrDemodCoreHandle*) handle;
    ofdmDemod_SetTransferMode(demodHandle->ofdmDemod, transferMode);
    return;
}

void CDRDemodulation_SetSpectrumMode(cdrDemodHandle handle, enum SpectrumType spectrumMode)
{
    cdrDemodCoreHandle* demodHandle = (cdrDemodCoreHandle*) handle;
    ofdmDemod_SetSpectrumMode(demodHandle->ofdmDemod, spectrumMode);
    IQFilter_SetSpectrumMode(demodHandle->iqFilter, spectrumMode);
    return;
}

double CDRDemodulation_GetFrequencyOffset(cdrDemodHandle handle)
{
    cdrDemodCoreHandle* demodHandle = (cdrDemodCoreHandle*) handle;
    return ofdmDemod_GetFrequencyOffset(demodHandle->ofdmDemod);
}

double CDRDemodulation_GetSampleRateOffset(cdrDemodHandle handle)
{
    cdrDemodCoreHandle* demodHandle = (cdrDemodCoreHandle*) handle;
    return ofdmDemod_GetSampleRateOffset(demodHandle->ofdmDemod);
}

int CDRDemodulation_GetNetID(cdrDemodHandle handle, char* Name, int *nameLength, int *freq)
{
    cdrDemodCoreHandle* demodHandle = (cdrDemodCoreHandle*) handle;
    NetDescription netInfo = demodHandle->deMultiplexSDIS->netInformation->CurrentNet;

    *freq = netInfo.AllFrequney[0];
    *nameLength = netInfo.NameLength;
    memcpy(Name, netInfo.NetName, *nameLength);
    return netInfo.NetID;
}

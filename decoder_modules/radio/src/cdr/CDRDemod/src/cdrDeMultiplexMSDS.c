#include "cdrDeMultiplexMSDS.h"

#include "cdrUtility.h"
#include "cdrSuperFrame.h"

#include <stdlib.h>

#define DeMultiplex_MSDS_NO_ERROR  cdr_NO_ERROR
#define DeMultiplex_MSDS_LESSDATA  -1051
#define DeMultiplex_MSDS_SMF_FAIL  -1052
#define DeMultiplex_MSDS_SERVICE_HEAD_FAIL  -1053
#define DeMultiplex_MSDS_AUDIO_HEAD_FAIL    -1054
#define DeMultiplex_MSDS_DATA_HEAD_FAIL     -1055
#define DeMultiplex_MSDS_BLOCK_HEAD_FAIL    -1056

#define DeMultiplex_SMF_LENGTH  1

#define MAX_DRA_LENGTH 8192 

#define NUM_DRA_SREAM  3 

const double TIME_SCALE = 1 / 22500;

static int getFrameHeader(cdr_byteArray* serviceFrame, int start, FrameHeader* FrameHeaderInfo)
{
    int position = start;     
    cdr_byte *handle = serviceFrame->handle;

    FrameHeaderInfo->Size = handle[position];
    position++;

    FrameHeaderInfo->Version = (handle[position] & 0xF0) >> 4;
    int emergencyOn = (handle[position] & 0x0C) >> 2;
    switch (emergencyOn)
    {
        case 0:
            FrameHeaderInfo->EmergencyOn = Emergency_None;
            break;
        case 1:
            FrameHeaderInfo->EmergencyOn = Emergency_FirstFrame;
            break;
        case 2:
            FrameHeaderInfo->EmergencyOn = Emergency_SMFHeadExtend;
            break;
        default:
            return -1;
    }
    position++;

    FrameHeaderInfo->ServiceMultiplexID = (handle[position] & 0xFC) >> 2;
    position++;

    FrameHeaderInfo->NetUpdateCount = handle[position] & 0x0F;
    position++;

    FrameHeaderInfo->ServiceUpdateCount = (handle[position] & 0xF0) >> 4;
    FrameHeaderInfo->ESGUpdateCount = handle[position] & 0x0F;
    position++;

    FrameHeaderInfo->NumOfService = handle[position] & 0x0F;
    position++;

    for (int i = 0; i < FrameHeaderInfo->NumOfService; i++)
    {
        FrameHeaderInfo->ServiceFrameSize[i] = (handle[position] << 16) + (handle[position + 1] << 8) + (handle[position + 2]);
        position += 3;
    }
    
    if (FrameHeaderInfo->EmergencyOn == Emergency_SMFHeadExtend)
    {
        position += 4;
    }

    FrameHeaderInfo->Size += 4;    
    int crcResult = CRC32(handle, start, FrameHeaderInfo->Size);
    position += 4;

    return crcResult;
}

static int getServiceHeader(cdr_byteArray* serviceFrame, int start, ServcieHeader* serviceHeaderInfo)
{
    int position = start;
    cdr_byte *handle = serviceFrame->handle;
            
    serviceHeaderInfo->Size = handle[position];
    position++;

    int timeValid   = ((handle[position] & 0x80) >> 7) != 0;
    int audioValid  = ((handle[position] & 0x40) >> 6) != 0;
    int dataValid   = ((handle[position] & 0x20) >> 5) != 0;
    int extendValid = ((handle[position] & 0x10) >> 4) != 0;
    int mode = (handle[position] & 0x08) >> 3;
    switch (mode)
    {
        case 0:
            serviceHeaderInfo->Mode = Pack_Mode2;
            break;
        case 1:
        default:
            serviceHeaderInfo->Mode = Pack_Mode1;
            break;
    }
    position++;

    int timeOffset = 0;
    if (timeValid)
    {
        timeOffset = (handle[position] << 24) + (handle[position + 1] << 16) + (handle[position + 2] << 8) + handle[position + 3];
        position += 4;
    }
    else
    {
        timeOffset = 0;
    }
    serviceHeaderInfo->TimeBase = timeOffset * TIME_SCALE;
    
    if (audioValid)
    {
        serviceHeaderInfo->AudioLength = (handle[position] << 13) + (handle[position + 1] << 5) + (handle[position + 2] >> 3);
        serviceHeaderInfo->NumOfAudio = handle[position + 2] & 0x7;
        position += 3;
    }
    else
    {
        serviceHeaderInfo->AudioLength = 0;
        serviceHeaderInfo->NumOfAudio = 0;
    }

    if (dataValid)
    {
        serviceHeaderInfo->DataLength = (handle[position] << 13) + (handle[position + 1] << 5) + (handle[position + 2] >> 3);
        position += 3;
    }
    else
    {
        serviceHeaderInfo->DataLength = 0;
    }
    
    if (audioValid && extendValid)
    {
        for (int i = 0; i < serviceHeaderInfo->NumOfAudio; i++)
        {
            serviceHeaderInfo->AudioInfo[i].CodeType = (handle[position] & 0xF0) >> 4;
            int codeRateValid    = ((handle[position] & 0x08) >> 3) != 0;
            int sampleRateValid  = ((handle[position] & 0x04) >> 2) != 0;
            int descriptionValid = ((handle[position] & 0x02) >> 1) != 0;
            int numberOfChannels  = ((handle[position] & 0x01) << 2) + ((handle[position + 1] & 0xC0) >> 6);
            switch (numberOfChannels)
            {
                case 1:
                    serviceHeaderInfo->AudioInfo[i].Channels = Channel_Mono;
                    break;
                case 2:
                    serviceHeaderInfo->AudioInfo[i].Channels = Channel_Stereo;
                    break;
                case 3:
                    serviceHeaderInfo->AudioInfo[i].Channels = Channel_Surround_5_1;
                    break;
                default:
                    return -1;
            }
            position += 2;

            if (codeRateValid)
            {
                serviceHeaderInfo->AudioInfo[i].CodeRate = ((handle[position] << 6) + ((handle[position + 1] & 0xFC) >> 2)) * 100;
                position += 2;
            }

            if (sampleRateValid)
            {
                int sampleRate = (handle[position] & 0x0F);
                switch (sampleRate)
                {
                    case 2:
                        serviceHeaderInfo->AudioInfo[i].SampleRate = Audio_Fs16k;
                        break;
                    case 3:
                        serviceHeaderInfo->AudioInfo[i].SampleRate = Audio_Fs22k;
                        break;
                    case 4:
                        serviceHeaderInfo->AudioInfo[i].SampleRate = Audio_Fs24k;
                        break;
                    case 5:
                        serviceHeaderInfo->AudioInfo[i].SampleRate = Audio_Fs32k;
                        break;
                    case 6:
                        serviceHeaderInfo->AudioInfo[i].SampleRate = Audio_Fs44k;
                        break;
                    case 7:
                        serviceHeaderInfo->AudioInfo[i].SampleRate = Audio_Fs48k;
                        break;
                    case 8:
                        serviceHeaderInfo->AudioInfo[i].SampleRate = Audio_Fs96k;
                        break;
                    default:
                        return -1;
                }
                position++;
            }
            
            if (descriptionValid)
            {
                for (int j=0; j<3; j++)
                {
                    serviceHeaderInfo->AudioInfo[i].Country[j] = (char) handle[position];
                    position ++;
                }
            }
        }
    }

    serviceHeaderInfo->Size += 4;    
    int crcResult = CRC32(handle, start, serviceHeaderInfo->Size);
    position += 4;
    return crcResult;
}

static int getAudioHeader(cdr_byteArray* serviceFrame, int start, AudioHeader* audioHeaderInfo)
{
    int position = start;
    cdr_byte *handle = serviceFrame->handle;

    audioHeaderInfo->NumOfUnit = handle[position];
    position++;

    for (int i = 0; i < audioHeaderInfo->NumOfUnit; i++)
    {
        audioHeaderInfo->UnitInfo[i].Length = (handle[position] << 8) + handle[position + 1];
        position += 2;
        audioHeaderInfo->UnitInfo[i].AudioID = (handle[position] & 0xE0) >> 5;
        position++;
        audioHeaderInfo->UnitInfo[i].TimeOffset = (double)((handle[position] << 8) + handle[position + 1]) * TIME_SCALE;
        position += 2;
    }

    audioHeaderInfo->Size = position - start + 4;   
    int crcresult = CRC32(handle, start, audioHeaderInfo->Size);
    position += 4;
    return crcresult;
}

static int getDataHeader(cdr_byteArray* serviceFrame, int start, DataHeader* dataHeaderInfo)
{
    int position = start;
    cdr_byte *handle = serviceFrame->handle;

    dataHeaderInfo->NumOfUnit = handle[position];
    position++;

    for (int i = 0; i < dataHeaderInfo->NumOfUnit; i++)
    {
        dataHeaderInfo->UnitInfo[i].D_Type = handle[position];
        position++;
        dataHeaderInfo->UnitInfo[i].Length = (handle[position] << 8) + handle[position + 1];
        position += 2;
    }
    
    dataHeaderInfo->Size = position - start;
    dataHeaderInfo->Size += 4;
    int crcResult = CRC32(handle, start, dataHeaderInfo->Size);
    position += 4;
    return crcResult;
}

static int getBlockHeader(cdr_byteArray* serviceFrame, int start, BlockHeader* unitHeaderInfo)
{
    int position = start;
    cdr_byte *handle = serviceFrame->handle;

    int sync = handle[position];
    if (sync != 0x55)
    { return -1; }
    position++;

    unitHeaderInfo->FirstBlock = (handle[position] & 0x80) != 0;
    unitHeaderInfo->LastBlock  = (handle[position] & 0x40) != 0;
    int type = (handle[position] & 0x30) >> 4;
    switch (type)
    {
        case 1:
            unitHeaderInfo->Type = Block_Audio;
            break;
        case 2:
            unitHeaderInfo->Type = Block_Data;
            break;
    }

    unitHeaderInfo->Length = ((handle[position] & 0x0F) << 8) + handle[position + 1];
    position += 2;

    if (unitHeaderInfo->Type == Block_Data)
    {
        unitHeaderInfo->D_Type = handle[position];
        position++;
    }

    unitHeaderInfo->Size = position - start + 1;
    int crcResult = CRC8(handle, start, unitHeaderInfo->Size);
    position++;
    
    return crcResult;
}

static void DeMultiplexMSDS_ResetBufferSize(DeMultiplexMSDSHandle* handle)
{
    int i;
    int numOfPrograms = handle->frameHeaderInfo->NumOfService;
    int oldPrograms = handle->numOfDRAStream;
    cdr_byteArray **oldBuffer  = handle->draBuffer;
    ServcieHeader **oldService = handle->serviceHeaderInfo;

    if (numOfPrograms > oldPrograms)
    {
        handle->numOfDRAStream    = NUM_DRA_SREAM;
        handle->serviceHeaderInfo = (ServcieHeader**)malloc(handle->numOfDRAStream * sizeof(ServcieHeader*));
        handle->draBuffer         = (cdr_byteArray**)malloc(handle->numOfDRAStream * sizeof(cdr_byteArray*));
    
        for (i = 0; i < oldPrograms; i++)
        {
            handle->serviceHeaderInfo[i] = oldService [i];
            handle->draBuffer[i]         = oldBuffer [i];
        }

        for (i = oldPrograms; i < handle->numOfDRAStream; i++)
        {
            handle->serviceHeaderInfo[i] = (ServcieHeader*)malloc (sizeof(ServcieHeader));
            handle->draBuffer[i]         = cdr_byteArray_Init(MAX_DRA_LENGTH*SuperFrame_numOfLogicalFrame);
        }

        free (oldService);
        free (oldBuffer);
    }
    return;
}

int DeMultiplexMSDS_GetNumOfPrograms (DeMultiplexMSDSHandle* handle)
{
    return handle->frameHeaderInfo->NumOfService;
}

cdr_byteArray* DeMultiplexMSDS_GetDraStream (DeMultiplexMSDSHandle* handle, int programeID)
{
    return handle->draBuffer[programeID];
}

DeMultiplexMSDSHandle* DeMultiplexMSDS_Init(void)
{ 
    DeMultiplexMSDSHandle* handle = (DeMultiplexMSDSHandle*)malloc (sizeof(DeMultiplexMSDSHandle));

    handle->frameHeaderInfo   = (FrameHeader*)  malloc (sizeof(FrameHeader));
    handle->audioHeaderInfo   = (AudioHeader*)  malloc (sizeof(AudioHeader));
    handle->dataHeaderInfo    = (DataHeader*)   malloc (sizeof(DataHeader));
    handle->blockHeaderInfo   = (BlockHeader*)  malloc (sizeof(BlockHeader));

    handle->numOfDRAStream    = NUM_DRA_SREAM;
    handle->serviceHeaderInfo = (ServcieHeader**)malloc(handle->numOfDRAStream * sizeof(ServcieHeader*));
    handle->draBuffer         = (cdr_byteArray**)malloc(handle->numOfDRAStream * sizeof(cdr_byteArray*));

    for (int i = 0; i < handle->numOfDRAStream; i++)
    {
        handle->serviceHeaderInfo[i] = (ServcieHeader*)malloc (sizeof(ServcieHeader));
        handle->draBuffer[i]         = cdr_byteArray_Init(MAX_DRA_LENGTH*SuperFrame_numOfLogicalFrame);
    }

    return handle;
}

void DeMultiplexMSDS_Release (DeMultiplexMSDSHandle* handle)
{
    free (handle->frameHeaderInfo);    
    free (handle->audioHeaderInfo);
    free (handle->dataHeaderInfo);
    free (handle->blockHeaderInfo);

    for (int i = 0; i < handle->numOfDRAStream; i++)
    {
        free (handle->serviceHeaderInfo[i]);
        cdr_byteArray_Release (handle->draBuffer[i]); 
    }
    free (handle->draBuffer);
    free (handle->serviceHeaderInfo);

    free (handle);

    return;
}

int DeMultiplexMSDS_Process(DeMultiplexMSDSHandle* handle, cdr_byteArray* serviceStream, int frameID)
{
    int error;
    int lastError = DeMultiplex_MSDS_NO_ERROR;
    int tsPosition = 0;

    for (int smfIndex = 0; smfIndex < DeMultiplex_SMF_LENGTH; smfIndex++)
    {
        error = (tsPosition >= serviceStream->validLength) ? DeMultiplex_MSDS_LESSDATA : DeMultiplex_MSDS_NO_ERROR;
        lastError = ErrorHandling (error, DeMultiplex_MSDS_LESSDATA, lastError);
        if (error != DeMultiplex_MSDS_NO_ERROR) 
        {
            break; 
        }

        error = getFrameHeader(serviceStream, tsPosition, handle->frameHeaderInfo);
        lastError = ErrorHandling (error, DeMultiplex_MSDS_SMF_FAIL, lastError);
        if (error != DeMultiplex_MSDS_NO_ERROR) 
        {
            break; 
        }
        tsPosition += handle->frameHeaderInfo->Size;

        DeMultiplexMSDS_ResetBufferSize (handle);

        for (int serviceIndex = 0; serviceIndex < handle->frameHeaderInfo->NumOfService; serviceIndex++)
        {
            int servicePosition = tsPosition;

            error = getServiceHeader(serviceStream, servicePosition, handle->serviceHeaderInfo[serviceIndex]);
            lastError = ErrorHandling (error, DeMultiplex_MSDS_SERVICE_HEAD_FAIL, lastError);
            if (error != DeMultiplex_MSDS_NO_ERROR) 
            {
                break; 
            }
            servicePosition += handle->serviceHeaderInfo[serviceIndex]->Size;

            if (frameID==0)
                handle->draBuffer[serviceIndex]->validLength = 0;

            if (handle->serviceHeaderInfo[serviceIndex]->AudioLength > 0)
            {
                int audioPosition = servicePosition;

                error = getAudioHeader(serviceStream, audioPosition, handle->audioHeaderInfo);
                lastError = ErrorHandling (error, DeMultiplex_MSDS_AUDIO_HEAD_FAIL, lastError);
                if (error != DeMultiplex_MSDS_NO_ERROR) 
                {
                    break;
                }
                audioPosition += handle->audioHeaderInfo->Size;

                for (int unitIndex = 0; unitIndex < handle->audioHeaderInfo->NumOfUnit; unitIndex++)
                {
                    int unitPosition = audioPosition;

                    switch (handle->serviceHeaderInfo[serviceIndex]->Mode)
                    {
                        case Pack_Mode1:
                        {
                            break;
                        }
                        case Pack_Mode2:
                        {
                            int blockPostion = unitPosition;

                            int blockLength = 0;
                            double currentTime = handle->serviceHeaderInfo[serviceIndex]->TimeBase + handle->audioHeaderInfo->UnitInfo[unitIndex].TimeOffset;

                            while (blockLength < handle->audioHeaderInfo->UnitInfo[unitIndex].Length)
                            {
                                error = getBlockHeader(serviceStream, blockPostion, handle->blockHeaderInfo);
                                lastError = ErrorHandling (error, DeMultiplex_MSDS_BLOCK_HEAD_FAIL, lastError);
                                if (error != DeMultiplex_MSDS_NO_ERROR) 
                                {
                                    break;
                                }

                                blockPostion += handle->blockHeaderInfo->Size;
                                blockLength  += handle->blockHeaderInfo->Size;

                                cdr_byte* source = serviceStream->handle + blockPostion;
                                cdr_byte* destination = handle->draBuffer[serviceIndex]->handle + handle->draBuffer[serviceIndex]->validLength;
                                ArrayCopy_byte(source, destination, handle->blockHeaderInfo->Length);

                                blockPostion += handle->blockHeaderInfo->Length;
                                blockLength  += handle->blockHeaderInfo->Length;
                                handle->draBuffer[serviceIndex]->validLength += handle->blockHeaderInfo->Length;
                            }
                            break;
                        }
                    }
                                    
                    audioPosition += handle->audioHeaderInfo->UnitInfo[unitIndex].Length;
                }
            }
            
            if (handle->serviceHeaderInfo[serviceIndex]->DataLength > 0)
            {
                int dataPosition = servicePosition + handle->serviceHeaderInfo[serviceIndex]->AudioLength;
                error = getDataHeader (serviceStream, dataPosition, handle->dataHeaderInfo);
                lastError = ErrorHandling (error, DeMultiplex_MSDS_DATA_HEAD_FAIL, lastError);
                if (error != DeMultiplex_MSDS_NO_ERROR) 
                {
                    break;
                }
            }

            tsPosition += handle->frameHeaderInfo->ServiceFrameSize[serviceIndex];
        }        
    }

    return lastError;
}



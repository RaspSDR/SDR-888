#include "cdrDeMultiplexSDIS.h"

#include "cdrUtility.h"

#include <stdlib.h>

#define DeMultiplex_SDIS_NO_ERROR  cdr_NO_ERROR
#define DeMultiplex_SDIS_LESSDATA   -1071
#define DeMultiplex_SDIS_HEAD_FAIL  -1073
#define DeMultiplex_SDIS_SERVICE_TABLE_FAIL    -1074
#define DeMultiplex_SDIS_NETWORK_TABLE_FAIL    -1075

static int getControlHeadInfo(cdr_byteArray* controlFrame, int start, ControlFrameHead* controlHeadInfo)
{
    int position = start;
    cdr_byte *handle = controlFrame->handle;
               
    controlHeadInfo->Size = (handle[position] << 2) + (handle[position + 1] >> 6);
    controlHeadInfo->NumOfTables = handle[position + 1] & 0x3F;
    position += 2;
            
    for (int i = 0; i < controlHeadInfo->NumOfTables; i++)
    {
        controlHeadInfo->ControlTableLength[i] = (handle[position] << 8) + (handle[position + 1]);
        position += 2;
    }
    
    position++;
    controlHeadInfo->Size++;   
    return CRC8(handle, start, controlHeadInfo->Size);
}

static int getServiceConfigTable(cdr_byteArray* controlFrame, int start, ServiceConfigTable* serviceConfigTable)
{
    int position = start;
    cdr_byte *handle = controlFrame->handle;

    int tableID = handle[position];
    position++;
    if (tableID != (int)SMCT)         
        return 0;

    serviceConfigTable->Size = ((handle[position]) << 8) + handle[position + 1];
    position += 2;

    serviceConfigTable->configBlockID     = (handle[position] & 0xF0) >> 4;
    serviceConfigTable->configBlockNumber = handle[position] & 0x0F;
    position++;

    serviceConfigTable->configBlockUpdate = (handle[position] & 0xF0) >> 4;
    position++;

    serviceConfigTable->NumOfSMFInfos = handle[position] & 0x3F;
    serviceConfigTable->NumOfTotalSMF = 0;
    position++;

    for (int i = 0; i<serviceConfigTable->NumOfSMFInfos; i++)
    {
        int current_SMF = (handle[position] & 0xFC) >> 2;
        int SMF_idx = 0;
        for (; SMF_idx<serviceConfigTable->NumOfTotalSMF; SMF_idx++)
        {
            if (current_SMF == serviceConfigTable->SMFInfo[SMF_idx].ServiceMultiplexID)
                break;
        }
        if (SMF_idx == serviceConfigTable->NumOfTotalSMF )
        {
            serviceConfigTable->NumOfTotalSMF++;
        }

        serviceConfigTable->SMFInfo[SMF_idx].ServiceMultiplexID   = current_SMF;
        serviceConfigTable->SMFInfo[SMF_idx].MultiLayerModulation = (handle[position] & 0x02) != 0;
        serviceConfigTable->SMFInfo[SMF_idx].ProtectionLayer      = (handle[position] & 0x01) != 0;
        position++;

        serviceConfigTable->SMFInfo[SMF_idx].TransferMode = (handle[position] & 0xF0) >> 4;
        serviceConfigTable->SMFInfo[SMF_idx].NumOfService =  handle[position] & 0x0F;
        position++;

        for (int j = 0; j<serviceConfigTable->SMFInfo[i].NumOfService; j++)
        {
            serviceConfigTable->SMFInfo[SMF_idx].ServiceID[j] = (handle[position] << 8) + handle[position + 1];
            position += 2;
        }

        position += 2;
    }

    position += 4;
    serviceConfigTable->Size += 4;    
    return CRC32(handle, start, serviceConfigTable->Size);
}

static int getNetInformationTable(cdr_byteArray* controlFrame, int start, NetInformationTable* netInformationTable)
{
    int position = start;
    cdr_byte *handle = controlFrame->handle;

    int tableID = handle[position];
    position++;
    if (tableID != NIT)         
        return 0;

    netInformationTable->Size = ((handle[position]) << 8) + handle[position + 1];
    position += 2;

    netInformationTable->netBlockID     = (handle[position] & 0xF0) >> 4;
    netInformationTable->netBlockNumber =  handle[position] & 0x0F;
    position++;

    netInformationTable->netBlockID = (handle[position] & 0xF0) >> 4;
    position++;

    if (netInformationTable->netBlockID == 1)
    {
        for (int i=0;i<3;i++)
        {
            netInformationTable->Country[i] = handle[position];
            position ++;
        }

        netInformationTable->CurrentNet.NetID =  ( handle[position]     << 28)
                                               + ( handle[position + 1] << 20)
                                               + ( handle[position + 2] << 12)
                                               + ( handle[position + 3] << 4)
                                               + ((handle[position + 4] & 0xF0) >> 4);
        
        netInformationTable->CurrentNet.NumOfFreq = ((handle[position + 4] & 0x0F) << 4) + handle[position + 5];
        position += 6;

        for (int i = 0; i < netInformationTable->CurrentNet.NumOfFreq; i++)
        {
            netInformationTable->CurrentNet.AllFrequney[i] = ((handle[position]     << 24)
                                                            + (handle[position + 1] << 16)
                                                            + (handle[position + 2] << 8)
                                                            + (handle[position + 3])) * 10.0;
            position += 4;
        }

        netInformationTable->CurrentNet.NameLength = handle[position];
        position++;
        for (int i=0; i<netInformationTable->CurrentNet.NameLength; i++)
        {
            netInformationTable->CurrentNet.NetName[i] = handle[position];
            position ++;
        }
    }

    netInformationTable->NumOfNeighbors = (handle[position] & 0xFC) >> 2;
    position++;

    for (int i = 0; i < netInformationTable->NumOfNeighbors; i++)
    {
        netInformationTable->Neighbors[i].NetID =   (handle[position]     << 28)
                                                  + (handle[position + 1] << 20)
                                                  + (handle[position + 2] << 12)
                                                  + (handle[position + 3] << 4)
                                                  +((handle[position + 4] & 0xF0) >> 4);
        netInformationTable->Neighbors[i].NumOfFreq = ((handle[position + 4] & 0x0F) << 4) + handle[position + 5];
        position += 6;
        for (int j = 0; j < netInformationTable->Neighbors[i].NumOfFreq; j++)
        {
            netInformationTable->Neighbors[i].AllFrequney[j] =  ((handle[position]     << 24)
                                                               + (handle[position + 1] << 16)
                                                               + (handle[position + 2] << 8)
                                                               + (handle[position + 3])) * 10.0;
            position += 4;
        }

        position += 2;
    }

    position += 4;
    netInformationTable->Size += 4;    
    return CRC32(handle, start, netInformationTable->Size);
}

DeMultiplexSDISHandle* DeMultiplexSDIS_Init(void)
{ 
    DeMultiplexSDISHandle* handle = (DeMultiplexSDISHandle*)malloc (sizeof(DeMultiplexSDISHandle));

    handle->controlFrameHead = (ControlFrameHead*)    malloc (sizeof(ControlFrameHead));
    handle->serviceConfig    = (ServiceConfigTable*)  malloc (sizeof(ServiceConfigTable));
    handle->netInformation   = (NetInformationTable*) malloc (sizeof(NetInformationTable));

    return handle;
}

void DeMultiplexSDIS_Release (DeMultiplexSDISHandle* handle)
{
    free (handle->controlFrameHead);
    free (handle->serviceConfig);
    free (handle->netInformation);  

    free (handle);

    return;
}

int DeMultiplexSDIS_Process(DeMultiplexSDISHandle* handle, cdr_byteArray* controlStream)
{
    int error;
    int lastEror = DeMultiplex_SDIS_NO_ERROR;
    int tsPosition = 0;  

    error = getControlHeadInfo(controlStream, tsPosition, handle->controlFrameHead);
    lastEror = ErrorHandling (error, DeMultiplex_SDIS_HEAD_FAIL, lastEror);
    if (error != DeMultiplex_SDIS_NO_ERROR)
    {
        return error;
    }

    tsPosition += handle->controlFrameHead->Size;
    
    int tablePostion = tsPosition;
    for (int tableIndex = 0; tableIndex < handle->controlFrameHead->NumOfTables; tableIndex++)
    {
        error = (tablePostion >= controlStream->validLength) ? DeMultiplex_SDIS_LESSDATA : cdr_NO_ERROR;
        lastEror = ErrorHandling (error, DeMultiplex_SDIS_LESSDATA, lastEror);
        if (error != DeMultiplex_SDIS_NO_ERROR)
        {
            break;
        }

        error = getServiceConfigTable(controlStream, tablePostion, handle->serviceConfig);
        lastEror = ErrorHandling (error, DeMultiplex_SDIS_SERVICE_TABLE_FAIL, lastEror);
        if (error != DeMultiplex_SDIS_NO_ERROR)
        {
            break;
        }
       
        error = getNetInformationTable(controlStream, tablePostion, handle->netInformation);
        lastEror = ErrorHandling (error, DeMultiplex_SDIS_NETWORK_TABLE_FAIL, lastEror);
        if (error != DeMultiplex_SDIS_NO_ERROR)
        {
            break;
        }

        tablePostion += handle->controlFrameHead->ControlTableLength[tableIndex];
    }

    return lastEror;
}



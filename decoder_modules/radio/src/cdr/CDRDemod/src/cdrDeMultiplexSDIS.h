#ifndef CDR_DEMULX_SDIS_H
#define CDR_DEMULX_SDIS_H

#include <cdrDemod.h>

#ifndef YYM_DEBUG
#define YYM_DEBUG

#define NUM_Control_Tables 2

#define NUM_SMFInfos       1
#define NUM_ServiceInfos   3

#define NUM_NetFrequency   1
#define NUM_NetNeighbor    1

#else
#define NUM_Control_Tables 63
#define NUM_SMFInfos       63
#define NUM_ServiceInfos   15
#define NUM_NetFrequency   4096
#define NUM_NetNeighbor    63

#endif

enum ControlTableType
{
    SMCT = 1,              
    NIT  = 2,            
    ESG  = 3,           
};
 
typedef struct ControlFrameHead
{
    int Size;
    int NumOfTables;
    int ControlTableLength [NUM_Control_Tables];
}ControlFrameHead;

typedef struct SMFConfigInfo
{
    int ServiceMultiplexID;
    int MultiLayerModulation;
    int ProtectionLayer;
    int TransferMode;
    int NumOfService;
    int ServiceID[NUM_ServiceInfos];
}SMFConfigInfo;

typedef struct ServiceConfigTable
{
    int Size;

    int configBlockID;
    int configBlockNumber;
    int configBlockUpdate;

    int NumOfSMFInfos;
    int NumOfTotalSMF;
    SMFConfigInfo SMFInfo[NUM_SMFInfos];
}ServiceConfigTable;

typedef struct NetDescription
{
    int NetID;
    int NameLength;
    char NetName [256];
    int NumOfFreq;
    double AllFrequney[NUM_NetFrequency];
}NetDescription;

typedef struct NetInformationTable
{
    int Size;

    int netBlockID;
    int netBlockNumber;
    int netBlockUpdate;

    char Country[3];
    NetDescription CurrentNet;
    int NumOfNeighbors;
    NetDescription Neighbors[NUM_NetNeighbor];
}NetInformationTable;

typedef struct DeMultiplexSDISHandle
{
    ControlFrameHead    *controlFrameHead;
    ServiceConfigTable  *serviceConfig;
    NetInformationTable *netInformation;

}DeMultiplexSDISHandle;

DeMultiplexSDISHandle* DeMultiplexSDIS_Init(void);

void DeMultiplexSDIS_Release (DeMultiplexSDISHandle* handle);

int DeMultiplexSDIS_Process(DeMultiplexSDISHandle* handle, cdr_byteArray* controlStream);

#endif
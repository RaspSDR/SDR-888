#ifndef CDR_DEMULX_MSDS_H
#define CDR_DEMULX_MSDS_H

#include <cdrDemod.h>

#ifndef YYM_DEBUG
#define YYM_DEBUG

#define NUM_Service        3
#define NUM_Audio          1
#define NUM_Unit          15

#else
#define NUM_Service       15
#define NUM_Audio          7
#define NUM_Unit         255

#endif

enum EmergencyType
{
    Emergency_None          = 0,         
    Emergency_FirstFrame    = 1,         
    Emergency_SMFHeadExtend = 2,         
};

enum PackMode
{
    Pack_Mode2 = 0,
    Pack_Mode1 = 1,
};

enum AudioSampleRate
{
    Audio_Fs16k = 16000,
    Audio_Fs22k = 22050,
    Audio_Fs24k = 24000,
    Audio_Fs32k = 32000,
    Audio_Fs44k = 44100,
    Audio_Fs48k = 48000,
    Audio_Fs96k = 96000,
};

enum ChannelMode
{
    Channel_Mono         = 1,
    Channel_Stereo       = 2,
    Channel_Surround_5_1 = 6,
};

enum BlockType
{
    Block_Audio = 1,
    Block_Data  = 2,
};

enum DataType
{
    Data_ESG         =  0,
    Data_ESG_Remind  =  1,
    Data_Emergency   = 64,
    Data_Boardcast   =160,
    Data_Test        =255,
};

typedef struct FrameHeader
{
    int Size;
    int Version;
    enum EmergencyType EmergencyOn;
    int ServiceMultiplexID;
    int NetUpdateCount;
    int ServiceUpdateCount;
    int ESGUpdateCount;
    int NumOfService;
    int ServiceFrameSize [NUM_Service];
}FrameHeader;

typedef struct AudioFormat
{
    int CodeType;
    double CodeRate;
    enum AudioSampleRate SampleRate;
    char Country [3];
    enum ChannelMode Channels;
}AudioFormat;

typedef struct ServcieHeader
{
    int Size;
    double TimeBase;
    int AudioLength;
    int DataLength;
    enum PackMode Mode;                
    int NumOfAudio;
    AudioFormat AudioInfo[NUM_Audio];
}ServcieHeader;

typedef struct AudioUnit
{
    int Length;
    int AudioID;
    double TimeOffset;
}AudioUnit;

typedef struct AudioHeader
{
    int Size;
    int NumOfUnit;
    AudioUnit UnitInfo [NUM_Unit];
}AudioHeader;

typedef struct DataUnit
{
    int Length;
    enum DataType D_Type;
}DataUnit;

typedef struct DataHeader
{
    int Size;
    int NumOfUnit;
    DataUnit UnitInfo [NUM_Unit];
}DataHeader;

typedef struct BlockHeader
{
    int Size;
    int FirstBlock;
    int LastBlock;
    enum BlockType Type;
    int Length;
    enum DataType D_Type;
}BlockHeader;

typedef struct DeMultiplexMSDSHandle
{
    FrameHeader      *frameHeaderInfo;
    ServcieHeader    **serviceHeaderInfo;
    AudioHeader      *audioHeaderInfo;
    DataHeader       *dataHeaderInfo;
    BlockHeader      *blockHeaderInfo;

    cdr_byteArray    **draBuffer;
    int              numOfDRAStream;

}DeMultiplexMSDSHandle;

DeMultiplexMSDSHandle* DeMultiplexMSDS_Init(void);

void DeMultiplexMSDS_Release (DeMultiplexMSDSHandle* handle);

int DeMultiplexMSDS_Process(DeMultiplexMSDSHandle* handle, cdr_byteArray* serviceStream, int frameID);

int DeMultiplexMSDS_GetNumOfPrograms (DeMultiplexMSDSHandle* handle);

cdr_byteArray* DeMultiplexMSDS_GetDraStream (DeMultiplexMSDSHandle* handle, int programeID);

#endif
#ifndef DRA_CONST_H
#define DRA_CONST_H

#define FrameStart 0x7FFF
#define FrameStart_Byte0 (FrameStart>>8)
#define FrameStart_Byte1 (FrameStart&0xFF)

extern const int SampleRateList[16];

#define MAX_NumCluster 3

#define MAX_HSNumBands 31

#define MAX_CBArraySize 25

#define SHORT_BLOCKS_PER_FRAME 8

#define SHORT_MDCT_BINS 128

#define SHORT_IMDCT_SAMPLES (SHORT_MDCT_BINS*2)

#define SHORT_SAMPLES_OFFSET 448

#define LONG_MDCT_BINS (SHORT_MDCT_BINS*SHORT_BLOCKS_PER_FRAME)

#define LONG_IMDCT_SAMPLES (LONG_MDCT_BINS*2)

#define MDCT_BINS_PER_FRAME LONG_MDCT_BINS

#define SAMPLES_PER_FRAME LONG_MDCT_BINS

#define IMDCT_SAMPLES (SAMPLES_PER_FRAME*2)

extern const float aunStepSize [116];

extern const int QuantizationStep [116];

#define QuantizationMaxStep 8388608     
#define QuantizationStepLength  116

typedef struct CriticalBandInfo
{  
    const int* data;     
    int size;            
} CriticalBandInfo;

extern const CriticalBandInfo CriticalBands[2][13];

typedef struct DctWindowInfo
{  
    const float* coef;   
    int size;            
} DctWindowInfo;

extern const DctWindowInfo DctWindows[13];

#endif
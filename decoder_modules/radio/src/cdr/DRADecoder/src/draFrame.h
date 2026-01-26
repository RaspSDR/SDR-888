#ifndef _DRA_FRAME_H
#define _DRA_FRAME_H

#include "draDecoder.h"
#include "draConst.h"
#include "draMDCT.h"
#include <stdbool.h>

enum FrameHeaderType
{
    NoramlHead=0,  
    ExtendHead=1,   
};

enum WinType
    {
        WIN_LONG_LONG2LONG = 0,
        WIN_LONG_LONG2SHORT = 1,
        WIN_LONG_SHORT2LONG = 2,
        WIN_LONG_SHORT2SHORT = 3,
        WIN_LONG_LONG2BRIEF = 4,
        WIN_LONG_BRIEF2LONG = 5,
        WIN_LONG_BRIEF2BRIEF = 6,
        WIN_LONG_SHORT2BRIEF = 7,
        WIN_LONG_BRIEF2SHORT = 8,
        WIN_SHORT_SHORT2SHORT = 9,
        WIN_SHORT_SHORT2BRIEF = 10,
        WIN_SHORT_BRIEF2BRIEF = 11,
        WIN_SHORT_BRIEF2SHORT = 12,
        WIN_UNDEFINE = 13,
        WIN_UNDEFINE_14 = 14,
        WIN_UNDEFINE_15 = 15,
    };

typedef struct FrameHeaderInfo
{
    enum FrameHeaderType nFrmHeaderType;    
    int nNumWord;                         
    int nDRABits;                         
    int nFrameLength;                     
    int nNumBlocksPerFrm;                
    int nSampleRateIndex;              
    int nSampleRate;                   
    int nNumNormalCh;                  
    int nNumLfeCh;                     
    bool bAuxData;                     
    int nAuxDataLength;                 
    bool bUseSumDiff;                  
    bool bUseJIC;                      
    int nJicCb;                        
} FrameHeaderInfo;

typedef struct ChannelDataInfo
{
    int nMDCTBins;                                          
    bool bShortWindow;                                       
    
    enum WinType nWinTypeCurrent;                         
    int nNumCluster;                                            
    
    int anClusterBin0[MAX_NumCluster];                     
    int anNumBlocksPerFrmPerCluster[MAX_NumCluster];              
    
    int anHSNumBands[MAX_NumCluster];                      
    int mnHSBandEdge[MAX_NumCluster][MAX_HSNumBands];        
    int mnHS[MAX_NumCluster][MAX_HSNumBands];                

    int pnCBEdge[MAX_CBArraySize];                           
    int nCBEdgeLength;                                    
    int anMaxActCb[MAX_NumCluster];                        

    int anQIndex[MDCT_BINS_PER_FRAME];                     
    int mnQStepIndex[MAX_NumCluster][MAX_CBArraySize];      

    bool anSumDffAllOff[MAX_NumCluster];                   
    bool mnSumDffOn[MAX_NumCluster][MAX_CBArraySize];       

    int mnJicStepIndex[MAX_NumCluster][MAX_CBArraySize];    

    float afBinReconst[MDCT_BINS_PER_FRAME];               

    float afBinNatural[MDCT_BINS_PER_FRAME];              

    enum WinType anWinTypeShort[SHORT_BLOCKS_PER_FRAME];   

    enum WinType nWinTypeLast;                            

    float afAudio[MDCT_BINS_PER_FRAME];                   

    float afHistory[MDCT_BINS_PER_FRAME];                 
} ChannelDataInfo;

typedef struct bitStreamInfo
{
    dra_byte *inputData;
    int inputDataLength;
    int processInputLength;

    int bitIndex;
}bitStreamInfo;

typedef struct auidoStreamInfo
{
    dra_short *outputData;
    int outputDataLength;
    int validOutputLength;
} auidoStreamInfo;

typedef struct imdctBufferInfo
{
    float imdctLongSamples [LONG_IMDCT_SAMPLES];
    float imdctShortBins [SHORT_MDCT_BINS];
    float imdctShortSamples[SHORT_IMDCT_SAMPLES];
    float imdctTotalFrameShort[LONG_IMDCT_SAMPLES];
} imdctBufferInfo;

typedef struct decodeFrameInfo
{
    int error;
    
    bitStreamInfo   *bitStream;

    auidoStreamInfo *audioStream;

    FrameHeaderInfo *frameHeader;

    ChannelDataInfo *allChannels;
    int channelNumber;
    
    imdctBufferInfo *imdctBuffer;
    
    draMDCT *mdctLongPlan;
    draMDCT *mdctShortPlan;

} decodeFrameInfo;

decodeFrameInfo* dra_Frame_Init (void);

void dra_Frame_Release(decodeFrameInfo *frame_handle);

int dra_Frame_Proccess(decodeFrameInfo *frame_handle, dra_byte *draStream, int *draLength, dra_short *audioData, int *audioLength);

int dra_Frame_FindSyncHeader(dra_byte *processBuffer, int length);

#endif
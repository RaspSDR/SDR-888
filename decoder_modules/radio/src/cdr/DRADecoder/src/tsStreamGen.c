#include "ts.h"

#include <string.h>
#include <stdlib.h>

#define Max_Stream_Counter 3
const int PMT_ID [Max_Stream_Counter] = {0x102, 0x103, 0x104};
const int ES_ID  [Max_Stream_Counter] = {0x294, 0x295, 0x296};
const int PMT_CRC[Max_Stream_Counter][4] = {{0x36, 0xB8, 0x21, 0x9F},{0xDB, 0xC3, 0xDF, 0xA7}, {0xED, 0x4B, 0x3D, 0x34}};

typedef struct tsFrameInfo
{
    long long timeInc;

    long long currentTime [Max_Stream_Counter];

    int PATcounter;
    int PMTcounter [Max_Stream_Counter];
    int EScounter  [Max_Stream_Counter];

}tsFrameInfo;

static void Generate_PTS (long long pts, ts_byte *ptsBytes)
{
    ptsBytes[0] = (ts_byte)((((pts >> 30) & 7)    << 1) | 0x21);     
    ptsBytes[1] = (ts_byte)  ((pts >> 22) & 0xff);                   
    ptsBytes[2] = (ts_byte)((((pts >> 15) & 0x7f) << 1) | 0x01);     
    ptsBytes[3] = (ts_byte)  ((pts >>  7) & 0xff);                   
    ptsBytes[4] = (ts_byte) (((pts        & 0x7f) << 1) | 0x01);     
}

tsHandle ts_Init (void)
{
    tsFrameInfo *handle = (tsFrameInfo*) malloc (sizeof(tsFrameInfo));
    
    handle->PATcounter  = 0;
    for (int i=0;i<Max_Stream_Counter;i++)
    {
        handle->currentTime[i] = 0;
        handle->PMTcounter [i] = 0;
        handle->EScounter  [i] = 0;
    }
    handle->timeInc = (long long)(1024 / 24000.0 * 90000);         

    return (tsHandle)handle;
}

void ts_Release (tsHandle handle)
{
    tsFrameInfo *info = (tsFrameInfo*) handle;
    free (info);
    return;
}

int ts_GenES (tsHandle handle, int id, ts_byte *input, int inputLength, ts_byte *output)
{
    tsFrameInfo *info = (tsFrameInfo*) handle;

    memset(output, 0xFF, TS_BLOCK_SIZE);


    output[0] = 0x47;                                                    
    output[1] = 0x40 | ((ES_ID[id] >> 8) & 0x1F);                       
    output[2] = (ES_ID[id]) & 0xFF;                                    
    output[3] = 0x10 | ((info->EScounter[id]) & 0x0F);                
    output[4] = 0x00;                                                  
    output[5] = 0x00;
    output[6] = 0x01;
    output[7] = 0xBD;                                                  
    output[8] = ((inputLength+8) >> 8) & 0xFF;                        
    output[9] = ((inputLength+8) )     & 0xFF;
    output[10]= 0x80;                                                 
    output[11] = 0x80;                                                 
    output[12] = 0x05;                                                
    Generate_PTS (info->currentTime[id], output+13);                  
    
    memcpy(output+18, input, inputLength * sizeof(ts_byte));
    
    info->EScounter[id]++;
    info->currentTime[id] += info->timeInc;

    return TS_BLOCK_SIZE;
}

int ts_GenPAT (tsHandle handle, ts_byte *output)
{
    tsFrameInfo *info = (tsFrameInfo*) handle;

    memset(output, 0xFF, TS_BLOCK_SIZE);

    output[0] = 0x47;
    output[1] = 0x40;
    output[2] = 0x00;                                        
    output[3] = 0x10 | ((info->PATcounter) & 0x0F);
    output[4] = 0x00;                                      
    output[5] = 0x00;
    output[6] = 0x80;
    output[7] = 0x15;                                              
    output[8] = 0x00;                                      
    output[9] = 0x01;
    output[10]= 0xDB;                                     
    output[11] = 0x00;                                     
    output[12] = 0x00;                                            
    output[13] = 0x00;                                     
    output[14] = 0x01;
    output[15] = 0xE0 | ((PMT_ID[0]>>8)&0x0F);             
    output[16] = (PMT_ID[0])&0xFF;
    output[17] = 0x00;                                                                            
    output[18] = 0x02;
    output[19] = 0xE0 | ((PMT_ID[1]>>8)&0x0F);             
    output[20] = (PMT_ID[1])&0xFF;
    output[21] = 0x00;                                       
    output[22] = 0x03;
    output[23] = 0xE0 | ((PMT_ID[2]>>8)&0x0F);             
    output[24] = (PMT_ID[2])&0xFF;
    output[25] = 0x4C;                                     
    output[26] = 0x76;
    output[27] = 0x85;
    output[28] = 0x3A;
    
    info->PATcounter++;

    return TS_BLOCK_SIZE;
}

int ts_GenPMT (tsHandle handle, int id, ts_byte *output)
{
    tsFrameInfo *info = (tsFrameInfo*) handle;

    memset(output, 0xFF, TS_BLOCK_SIZE);

    output[0] = 0x47;
    output[1] = 0x40 | ((PMT_ID[id]>>8)&0x0F);                 
    output[2] = (PMT_ID[id])&0xFF;
    output[3] = 0x10 | ((info->PMTcounter[id]) & 0x0F);
    output[4] = 0x00;                                            
    output[5] = 0x02;
    output[6] = 0xB0;
    output[7] = 0x21;                                          
    output[8] = 0x00;                                            
    output[9] = 0x01;
    output[10]= 0xC3;                                          
    output[11] = 0x00;
    output[12] = 0x00;
    output[13] = 0xE0 | ((ES_ID[id]>>8)&0x0F);                 
    output[14] = (ES_ID[id])&0xFF;
    output[15] = 0xF0;                                           
    output[16] = 0x05;
    output[17] = 0x0E;
    output[18] = 0x03;
    output[19] = 0xC0;
    output[20] = 0x00;
    output[21] = 0x96;
    output[22] = 0x06;                                             
    output[23] = 0xE0 | ((ES_ID[id]>>8)&0x0F);                 
    output[24] = (ES_ID[id])&0xFF;
    output[25] = 0xF0;                                         
    output[26] = 0x0A; 
    output[27] = 0x05;                                            
    output[28] = 0x04;                                          
    output[29] = 0x44;                                           
    output[30] = 0x52;
    output[31] = 0x41;
    output[32] = 0x31;
    output[33] = 0xA0;                                            
    output[34] = 0x02;                                        
    output[35] = 0x50;                                            
    output[36] = 0x40;
    output[37] = PMT_CRC[id][0];                               
    output[38] = PMT_CRC[id][1]; 
    output[39] = PMT_CRC[id][2]; 
    output[40] = PMT_CRC[id][3]; 

    info->PMTcounter[id]++;

    return TS_BLOCK_SIZE;
}

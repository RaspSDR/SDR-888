#include "draFrame.h"

#include "draUtility.h"
#include "draMDCT.h"
#include "Huffmanbook.h"
#include "huffmanConst.h"

#include <stdlib.h>
#include <math.h>

#define DRA_NO_ERROR 0
#define DRA_ERROR_INVALID_FRAME_LENG  -2001
#define DRA_ERROR_INVALID_FRAME_BLOCK -2002
#define DRA_ERROR_INVALID_SAMPLE_RATE -2003
#define DRA_ERROR_INVALID_WIN_TYPE    -2004
#define DRA_ERROR_INVALID_CODE_BOOK   -2005
#define DRA_ERROR_INVALID_CB_EDGE     -2006
#define DRA_ERROR_INVALID_FRAME_START -2007


#define max(a,b) ( ((a)>(b))? (a):(b) )
#define min(a,b) ( ((a)<(b))? (b):(a) )

static void ResetChannel (ChannelDataInfo* currentCH)
{
    for (int i = 0; i < MAX_NumCluster; i++)
    {
        currentCH->anClusterBin0[i] = 0;
        currentCH->anNumBlocksPerFrmPerCluster[i] = 0;
        currentCH->anHSNumBands[i] = 0;
        currentCH->anMaxActCb[i] = 0;
        currentCH->anSumDffAllOff[i] = 0;

        for (int j = 0; j < MAX_HSNumBands; j++)
        {
            currentCH->mnHSBandEdge[i][j] = 0;
            currentCH->mnHS[i][j] = 0;
        }

        for (int j = 0; j < MAX_CBArraySize; j++)
        {
            currentCH->mnQStepIndex[i][j] = 0;
            currentCH->mnSumDffOn[i][j] = false;
            currentCH->mnJicStepIndex[i][j] = false;
        }
    }

    for (int i = 0; i < MAX_CBArraySize; i++)
    {
        currentCH->pnCBEdge[i] = 0;
    }

    for (int i = 0; i < MDCT_BINS_PER_FRAME; i++)
    {
        currentCH->anQIndex[i] = 0;
        currentCH->afBinReconst[i] = 0.0f;
        currentCH->afBinNatural[i] = 0.0f;
        currentCH->afAudio[i] = 0.0f;
        currentCH->afHistory[i] = 0.0f;
    }

    for (int i = 0; i < SHORT_BLOCKS_PER_FRAME; i++)
    {
        currentCH->anWinTypeShort[i] = WIN_LONG_LONG2LONG;
    }
}

static void ResetImdcBuffer (imdctBufferInfo* buffer)
{
    dra_ArrayClear_float(buffer->imdctLongSamples, LONG_IMDCT_SAMPLES);
    dra_ArrayClear_float(buffer->imdctShortBins, SHORT_MDCT_BINS);
    dra_ArrayClear_float(buffer->imdctShortSamples, SHORT_IMDCT_SAMPLES);
    dra_ArrayClear_float(buffer->imdctTotalFrameShort, LONG_IMDCT_SAMPLES);
}

static int Unpack(void* steamInput, int length)
{
    bitStreamInfo* inner_stream = (bitStreamInfo*)steamInput;
    if (length > 31)
    {
        inner_stream->bitIndex += length;
        return 0;
    }
    int data = 0;
    for (int i = 0; i < length; i++)
    {
        data = data << 1;                
        int byteIndex = inner_stream->bitIndex >> 3;              
        int bitInByte = 7 - (inner_stream->bitIndex % 8);         
        data += (((int)(inner_stream->inputData[byteIndex])) >> bitInByte) & 0x1;
        inner_stream->bitIndex++;
    }
    return data;
}

static void ErrorHandling(decodeFrameInfo* frameInfo, int errorCode)
{
    frameInfo->error = errorCode;
    bitStreamInfo* bitStream = frameInfo->bitStream;
    int bitsOverByte = bitStream->bitIndex % 8;
    int extBits = (bitsOverByte==0) ? 0 : (8 - bitsOverByte);
    Unpack(bitStream, extBits);
    return;
}

static void FrameHeader(decodeFrameInfo* frameInfo)
{
    if (frameInfo->error) { return; }

    bitStreamInfo* bitStream = frameInfo->bitStream;
    FrameHeaderInfo* frameHeader = frameInfo->frameHeader;  

    frameHeader->nFrmHeaderType = (Unpack(bitStream, 1) == 0) ? NoramlHead : ExtendHead;

    if (frameHeader->nFrmHeaderType == NoramlHead)
    {
        frameHeader->nNumWord = Unpack(bitStream, 10);
    }
    else if (frameHeader->nFrmHeaderType == ExtendHead)
    {
        frameHeader->nNumWord = Unpack(bitStream, 13);
    }
    frameHeader->nDRABits = frameHeader->nNumWord*32;
    frameHeader->nFrameLength = frameHeader->nNumWord*4;

    if (frameHeader->nFrameLength > bitStream->inputDataLength)
    { 
        ErrorHandling(frameInfo, DRA_ERROR_INVALID_FRAME_LENG);
        return;
    }

    frameHeader->nNumBlocksPerFrm = 1 << Unpack(bitStream, 2);
    if (frameHeader->nNumBlocksPerFrm != 8)
    {
        ErrorHandling(frameInfo, DRA_ERROR_INVALID_FRAME_BLOCK);
        return;
    }

    frameHeader->nSampleRateIndex = Unpack(bitStream, 4);
    frameHeader->nSampleRate = SampleRateList[frameHeader->nSampleRateIndex];
    if (frameHeader->nSampleRate == -1)
    {
        ErrorHandling(frameInfo, DRA_ERROR_INVALID_SAMPLE_RATE);
        return;
    } 

    if (frameHeader->nFrmHeaderType == NoramlHead)
    {
        frameHeader->nNumNormalCh = Unpack(bitStream, 3) + 1;
        frameHeader->nNumLfeCh = Unpack(bitStream, 1);
    }
    else if (frameHeader->nFrmHeaderType == ExtendHead)
    {
        frameHeader->nNumNormalCh = Unpack(bitStream, 6) + 1;
        frameHeader->nNumLfeCh = Unpack(bitStream, 2);
    }

    frameHeader->bAuxData = Unpack(bitStream, 1) == 1;

    if (frameHeader->nFrmHeaderType == NoramlHead)
    {
        if (frameHeader->nNumNormalCh > 1)
        {
            frameHeader->bUseSumDiff = Unpack(bitStream, 1) == 1;
            frameHeader->bUseJIC = Unpack(bitStream, 1) == 1;
        }
        else
        {
            frameHeader->bUseSumDiff = false;
            frameHeader->bUseJIC = false;
        }
        if (frameHeader->bUseJIC)
        {
            frameHeader->nJicCb = Unpack(bitStream, 5) + 1;
        }
        else
        {
            frameHeader->nJicCb = 0;
        }
    }
    else
    {
        frameHeader->bUseSumDiff = false;
        frameHeader->bUseJIC = false;
        frameHeader->nJicCb = 0;
    }
}

static void UnpackWinSequence(decodeFrameInfo* frameInfo, int nCh)
{
    if (frameInfo->error) { return; }

    bitStreamInfo* bitStream = frameInfo->bitStream;
    FrameHeaderInfo* frameHeader = frameInfo->frameHeader;
    ChannelDataInfo* ch0Data = &(frameInfo->allChannels[0]);
    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);

    if (nCh == 0 || (!frameHeader->bUseJIC && !frameHeader->bUseSumDiff))
    {
        channelData->nWinTypeCurrent = (enum WinType)Unpack(bitStream, 4);
        if (channelData->nWinTypeCurrent >= WIN_UNDEFINE)
        {
            ErrorHandling(frameInfo, DRA_ERROR_INVALID_WIN_TYPE);
            return;
        }

        channelData->bShortWindow = channelData->nWinTypeCurrent > WIN_LONG_BRIEF2SHORT;
        channelData->nMDCTBins = channelData->bShortWindow ? SHORT_MDCT_BINS : LONG_MDCT_BINS;

        if (channelData->bShortWindow)
        {
            channelData->nNumCluster = Unpack(bitStream, 2) + 1;
            if (channelData->nNumCluster >= 2)
            {
                const HuffmanStruct* pClusterBook = &HuffDec1_7x1;
                int nLast = 0;
                for (int nCluster = 0; nCluster < channelData->nNumCluster - 1; nCluster++)
                {
                    int k = HuffmanDecode(pClusterBook, bitStream, Unpack) + 1;
                    channelData->anNumBlocksPerFrmPerCluster[nCluster] = k;
                    nLast += k;
                }
                if (frameHeader->nNumBlocksPerFrm <= nLast)
                {
                    ErrorHandling(frameInfo, DRA_ERROR_INVALID_FRAME_BLOCK);
                    return;
                }
                channelData->anNumBlocksPerFrmPerCluster[channelData->nNumCluster - 1] = frameHeader->nNumBlocksPerFrm - nLast;
            }
            else
            {
                channelData->anNumBlocksPerFrmPerCluster[0] = frameHeader->nNumBlocksPerFrm;
            }

            channelData->anClusterBin0[0] = 0;
            for (int nCluster = 1; nCluster < channelData->nNumCluster; nCluster++)
            {
                channelData->anClusterBin0[nCluster] = channelData->anClusterBin0[nCluster - 1] + channelData->anNumBlocksPerFrmPerCluster[nCluster - 1] * channelData->nMDCTBins;
            }
        }
        else
        {     

            channelData->nNumCluster = 1;
            channelData->anNumBlocksPerFrmPerCluster[0] = 1;
            channelData->anClusterBin0[0] = 0;
        }
    }
    else
    {
        channelData->nWinTypeCurrent = ch0Data->nWinTypeCurrent;
        channelData->bShortWindow = ch0Data->bShortWindow;
        channelData->nMDCTBins = ch0Data->nMDCTBins;
        channelData->nNumCluster = ch0Data->nNumCluster;
        for (int i = 0; i < MAX_NumCluster; i++)
        {
            channelData->anClusterBin0[i] = ch0Data->anClusterBin0[i];
            channelData->anNumBlocksPerFrmPerCluster[i] = ch0Data->anNumBlocksPerFrmPerCluster[i];
        }
    }
}

static void GetCBEdge(decodeFrameInfo* frameInfo, int nCh)
{
    if (frameInfo->error) { return; }

    FrameHeaderInfo* frameHeader = frameInfo->frameHeader;
    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);

    const CriticalBandInfo* criticalBand;
    if (channelData->bShortWindow)
    {
        criticalBand = &CriticalBands[1][frameHeader->nSampleRateIndex];
    }
    else
    {
        criticalBand = &CriticalBands[0][frameHeader->nSampleRateIndex];
    }

    channelData->pnCBEdge[0] = 0;
    for (int i = 0; i < criticalBand->size; i++)
    {
        channelData->pnCBEdge[i + 1] = channelData->pnCBEdge[i] + criticalBand->data[i];
    }
    
    channelData->nCBEdgeLength = criticalBand->size + 1;
}

static void UnpackCodeBooks(decodeFrameInfo* frameInfo, int nCh)
{
    if (frameInfo->error) { return; }

    bitStreamInfo* bitStream = frameInfo->bitStream;
    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);

    const HuffmanStruct* pRunLengthBook;
    const HuffmanStruct* pHSBook;    
    if (channelData->bShortWindow)
    {
        pRunLengthBook = &HuffDec3_32x1;
        pHSBook = &HuffDec5_18x1;
    }
    else
    {
        pRunLengthBook = &HuffDec2_64x1;
        pHSBook = &HuffDec4_18x1;
    }

    for (int nCluster = 0; nCluster < channelData->nNumCluster; nCluster++)
    {
        channelData->anHSNumBands[nCluster] = Unpack(bitStream, 5);
        int nLast = 0;
        for (int nBand = 0; nBand < channelData->anHSNumBands[nCluster]; nBand++)
        {
            int k = HuffDecRecursive(pRunLengthBook, bitStream, Unpack) + nLast + 1;
            channelData->mnHSBandEdge[nCluster][nBand] = k;
            nLast = k;
        }
   }

    for (int nCluster = 0; nCluster < channelData->nNumCluster; nCluster++)
    {
        if (channelData->anHSNumBands[nCluster] > 0)
        {
            int nLast = Unpack(bitStream,4);
            channelData->mnHS[nCluster][0] = nLast;
            for (int nBand = 1; nBand < channelData->anHSNumBands[nCluster]; nBand++)
            {
                int k = HuffmanDecode(pHSBook, bitStream, Unpack);
                if (k > 8)
                {
                    k -= 8;
                }
                else
                {
                    k -= 9;
                }
                k += nLast;
                channelData->mnHS[nCluster][nBand] = k;
                nLast = k;
                if (k < 0 || k > 9)
                {
                    ErrorHandling(frameInfo, DRA_ERROR_INVALID_CODE_BOOK);
                    return;
                }
            }
        }
    }
}

static void GetMaxActCb(decodeFrameInfo* frameInfo, int nCh)
{
    if (frameInfo->error) { return; }

    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);

    for (int nCluster = 0; nCluster < channelData->nNumCluster; nCluster++)
    {
        int nCb = 0;
        int nMaxBand = channelData->anHSNumBands[nCluster];
        if (nMaxBand != 0)
        {
            int nMaxBin = channelData->mnHSBandEdge[nCluster][nMaxBand - 1] * 4;
            nMaxBin = (int)ceil((double)nMaxBin / channelData->anNumBlocksPerFrmPerCluster[nCluster]);
            while (channelData->pnCBEdge[nCb] < nMaxBin)
            {
                nCb++;
                if (nCb >= channelData->nCBEdgeLength)
                {
                    ErrorHandling(frameInfo, DRA_ERROR_INVALID_CB_EDGE);
                    return;
                }
            }
        }
        channelData->anMaxActCb[nCluster] = nCb;
    }
}

static void UnpackQIndex(decodeFrameInfo* frameInfo, int nCh)
{
    if (frameInfo->error) { return; }

    bitStreamInfo* bitStream = frameInfo->bitStream;
    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);

    const HuffmanStruct* pQuotientWidthBook;
    const HuffmanStruct* QIndexBooks[9];
            
    if (channelData->bShortWindow)
    {
        pQuotientWidthBook = &HuffDec9_16x1;
        QIndexBooks[0] = &HuffDec19_81x4;
        QIndexBooks[1] = &HuffDec20_25x2;
        QIndexBooks[2] = &HuffDec21_81x2;
        QIndexBooks[3] = &HuffDec22_289x2;
        QIndexBooks[4] = &HuffDec23_31x1;
        QIndexBooks[5] = &HuffDec24_63x1;
        QIndexBooks[6] = &HuffDec25_127x1;
        QIndexBooks[7] = &HuffDec26_255x1;
        QIndexBooks[8] = &HuffDec27_256x1;
    }
    else
    {
        pQuotientWidthBook = &HuffDec8_16x1;
        QIndexBooks[0] = &HuffDec10_81x4;
        QIndexBooks[1] = &HuffDec11_25x2;
        QIndexBooks[2] = &HuffDec12_81x2;
        QIndexBooks[3] = &HuffDec13_289x2;
        QIndexBooks[4] = &HuffDec14_31x1;
        QIndexBooks[5] = &HuffDec15_63x1;
        QIndexBooks[6] = &HuffDec16_127x1;
        QIndexBooks[7] = &HuffDec17_255x1;
        QIndexBooks[8] = &HuffDec18_256x1;
    }

    int pQuotientWidthBook_nIndex = 0;

    dra_ArrayClear_int (channelData->anQIndex, MDCT_BINS_PER_FRAME);
    
    for (int nCluster = 0; nCluster < channelData->nNumCluster; nCluster++)
    {
        int nStart = channelData->anClusterBin0[nCluster];
        for (int nBand = 0; nBand < channelData->anHSNumBands[nCluster]; nBand++)
        {
            int nEnd = channelData->anClusterBin0[nCluster] + channelData->mnHSBandEdge[nCluster][nBand] * 4;
            int nQSelect = channelData->mnHS[nCluster][nBand];
            if (nQSelect == 0)
            {
                for (int nBin = nStart; nBin < nEnd; nBin++)
                {
                    channelData->anQIndex[nBin] = 0;
                }
            }
            else
            {
                nQSelect--;
                const HuffmanStruct* pQIndexBook = QIndexBooks[nQSelect];
                if (nQSelect == 8)
                {
                    int nMaxIndex = pQIndexBook->DimensionLength - 1;
                    int nCtr = 0;
                    for (int nBin = nStart; nBin < nEnd; nBin++)
                    {
                        int nQIndex = HuffmanDecode(pQIndexBook, bitStream, Unpack);
                        if (nQIndex == nMaxIndex)
                        {
                            nCtr++;
                        }
                        channelData->anQIndex[nBin] = nQIndex;
                    }
                    if (nCtr > 0)
                    {
                        pQuotientWidthBook_nIndex = HuffDecDiff(pQuotientWidthBook, bitStream, Unpack, pQuotientWidthBook_nIndex);
                        int nQuotientWidth =  pQuotientWidthBook_nIndex + 1;
                        for (int nBin = nStart; nBin < nEnd; nBin++)
                        {
                            int nQIndex = channelData->anQIndex[nBin];
                            if (nQIndex == nMaxIndex)
                            {
                                nQIndex *= Unpack(bitStream, nQuotientWidth) + 1;
                                nQIndex += HuffmanDecode(pQIndexBook, bitStream, Unpack);
                                channelData->anQIndex[nBin] = nQIndex;
                            }                            
                        }
                    }
                }
                else
                {
                    int nDim = pQIndexBook->Dimension;
                    if (nDim > 1)
                    {
                        int nNumCodes = pQIndexBook->DimensionLength;      
                        for (int nBin = nStart; nBin < nEnd; nBin += nDim)
                        {
                            int nQIndex = HuffmanDecode(pQIndexBook, bitStream, Unpack);
                            for (int k = 0; k < nDim; k++)
                                {
                                    channelData->anQIndex[nBin + k] = nQIndex % nNumCodes;
                                    nQIndex = nQIndex / nNumCodes;
                                }
                        }
                    }
                    else
                    {
                        for (int nBin = nStart; nBin < nEnd; nBin++)
                        {
                            channelData->anQIndex[nBin] = HuffmanDecode(pQIndexBook, bitStream, Unpack);
                        }
                    }
                }

                if (pQIndexBook->MidThread == true)
                {
                    int nMaxIndex = pQIndexBook->DimensionLength / 2;
                    for (int nBin = nStart; nBin < nEnd; nBin++)
                    {
                        channelData->anQIndex[nBin] -= nMaxIndex;
                    }
                }
                else
                {
                    for (int nBin = nStart; nBin < nEnd; nBin++)
                    {
                        int nQIndex = channelData->anQIndex[nBin];
                        if (nQIndex != 0)
                        {
                            int nSign = Unpack(bitStream, 1);
                            if (nSign == 0)
                            {
                                nQIndex = -nQIndex;
                            }
                        }
                        channelData->anQIndex[nBin] = nQIndex;
                    }
                }
            }
            nStart = nEnd;
        }
    }
}

static void UnpackQStepIndex(decodeFrameInfo* frameInfo, int nCh)
{
    if (frameInfo->error) { return; }

    bitStreamInfo* bitStream = frameInfo->bitStream;
    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);

    const HuffmanStruct* pQStepBook;
    if (channelData->bShortWindow)
    {
        pQStepBook = &HuffDec7_116x1;
    }
    else
    {
        pQStepBook = &HuffDec6_116x1;
    }

    int pQStepBook_nIndex = 0;

    for (int nCluster = 0; nCluster < channelData->nNumCluster; nCluster++)
    {
        for (int nCBIndex = 0; nCBIndex < channelData->anMaxActCb[nCluster]; nCBIndex++)
        {
            pQStepBook_nIndex = HuffDecDiff(pQStepBook, bitStream, Unpack, pQStepBook_nIndex);
            channelData->mnQStepIndex[nCluster][nCBIndex] = pQStepBook_nIndex;
        }
    }
}

static void UnpackSumDff(decodeFrameInfo* frameInfo, int nCh)
{
    if (frameInfo->error) { return; }

    bitStreamInfo* bitStream = frameInfo->bitStream;
    FrameHeaderInfo* frameHeader = frameInfo->frameHeader; 
    ChannelDataInfo* ch0Data = &(frameInfo->allChannels[0]);
    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);

    if (frameHeader->bUseSumDiff == true && (nCh % 2) == 1)
    {
        for (int nCluster = 0; nCluster < channelData->nNumCluster; nCluster++)
        {
            int nMaxCb = max(ch0Data->anMaxActCb[nCluster], channelData->anMaxActCb[nCluster]);
            if (frameHeader->nJicCb > 0)
            {
                nMaxCb = min(frameHeader->nJicCb, nMaxCb);
            }
            if (nMaxCb > 0)
            {
                channelData->anSumDffAllOff[nCluster] = (Unpack(bitStream, 1) == 1);
                if (channelData->anSumDffAllOff[nCluster])
                {
                    for (int nCBIndex = 0; nCBIndex < nMaxCb; nCBIndex++)
                    {
                        channelData->mnSumDffOn[nCluster][nCBIndex] = false;
                    }
                }
                else
                {
                    for (int nCBIndex = 0; nCBIndex < nMaxCb; nCBIndex++)
                    {
                        channelData->mnSumDffOn[nCluster][nCBIndex] = (Unpack(bitStream, 1) == 1);
                    }
                }
            }
        }
    }
}

static void UnpackJicScale(decodeFrameInfo* frameInfo, int nCh)
{
    if (frameInfo->error) { return; }

    bitStreamInfo* bitStream = frameInfo->bitStream;
    FrameHeaderInfo* frameHeader = frameInfo->frameHeader;
    ChannelDataInfo* ch0Data = &(frameInfo->allChannels[0]);
    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);

    if (frameHeader->bUseJIC == true && nCh > 0)
    {
        const HuffmanStruct* pQStepBook;
        if (channelData->bShortWindow)
        {
            pQStepBook = &HuffDec7_116x1;
        }
        else
        {
            pQStepBook = &HuffDec6_116x1;
        }
        
        int pQStepBook_nIndex = 57;

        for (int nCluster = 0; nCluster < channelData->nNumCluster; nCluster++)
        {
            for (int nCBIndex = frameHeader->nJicCb; nCBIndex < ch0Data->anMaxActCb[nCluster]; nCBIndex++)
            {
                pQStepBook_nIndex = HuffDecDiff(pQStepBook, bitStream, Unpack, pQStepBook_nIndex);
                channelData->mnJicStepIndex[nCluster][nCBIndex] = pQStepBook_nIndex;
            }
        }
    }
}

static void UnpackWinSequenceLFE(decodeFrameInfo* frameInfo, int nCh)
{
    if (frameInfo->error) { return; }

    FrameHeaderInfo* frameHeader = frameInfo->frameHeader;
    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);

    if (frameHeader->nNumBlocksPerFrm == 8)
    {
        channelData->nWinTypeCurrent = WIN_LONG_LONG2LONG;
        channelData->bShortWindow = false;
        channelData->nMDCTBins = LONG_MDCT_BINS;
        channelData->nNumCluster = 1;
        channelData->anClusterBin0[0] = 0;
        channelData->anNumBlocksPerFrmPerCluster[0] = 1;
    }
    else
    {
        channelData->nWinTypeCurrent = WIN_SHORT_SHORT2SHORT;
        channelData->bShortWindow = true;
        channelData->nMDCTBins = SHORT_MDCT_BINS;
        channelData->nNumCluster = 1;
        channelData->anClusterBin0[0] = 0;
        channelData->anNumBlocksPerFrmPerCluster[0] = frameHeader->nNumBlocksPerFrm;
    }
}

static void UnpackBitPad(decodeFrameInfo* frameInfo)
{
    if (frameInfo->error) { return; }

    bitStreamInfo* bitStream = frameInfo->bitStream;
    FrameHeaderInfo* frameHeader = frameInfo->frameHeader; 

    int padLength = frameHeader->nDRABits - bitStream->bitIndex;
    if (padLength < 0)
    {
        ErrorHandling(frameInfo, DRA_ERROR_INVALID_FRAME_LENG);
        return;
    }
    Unpack(bitStream, padLength);
}

static void AuxiliaryData(decodeFrameInfo* frameInfo)
{
    if (frameInfo->error) { return; }

    bitStreamInfo* bitStream = frameInfo->bitStream;
    FrameHeaderInfo* frameHeader = frameInfo->frameHeader; 

    if (frameHeader->bAuxData)
    {
        frameHeader->nAuxDataLength = Unpack(bitStream, 8);
        frameHeader->nFrameLength++;

        if (frameHeader->nAuxDataLength == 255)
        {
            frameHeader->nAuxDataLength += Unpack(bitStream, 16) - 2;
        }
        frameHeader->nFrameLength += frameHeader->nAuxDataLength;

        for (int i=0; i<frameHeader->nAuxDataLength; i++)
        {
            Unpack(bitStream, 8);
        }   
    }
}

static void Frame(decodeFrameInfo* frameInfo)
{
    if (frameInfo->error) { return; }

    FrameHeaderInfo* frameHeader = frameInfo->frameHeader;

    FrameHeader(frameInfo);
    if (frameInfo->channelNumber != (frameHeader->nNumNormalCh + frameHeader->nNumLfeCh))
    {
        if (frameInfo->channelNumber != 0)
        {
            free(frameInfo->allChannels);
        }

        frameInfo->channelNumber = frameHeader->nNumNormalCh + frameHeader->nNumLfeCh;
        frameInfo->allChannels = (ChannelDataInfo*)malloc(frameInfo->channelNumber * sizeof(ChannelDataInfo));

        for (int i = 0; i < frameInfo->channelNumber; i++)
        {
            ResetChannel(&(frameInfo->allChannels[i]));
        }
    }

    int nCh = 0;
    for (; nCh < frameHeader->nNumNormalCh; nCh++)
    {
        UnpackWinSequence   (frameInfo, nCh);
        GetCBEdge           (frameInfo, nCh);
        UnpackCodeBooks     (frameInfo, nCh);
        GetMaxActCb         (frameInfo, nCh);
        UnpackQIndex        (frameInfo, nCh);
        UnpackQStepIndex    (frameInfo, nCh);
        UnpackSumDff        (frameInfo, nCh);
        UnpackJicScale      (frameInfo, nCh);
    }
    for (nCh =  frameHeader->nNumNormalCh; nCh < frameInfo->channelNumber; nCh++)
    {
        UnpackWinSequenceLFE(frameInfo, nCh);
        GetCBEdge           (frameInfo, nCh);
        UnpackCodeBooks     (frameInfo, nCh);
        GetMaxActCb         (frameInfo, nCh);
        UnpackQIndex        (frameInfo, nCh);
        UnpackQStepIndex    (frameInfo, nCh);
    }

    UnpackBitPad(frameInfo);

    AuxiliaryData(frameInfo);
}

static void DeQuatlize(decodeFrameInfo* frameInfo, int nCh)
{
    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);
    
    dra_ArrayClear_float(channelData->afBinReconst, MDCT_BINS_PER_FRAME);

    for (int nCluster = 0; nCluster < channelData->nNumCluster; nCluster++)
    {
        int nBin0 = channelData->anClusterBin0[nCluster];
        for (int nCBIndex = 0; nCBIndex < channelData->anMaxActCb[nCluster]; nCBIndex++)
        {
            int nNumBlocks = channelData->anNumBlocksPerFrmPerCluster[nCluster];
            int nStart = nBin0 + nNumBlocks * channelData->pnCBEdge[nCBIndex];
            int nEnd = nBin0 + nNumBlocks * channelData->pnCBEdge[nCBIndex + 1];
            int nQStepSelect = channelData->mnQStepIndex[nCluster][nCBIndex];
            float nStepSize = aunStepSize[nQStepSelect];
            for (int nBin = nStart; nBin < nEnd; nBin++)
            {
                channelData->afBinReconst[nBin] = channelData->anQIndex[nBin] * nStepSize;
            }
        }
    }
}

static void DeJicScale(decodeFrameInfo* frameInfo, int nCh)
{
    FrameHeaderInfo* frameHeader = frameInfo->frameHeader;
    ChannelDataInfo* ch0Data = &(frameInfo->allChannels[0]);
    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);

    if (frameHeader->bUseJIC == true && nCh > 0)
    {
        float rJicScaleBias = 1.0 / aunStepSize[57];    
        for (int nCluster = 0; nCluster < channelData->nNumCluster; nCluster++)
        {
            int nBin0 = channelData->anClusterBin0[nCluster];
            for (int nCBIndex = frameHeader->nJicCb; nCBIndex < ch0Data->anMaxActCb[nCluster]; nCBIndex++)
            {
                int nStart = nBin0 + channelData->anNumBlocksPerFrmPerCluster[nCluster] * channelData->pnCBEdge[nCBIndex];
                int nEnd = nBin0 + channelData->anNumBlocksPerFrmPerCluster[nCluster] * channelData->pnCBEdge[nCBIndex + 1];
                int nQStepSelect = channelData->mnJicStepIndex[nCluster][nCBIndex];
                float fStepSize = aunStepSize[nQStepSelect] * rJicScaleBias;
                for (int nBin = nStart; nBin < nEnd; nBin++)
                {
                    channelData->afBinReconst[nBin] = fStepSize * ch0Data->afBinReconst[nBin];
                }
            }
        }
    }
}

static void DeSumDff(decodeFrameInfo* frameInfo, int nCh)
{
    FrameHeaderInfo* frameHeader = frameInfo->frameHeader;
    ChannelDataInfo* ch0Data = &(frameInfo->allChannels[0]);
    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);

    if (frameHeader->bUseSumDiff == true && (nCh % 2) == 1)
    {
        for (int nCluster = 0; nCluster < channelData->nNumCluster; nCluster++)
        {
            int nBin0 = channelData->anClusterBin0[nCluster];
            int nNumBlocks = channelData->anNumBlocksPerFrmPerCluster[nCluster];
            int nMaxCb = max(channelData->anMaxActCb[nCluster], ch0Data->anMaxActCb[nCluster]);
            if (frameHeader->nJicCb > 0)
            {
                nMaxCb = min(nMaxCb, frameHeader->nJicCb);
            }
            for (int nCBIndex = 0; nCBIndex < nMaxCb; nCBIndex++)
            {
                if (channelData->mnSumDffOn[nCluster][nCBIndex])
                {
                    int nStart = nBin0 + nNumBlocks * channelData->pnCBEdge[nCBIndex];
                    int nEnd = nBin0 + nNumBlocks * channelData->pnCBEdge[nCBIndex + 1];
                    for (int nBin = nStart; nBin < nEnd; nBin++)
                    {
                        float fLeft = ch0Data->afBinReconst[nBin] + channelData->afBinReconst[nBin];
                        float fRight = ch0Data->afBinReconst[nBin] - channelData->afBinReconst[nBin];
                        ch0Data->afBinReconst[nBin] = fLeft;
                        channelData->afBinReconst[nBin] = fRight;
                    }
                }
            }
        }
    }
}

static void Deinterleave(decodeFrameInfo* frameInfo, int nCh)
{
    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);

    dra_ArrayClear_float(channelData->afBinNatural, MDCT_BINS_PER_FRAME);

    if (channelData->bShortWindow)
    {
        int p = 0;

        for (int nCluster = 0; nCluster < channelData->nNumCluster; nCluster++)
        {
            int nBin0 = channelData->anClusterBin0[nCluster];
            int nNumBlocks = channelData->anNumBlocksPerFrmPerCluster[nCluster];
            for (int nBlock = 0; nBlock < nNumBlocks; nBlock++)
            {
                int q = nBin0;
                for (int nBin = 0; nBin < channelData->nMDCTBins; nBin++)
                {
                    channelData->afBinNatural[p] = channelData->afBinReconst[q];
                    q += nNumBlocks;
                    p++;
                }
                nBin0++;
            }
        }
    }
    else
    {
        for (int nBin = 0; nBin < channelData->nMDCTBins; nBin++)
        {
            channelData->afBinNatural[nBin] = channelData->afBinReconst[nBin];
        }
    }
}

static void ReBuildWindow(decodeFrameInfo* frameInfo, int nCh)
{
    FrameHeaderInfo* frameHeader = frameInfo->frameHeader;
    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);
    
    if (channelData->bShortWindow)
    {
        int nBlock;
        if (channelData->nWinTypeCurrent == WIN_SHORT_SHORT2SHORT || channelData->nWinTypeCurrent == WIN_SHORT_SHORT2BRIEF)
        {
            channelData->anWinTypeShort[0] = WIN_SHORT_SHORT2SHORT;
            switch (channelData->nWinTypeLast)
            {
                case WIN_SHORT_BRIEF2BRIEF:
                    channelData->anWinTypeShort[0] = WIN_SHORT_BRIEF2SHORT;
                    break;
                case WIN_LONG_LONG2SHORT:
                case WIN_LONG_SHORT2SHORT:
                case WIN_LONG_BRIEF2SHORT:
                case WIN_SHORT_SHORT2SHORT:
                case WIN_SHORT_BRIEF2SHORT:
                    break;
                default:
                    ErrorHandling(frameInfo, DRA_ERROR_INVALID_WIN_TYPE);
                    break;
            }
        }
        else
        {
            channelData->anWinTypeShort[0] = WIN_SHORT_BRIEF2BRIEF;
            switch (channelData->nWinTypeLast)
            {
                case WIN_SHORT_BRIEF2BRIEF:
                case WIN_SHORT_SHORT2BRIEF:
                case WIN_LONG_LONG2BRIEF:
                case WIN_LONG_BRIEF2BRIEF:
                case WIN_LONG_SHORT2BRIEF:
                    break;
                default:
                    ErrorHandling(frameInfo, DRA_ERROR_INVALID_WIN_TYPE);
                    break;
            }
        }
                
        for (nBlock = 1; nBlock < frameHeader->nNumBlocksPerFrm; nBlock++)
        {
            channelData->anWinTypeShort[nBlock] = WIN_SHORT_SHORT2SHORT;
        }

        nBlock = 0;
        for (int nCluster = 0; nCluster < channelData->nNumCluster - 1; nCluster++)
        {
            nBlock += channelData->anNumBlocksPerFrmPerCluster[nCluster];
            channelData->anWinTypeShort[nBlock] = WIN_SHORT_BRIEF2BRIEF;
        }

        if (channelData->anWinTypeShort[0] == WIN_SHORT_BRIEF2BRIEF)
        {
            if (channelData->anWinTypeShort[1] == WIN_SHORT_SHORT2SHORT)
            {
                channelData->anWinTypeShort[1] = WIN_SHORT_BRIEF2SHORT;
            }
        }

        for (nBlock = 1; nBlock < frameHeader->nNumBlocksPerFrm - 1; nBlock++)
        {
            if (channelData->anWinTypeShort[nBlock] == WIN_SHORT_BRIEF2BRIEF)
            {
                if (channelData->anWinTypeShort[nBlock - 1] == WIN_SHORT_SHORT2SHORT)
                {
                    channelData->anWinTypeShort[nBlock - 1] = WIN_SHORT_SHORT2BRIEF;
                }
                if (channelData->anWinTypeShort[nBlock - 1] == WIN_SHORT_BRIEF2SHORT)
                {
                    channelData->anWinTypeShort[nBlock - 1] = WIN_SHORT_BRIEF2BRIEF;
                }
                if (channelData->anWinTypeShort[nBlock + 1] == WIN_SHORT_SHORT2SHORT)
                {
                    channelData->anWinTypeShort[nBlock + 1] = WIN_SHORT_BRIEF2SHORT;
                }
            }
        }

        switch (channelData->anWinTypeShort[nBlock])
        {
            case WIN_SHORT_BRIEF2BRIEF:
                if (channelData->anWinTypeShort[nBlock - 1] == WIN_SHORT_SHORT2SHORT)
                {
                    channelData->anWinTypeShort[nBlock - 1] = WIN_SHORT_SHORT2BRIEF;
                }
                if (channelData->anWinTypeShort[nBlock - 1] == WIN_SHORT_BRIEF2SHORT)
                {
                    channelData->anWinTypeShort[nBlock - 1] = WIN_SHORT_BRIEF2BRIEF;
                }
                break;
            case WIN_SHORT_SHORT2SHORT:
                if (channelData->nWinTypeCurrent == WIN_SHORT_SHORT2BRIEF || channelData->nWinTypeCurrent == WIN_SHORT_BRIEF2BRIEF)
                {
                    channelData->anWinTypeShort[nBlock] = WIN_SHORT_SHORT2BRIEF;
                }
                break;
            case WIN_SHORT_BRIEF2SHORT:
                if (channelData->nWinTypeCurrent == WIN_SHORT_SHORT2BRIEF || channelData->nWinTypeCurrent == WIN_SHORT_BRIEF2BRIEF)
                {
                    channelData->anWinTypeShort[nBlock] = WIN_SHORT_BRIEF2BRIEF;
                }
                break;
            default:
                ErrorHandling(frameInfo, DRA_ERROR_INVALID_WIN_TYPE);
                break;
        }

        channelData->nWinTypeLast = channelData->anWinTypeShort[nBlock];
    }
    else
    {
        channelData->anWinTypeShort[0] = channelData->nWinTypeCurrent;
        channelData->nWinTypeLast = channelData->anWinTypeShort[0];
    }
}

static void InverseMDCT(decodeFrameInfo* frameInfo, int nCh)
{
    ChannelDataInfo* channelData = &(frameInfo->allChannels[nCh]);

    const DctWindowInfo *dctWindow;

    float* imdctLongSamples     = frameInfo->imdctBuffer->imdctLongSamples;
    float* imdctShortBins       = frameInfo->imdctBuffer->imdctShortBins;
    float* imdctShortSamples    = frameInfo->imdctBuffer->imdctShortSamples;
    float* imdctTotalFrameShort = frameInfo->imdctBuffer->imdctTotalFrameShort;
    draMDCT* mdctShortPlan      = frameInfo->mdctShortPlan;
    draMDCT* mdctLongPlan       = frameInfo->mdctLongPlan;

    if (channelData->bShortWindow)
    {
        int offset = SHORT_SAMPLES_OFFSET;
        for (int i = 0; i < SHORT_BLOCKS_PER_FRAME; i++)
        {
            dra_ArrayCopy_float(channelData->afBinNatural + i*SHORT_MDCT_BINS, imdctShortBins, SHORT_MDCT_BINS);
            dra_MDCT_InvertProcess(mdctShortPlan, imdctShortBins, imdctShortSamples);

            dctWindow = &DctWindows[(int)channelData->anWinTypeShort[i]];
            if (i != 0)
            {                
                for (int j = 0; j < SHORT_MDCT_BINS; j++)
                {
                    imdctTotalFrameShort[offset + j] += imdctShortSamples[j] * dctWindow->coef[j];
                    imdctTotalFrameShort[offset + j + SHORT_MDCT_BINS] = imdctShortSamples[j + SHORT_MDCT_BINS] *  dctWindow->coef[j + SHORT_MDCT_BINS]; 
                }
            }
            else
            {
                for (int j = 0; j < SHORT_IMDCT_SAMPLES; j++)
                {
                    imdctTotalFrameShort[offset + j] = imdctShortSamples[j] * dctWindow->coef[j];
                }
            }
            offset += SHORT_MDCT_BINS;
        }

        for (int i = 0; i < SAMPLES_PER_FRAME; i++)
        {
            channelData->afAudio[i] = channelData->afHistory[i] + imdctTotalFrameShort[i];
            channelData->afHistory[i] = imdctTotalFrameShort[i + SAMPLES_PER_FRAME];
        }
    }
    else
    {
        dra_MDCT_InvertProcess(mdctLongPlan, channelData->afBinNatural, imdctLongSamples);
        
        dctWindow = &DctWindows[(int)channelData->nWinTypeCurrent];
        for (int i = 0; i < SAMPLES_PER_FRAME; i++)
        {
            channelData->afAudio[i] = channelData->afHistory[i] + imdctLongSamples[i] * dctWindow->coef[i]; 
            channelData->afHistory[i] = imdctLongSamples[i + SAMPLES_PER_FRAME] * dctWindow->coef[i + SAMPLES_PER_FRAME];
        }
    }            
}

static void Decode(decodeFrameInfo* frameInfo)
{
    if (frameInfo->error) { return; }
    
    FrameHeaderInfo* frameHeader = frameInfo->frameHeader;

    for (int nCh = 0; nCh < frameHeader->nNumNormalCh; nCh++)
    {
        DeQuatlize      (frameInfo, nCh);
        DeJicScale      (frameInfo, nCh);
        DeSumDff        (frameInfo, nCh);
        Deinterleave    (frameInfo, nCh);
        ReBuildWindow   (frameInfo, nCh);
        InverseMDCT     (frameInfo, nCh);
   }
}

static void DataOut(decodeFrameInfo* frameInfo)
{
    FrameHeaderInfo *frameHeader = frameInfo->frameHeader;
    bitStreamInfo     *bitStream = frameInfo->bitStream;
    auidoStreamInfo *audioStream = frameInfo->audioStream;
    ChannelDataInfo *allChannels = frameInfo->allChannels;

    bitStream->processInputLength  = bitStream->bitIndex / 8;

    if (frameInfo->error) { return; }

    audioStream->validOutputLength = frameHeader->nNumNormalCh * SAMPLES_PER_FRAME;
    if (audioStream->validOutputLength > audioStream->outputDataLength)
    {
        audioStream->validOutputLength = 0;
        return;
    }

    for (int i = 0; i < SAMPLES_PER_FRAME; i++)
    {
        for ( int nCh = 0; nCh < frameHeader->nNumNormalCh; nCh++)
        {
            audioStream->outputData[i * frameHeader->nNumNormalCh + nCh] = (short)(allChannels[nCh].afAudio[i]);
        }
    }
}

static void InitFrameDecoder(decodeFrameInfo* frameInfo)
{
    frameInfo->error = DRA_NO_ERROR;

    frameInfo->bitStream->bitIndex = 0;

    frameInfo->audioStream->validOutputLength = 0;
    frameInfo->bitStream->processInputLength = 0;
    return;    
}

static void Bit_Stream(decodeFrameInfo* frameInfo)
{
    InitFrameDecoder(frameInfo);

    if (Unpack(frameInfo->bitStream, 16) != FrameStart) 
    {
        ErrorHandling(frameInfo, DRA_ERROR_INVALID_FRAME_START);
    }
    else
    {    
        Frame(frameInfo);
        Decode(frameInfo);
        DataOut(frameInfo);
    }

    return;         
}



int dra_Frame_FindSyncHeader(dra_byte *processBuffer, int length)
{           
    int offset = 0;  
    for (; offset <length-1; offset++)
    {
        if (processBuffer[offset] == FrameStart_Byte0 && processBuffer[offset + 1] == FrameStart_Byte1)
        {
            return offset;
        }
    }
    return offset;
}

decodeFrameInfo* dra_Frame_Init (void)
{
    decodeFrameInfo* frame_handle = (decodeFrameInfo*) malloc(sizeof(decodeFrameInfo));
    frame_handle->error = DRA_NO_ERROR;
    frame_handle->bitStream   = (bitStreamInfo*)   malloc(sizeof(bitStreamInfo));
    frame_handle->audioStream = (auidoStreamInfo*) malloc(sizeof(auidoStreamInfo));
    frame_handle->frameHeader = (FrameHeaderInfo*) malloc(sizeof(FrameHeaderInfo));
    frame_handle->allChannels   = NULL;
    frame_handle->channelNumber = 0;
    frame_handle->imdctBuffer   = (imdctBufferInfo*) malloc(sizeof(imdctBufferInfo));
    frame_handle->mdctLongPlan  = dra_MDCT_Init (LONG_IMDCT_SAMPLES);
    frame_handle->mdctShortPlan = dra_MDCT_Init (SHORT_IMDCT_SAMPLES);
    ResetImdcBuffer (frame_handle->imdctBuffer);
    return frame_handle;
}

void dra_Frame_Release(decodeFrameInfo* frame_handle)
{
    free(frame_handle->frameHeader);
    free(frame_handle->bitStream);
    free(frame_handle->audioStream);
    free(frame_handle->imdctBuffer);
    dra_MDCT_Release (frame_handle->mdctLongPlan);
    dra_MDCT_Release (frame_handle->mdctShortPlan);

    ChannelDataInfo* allChannels = frame_handle->allChannels;
    if (allChannels != NULL)
    {
        free(allChannels);
    }

    free(frame_handle);
}

int dra_Frame_Proccess(decodeFrameInfo *frame_handle,    
                        dra_byte        *draStream,       
                        int             *draLength,       
                        dra_short       *audioData,       
                        int             *audioLength)      
{     
    frame_handle->bitStream->inputData       = draStream;
    frame_handle->bitStream->inputDataLength = *draLength;
    
    frame_handle->audioStream->outputData       = audioData;
    frame_handle->audioStream->outputDataLength = *audioLength;
    
    Bit_Stream(frame_handle);
    
    *draLength = frame_handle->bitStream->processInputLength;
    *audioLength = frame_handle->audioStream->validOutputLength;

    return frame_handle->error;
}

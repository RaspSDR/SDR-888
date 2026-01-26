#include "cdrViterbiCodec.h"

#include "cdrUtility.h"

#include <stdlib.h>

#define VITERBI_NO_ERROR  cdr_NO_ERROR
#define VITERBI_INIT_FAIL -1027
#define __DEBUG_VITERBI__ 0

#define ConvolutionBuffer   7
#define InputDelay       (ConvolutionBuffer-1)

#define BufferStates    (1<<ConvolutionBuffer)

#define HistoryStates   (1<<(ConvolutionBuffer-1))

#define Viterbi_Rate  4

static void InitPathBuffer (DeConvolutionPath* path, int length)
{
    path->length = length;
    path->estimatePath = (cdr_byte**)malloc(sizeof(cdr_byte*)*length);
    for (int i = 0; i < length; i++)
    {
        path->estimatePath[i] = (cdr_byte*)malloc(sizeof(cdr_byte)*HistoryStates);
    }
}

static void ReleasePathBuffer (DeConvolutionPath* path)
{
    for (int i = 0; i < path->length; i++)
    {
        free (path->estimatePath[i]);
    }
    free (path->estimatePath);
}

static void ResizePathBuffer (DeConvolutionPath* path, int length)
{
    if (path->length > length)
        return; 
    ReleasePathBuffer (path);
    InitPathBuffer (path, length);
}

const cdr_byte ConvolutionStates [BufferStates][Viterbi_Rate] = {
    { 0, 0, 0, 0, },  { 1, 1, 1, 1, },  { 0, 1, 1, 0, },  { 1, 0, 0, 1, },  { 1, 1, 0, 1, },  { 0, 0, 1, 0, },  { 1, 0, 1, 1, },  { 0, 1, 0, 0, },
    { 1, 1, 0, 1, },  { 0, 0, 1, 0, },  { 1, 0, 1, 1, },  { 0, 1, 0, 0, },  { 0, 0, 0, 0, },  { 1, 1, 1, 1, },  { 0, 1, 1, 0, },  { 1, 0, 0, 1, },
    { 0, 0, 1, 0, },  { 1, 1, 0, 1, },  { 0, 1, 0, 0, },  { 1, 0, 1, 1, },  { 1, 1, 1, 1, },  { 0, 0, 0, 0, },  { 1, 0, 0, 1, },  { 0, 1, 1, 0, },
    { 1, 1, 1, 1, },  { 0, 0, 0, 0, },  { 1, 0, 0, 1, },  { 0, 1, 1, 0, },  { 0, 0, 1, 0, },  { 1, 1, 0, 1, },  { 0, 1, 0, 0, },  { 1, 0, 1, 1, },
    { 1, 0, 0, 1, },  { 0, 1, 1, 0, },  { 1, 1, 1, 1, },  { 0, 0, 0, 0, },  { 0, 1, 0, 0, },  { 1, 0, 1, 1, },  { 0, 0, 1, 0, },  { 1, 1, 0, 1, },
    { 0, 1, 0, 0, },  { 1, 0, 1, 1, },  { 0, 0, 1, 0, },  { 1, 1, 0, 1, },  { 1, 0, 0, 1, },  { 0, 1, 1, 0, },  { 1, 1, 1, 1, },  { 0, 0, 0, 0, },
    { 1, 0, 1, 1, },  { 0, 1, 0, 0, },  { 1, 1, 0, 1, },  { 0, 0, 1, 0, },  { 0, 1, 1, 0, },  { 1, 0, 0, 1, },  { 0, 0, 0, 0, },  { 1, 1, 1, 1, },
    { 0, 1, 1, 0, },  { 1, 0, 0, 1, },  { 0, 0, 0, 0, },  { 1, 1, 1, 1, },  { 1, 0, 1, 1, },  { 0, 1, 0, 0, },  { 1, 1, 0, 1, },  { 0, 0, 1, 0, },
    { 1, 1, 1, 1, },  { 0, 0, 0, 0, },  { 1, 0, 0, 1, },  { 0, 1, 1, 0, },  { 0, 0, 1, 0, },  { 1, 1, 0, 1, },  { 0, 1, 0, 0, },  { 1, 0, 1, 1, },
    { 0, 0, 1, 0, },  { 1, 1, 0, 1, },  { 0, 1, 0, 0, },  { 1, 0, 1, 1, },  { 1, 1, 1, 1, },  { 0, 0, 0, 0, },  { 1, 0, 0, 1, },  { 0, 1, 1, 0, },
    { 1, 1, 0, 1, },  { 0, 0, 1, 0, },  { 1, 0, 1, 1, },  { 0, 1, 0, 0, },  { 0, 0, 0, 0, },  { 1, 1, 1, 1, },  { 0, 1, 1, 0, },  { 1, 0, 0, 1, },
    { 0, 0, 0, 0, },  { 1, 1, 1, 1, },  { 0, 1, 1, 0, },  { 1, 0, 0, 1, },  { 1, 1, 0, 1, },  { 0, 0, 1, 0, },  { 1, 0, 1, 1, },  { 0, 1, 0, 0, },
    { 0, 1, 1, 0, },  { 1, 0, 0, 1, },  { 0, 0, 0, 0, },  { 1, 1, 1, 1, },  { 1, 0, 1, 1, },  { 0, 1, 0, 0, },  { 1, 1, 0, 1, },  { 0, 0, 1, 0, },
    { 1, 0, 1, 1, },  { 0, 1, 0, 0, },  { 1, 1, 0, 1, },  { 0, 0, 1, 0, },  { 0, 1, 1, 0, },  { 1, 0, 0, 1, },  { 0, 0, 0, 0, },  { 1, 1, 1, 1, },
    { 0, 1, 0, 0, },  { 1, 0, 1, 1, },  { 0, 0, 1, 0, },  { 1, 1, 0, 1, },  { 1, 0, 0, 1, },  { 0, 1, 1, 0, },  { 1, 1, 1, 1, },  { 0, 0, 0, 0, },
    { 1, 0, 0, 1, },  { 0, 1, 1, 0, },  { 1, 1, 1, 1, },  { 0, 0, 0, 0, },  { 0, 1, 0, 0, },  { 1, 0, 1, 1, },  { 0, 0, 1, 0, },  { 1, 1, 0, 1, },
};

static inline int GetDecodeLength (int inputLength)
{
    return inputLength / Viterbi_Rate - InputDelay;
}

static inline int GetHistoryLength (int inputLength)
{
    return inputLength / Viterbi_Rate;
}

static int GenerateConvolution_1P(int state, cdr_byte *outputBits)
{    
    outputBits[0] = (cdr_byte)(((state)^           (state>>2)^(state>>3)^           (state>>5)^(state>>6))&(0x01));
    outputBits[1] = (cdr_byte)(((state)^(state>>1)^(state>>2)^(state>>3)^                      (state>>6))&(0x01));
    outputBits[2] = (cdr_byte)(((state)^(state>>1)^                      (state>>4)^           (state>>6))&(0x01));
    outputBits[3] = (cdr_byte)(((state)^           (state>>2)^(state>>3)^           (state>>5)^(state>>6))&(0x01));
    return (state & 0x3F);
}

void GenerateConvolutionStates(cdr_byte** outputBits)
{
    int i;
    for (i = 0; i < BufferStates; i++)
    {
        GenerateConvolution_1P(i, outputBits[i]);
    }

    return;
}

static int GetMinScoreIndex (int* score, int length)
{
    int minIndex = 0;
    int minScore = score[minIndex];
    for (int i = 1; i < length; i++)
    {
        if (score[i] < minScore)
        {
            minIndex = i;
            minScore = score[i];
        }
    }
    return minIndex;
}

static int GetOutputBits (cdr_byte** estimatePath, int lastState, int pathLength,  cdr_byte* outputBits)
{
    int decoderIndex = pathLength - 1;
    for (; decoderIndex >= InputDelay; decoderIndex--)
    {
        cdr_byte temp_outputBit = estimatePath[decoderIndex][lastState];
        outputBits[decoderIndex-InputDelay] = temp_outputBit; 
        lastState = (lastState >> 1) + (temp_outputBit << (InputDelay-1));
    }

    cdr_byte init_Bits [InputDelay];
    for (; decoderIndex >= 0; decoderIndex--)
    {
        cdr_byte temp_outputBit = estimatePath[decoderIndex][lastState];
        init_Bits[decoderIndex] = temp_outputBit;
        if (temp_outputBit != 0)
        {
            return VITERBI_INIT_FAIL;
        }
        lastState = (lastState >> 1) + (temp_outputBit << (InputDelay-1));
    }

    return VITERBI_NO_ERROR;
}

static int CheckResult (cdr_byte* inputBits, cdr_byte* outputBits, int length)
{
    int i;
    int counterError = 0;
    for (i = 0; i < length; i++)
    {
        if (inputBits[i] != outputBits[i])
        {
            counterError++;
        }
    }
    return counterError;
}

static void GenerateCheckBits (cdr_byte* inputBits, int length, cdr_byte* outputBits)
{
    int i;
    cdr_byte* currentOutput = outputBits;
    int state = 0;
    for (i = 0; i < length; i++)
    {
        state = (state << 1) + (inputBits[i] & 0x01);
        state = GenerateConvolution_1P (state, currentOutput);
        currentOutput += Viterbi_Rate;
    }

    for (i=0; i<InputDelay; i++)
    {
        state = (state << 1) + 0x00;
        state = GenerateConvolution_1P (state, currentOutput);
        currentOutput += Viterbi_Rate;
    }
}

static void BufferInputBits (cdr_byteArray* inputBits, cdr_byteArray* bufferBits)
{
    int i;
    for (i = 0; i < inputBits->validLength; i++)
    {
        bufferBits->handle[i] = inputBits->handle[i];
    }
}

int DecodeConvolution(cdr_byteArray* inputBits, DeConvolutionPath* path)
{
    int historyScore [HistoryStates];
    int currentScore [BufferStates];

    int length = GetHistoryLength (inputBits->validLength);
    ResizePathBuffer (path, length);

    for (int decoderIndex = 0; decoderIndex < length; decoderIndex++)
    {
        int processBitStart = decoderIndex * Viterbi_Rate;

        for (int stateIndex = 0; stateIndex < BufferStates; stateIndex++)
        {
            currentScore[stateIndex] = (decoderIndex == 0) ? 0 : historyScore[stateIndex / 2];

            for (int k = 0; k < Viterbi_Rate; k++)
            {
                currentScore[stateIndex] += ConvolutionStates[stateIndex][k] ^ inputBits->handle[processBitStart + k];
            }
        }

        for (int historyIndex = 0; historyIndex < HistoryStates; historyIndex++)
        {
            if (currentScore[historyIndex] > currentScore[historyIndex + HistoryStates])
            {
                path->estimatePath[decoderIndex][historyIndex] = 1;
                historyScore[historyIndex] = currentScore[historyIndex + HistoryStates];
            }
            else
            {
                path->estimatePath[decoderIndex][historyIndex] = 0;
                historyScore[historyIndex] = currentScore[historyIndex];
            }
        }
    }

    int best_state = GetMinScoreIndex(historyScore, HistoryStates);
    
    cdr_byteArray* outputBits = inputBits;

#if __DEBUG_VITERBI__
    cdr_byteArray* originBits = cdr_byteArray_Init(inputBits->validLength);
    cdr_byteArray* checkBits  = cdr_byteArray_Init(inputBits->validLength);
    BufferInputBits (inputBits, originBits);
#endif

    int error = GetOutputBits (path->estimatePath, best_state, length, outputBits->handle);
    outputBits->validLength = GetDecodeLength (inputBits->validLength);

#if __DEBUG_VITERBI__
    GenerateCheckBits (outputBits->handle, outputBits->validLength, checkBits->handle);
    int count = CheckResult (originBits->handle, checkBits->handle, outputBits->validLength);
    cdr_byteArray_Release (originBits);
    cdr_byteArray_Release (checkBits);
#endif

    return error;
}

DeConvolutionPath* GenerateDeConvolutionPath (int length)
{
    DeConvolutionPath* state = (DeConvolutionPath*)malloc(sizeof(DeConvolutionPath));
    InitPathBuffer (state,  GetHistoryLength(length));
    return state;
}

void ReleaseDeConvolutionPath (DeConvolutionPath* state)
{
    ReleasePathBuffer (state);
    free (state);
}

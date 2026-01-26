
#include "draDecoder.h"

#include "draFrame.h"
#include "draUtility.h"

#include <stdlib.h>

#define DRA_NO_ERROR  0
#define DRA_Frame_NOT_READY  20001


#define FRAME_LENGTH 384
#define FRAME_NUMBER 15*4*2       
#define PROCESS_BUFFER_SIZE (FRAME_LENGTH * FRAME_NUMBER)

#define AUDIO_BASE 1024
#define AUDIO_MULTI 2
#define OUTPUT_BUFFER_SIZE (AUDIO_BASE * FRAME_NUMBER * AUDIO_MULTI)


typedef struct decoderHandle
{
    decodeFrameInfo* decodeFrameHandle;

    dra_byteArray*  processBuffer;

    dra_shortArray*  audioBuffer;

} decoderHandle;

draDecoderHandle draDecoder_Init(void)
{
    decoderHandle *handle     = (decoderHandle*) malloc (sizeof(decoderHandle));
    handle->decodeFrameHandle = dra_Frame_Init();
    handle->processBuffer     = dra_byteArray_Init(PROCESS_BUFFER_SIZE);
    handle->audioBuffer       = dra_shortArray_Init(OUTPUT_BUFFER_SIZE);

    return (draDecoderHandle) handle;
}

void draDecoder_Release(draDecoderHandle handle)
{
    decoderHandle* _handle = (decoderHandle*) handle;
    dra_Frame_Release(_handle->decodeFrameHandle);
    dra_byteArray_Release (_handle->processBuffer);
    dra_shortArray_Release(_handle->audioBuffer);
    free(_handle);
    return;
}

int draDecoder_Process(draDecoderHandle handle, unsigned char* inputData, int inputLength)
{
    int error = DRA_Frame_NOT_READY;

    decoderHandle* _handle = (decoderHandle*) handle;
    unsigned char* processBuffer = _handle->processBuffer->handle;
    int processEndPoint = _handle->processBuffer->validLength;
    int remainLength;
    int syncOffset, processLength;
    
    short* audioData = _handle->audioBuffer->handle;
    int audioPosition = 0;
    int audioEndPoint = _handle->audioBuffer->totalSize;
    int audioLength;

    dra_ArrayCopy_byte (inputData, (processBuffer+processEndPoint), inputLength);

    processEndPoint += inputLength;
    remainLength = processEndPoint;

    if (remainLength > FRAME_LENGTH*2)
    {
        do
        {
            syncOffset = dra_Frame_FindSyncHeader(processBuffer, remainLength);
            processBuffer += syncOffset;
            remainLength  -= syncOffset;
            
            if (remainLength > FRAME_LENGTH)
            {
                processLength = remainLength;
                audioLength   = audioEndPoint - audioPosition;
                error = dra_Frame_Proccess( _handle->decodeFrameHandle, 
                                            processBuffer, &processLength,
                                            audioData, &audioLength);
                
                processBuffer += processLength;
                remainLength  -= processLength;

                audioData     += audioLength;
                audioPosition += audioLength;

            }
        } while (remainLength > FRAME_LENGTH);        
        
        dra_ArrayCopy_byte (processBuffer, _handle->processBuffer->handle, remainLength);

        _handle->audioBuffer->validLength = audioPosition;
    }

    _handle->processBuffer->validLength = remainLength;
    return error;
}

int draDecoder_GetMaxInputDataSize (draDecoderHandle handle)
{ 
    decoderHandle* _handle = (decoderHandle*) handle;
    return _handle->processBuffer->totalSize - _handle->processBuffer->validLength;
}

int draDecoder_GetAudioSampleRate (draDecoderHandle handle)
{ 
    decoderHandle* _handle = (decoderHandle*) handle;
    return _handle->decodeFrameHandle->frameHeader->nSampleRate;
}

int draDecoder_GetAudioChannels (draDecoderHandle handle)
{ 
    decoderHandle* _handle = (decoderHandle*) handle;
    return _handle->decodeFrameHandle->frameHeader->nNumNormalCh;
}

short* draDecoder_GetAudioStream (draDecoderHandle handle, int *streamLength)
{
    decoderHandle* _handle = (decoderHandle*) handle;
    *streamLength = _handle->audioBuffer->validLength;
    return _handle->audioBuffer->handle;
}

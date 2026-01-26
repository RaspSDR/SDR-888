#include "draUtility.h"

#include <string.h>
#include <stdlib.h>

dra_byteArray* dra_byteArray_Init(int length)
{
    dra_byteArray *arrayHandle = (dra_byteArray*) malloc (sizeof (dra_byteArray));
    arrayHandle->handle        = (dra_byte*) malloc (sizeof (dra_byte)*length);
    arrayHandle->totalSize     = length;
    arrayHandle->validLength   = 0;
    return arrayHandle;
}

void dra_byteArray_Release(dra_byteArray* arrayHandle)
{
    free (arrayHandle->handle);
    free (arrayHandle);
}

dra_shortArray* dra_shortArray_Init(int length)
{
    dra_shortArray* arrayHandle = (dra_shortArray*) malloc (sizeof (dra_shortArray));
    arrayHandle->handle         = (short*) malloc (sizeof (short)*length);
    arrayHandle->totalSize      = length;
    arrayHandle->validLength    = 0;
    return arrayHandle;
}

void dra_shortArray_Release(dra_shortArray* arrayHandle)
{
    free (arrayHandle->handle);
    free (arrayHandle);
}


void dra_ArrayClear_int(int *array, int length)
{
    memset(array, 0, length * sizeof(int));
}

void dra_ArrayClear_float(float *array, int length)
{
    memset(array, 0, length * sizeof(float));
}

void dra_ArrayCopy_float(float *source, float *destination, int length)
{
    memcpy(destination, source, length * sizeof(float));
}

void dra_ArrayCopy_byte(dra_byte *source, dra_byte *destination, int length)
{
    memcpy(destination, source, length * sizeof(dra_byte));
}

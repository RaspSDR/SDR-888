#ifndef CDR_UTILITY_H
#define CDR_UTILITY_H

#include "draDecoder.h"

dra_byteArray* dra_byteArray_Init(int length);

void dra_byteArray_Release(dra_byteArray* arrayHandle);

dra_shortArray* dra_shortArray_Init(int length);

void dra_shortArray_Release(dra_shortArray* arrayHandle);

void dra_ArrayClear_int (int *array, int length);

void dra_ArrayClear_float(float *array, int length);

void dra_ArrayCopy_float(float *source, float *destination, int length);

void dra_ArrayCopy_byte(dra_byte *source, dra_byte *destination, int length);

#endif
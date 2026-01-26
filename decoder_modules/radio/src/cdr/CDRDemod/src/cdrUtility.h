#ifndef CDR_UTILITY_H
#define CDR_UTILITY_H

#include "cdrDemod.h"

int ErrorHandling (int error, int newErrorCode, int lastError);

int CRC32(const cdr_byte *data, int start, int length);

int CRC8(const cdr_byte *data, int start, int length);

cdr_byteArray* cdr_byteArray_Init(int length);

void cdr_byteArray_Release(cdr_byteArray *arrayHandle);

cdr_complexArray* cdr_complexArray_Init(int length);

void cdr_complexArray_Release(cdr_complexArray *arrayHandle);

cdr_complexFrame* cdr_complexFrame_Init(int totalLength, int numOfLines);

void cdr_complexFrame_Release(cdr_complexFrame* frameHanle);

void BitsToBytes(cdr_byteArray *inputBits, cdr_byteArray *outputBytes);

void BitsToUInt (cdr_byteArray *inputBits, cdr_uintArray *outputUInts);

int BitArrayToInt(cdr_byteArray *bitArray, int start, int length);

void CrossCorrelationAmplitudePeak (const cdr_complex *a, int aLength, const cdr_complex *b, int bLength, float *peak, int *peakIndex);

cdr_complex ComputeDotConjProduct (const cdr_complex *a,  const cdr_complex *b, int length);

float GetComplexPhase(cdr_complex z);

void GetComplexPhaseArray(const cdr_complex *z, int n, float *phaseArray);

void Modulator (cdr_complex *inout, int length, float frequency, float sampleRate, float *phase);


void ArrayCopy_byte(cdr_byte *source, cdr_byte *destination, int length);

void ArrayCopy_complex (cdr_complex* source, cdr_complex* destination, int length);

void ArrayMove_byte(cdr_byte *source, cdr_byte *destination, int length);

void ArrayMove_complex (cdr_complex *source, cdr_complex *destination, int length);

void ArrraySetZero_complex(cdr_complex *array, int length);

void ArrraySetZero_int(int *array, int length);

void ArrrayMulti_complex(cdr_complex *a, cdr_complex *b, cdr_complex *c, int length);

void ArrrayAmplitudePeak_complex (const cdr_complex *a, int length, float *peak, int *peakIndex);

void ArrrayConjunction_complex (cdr_complex *a, int length);

#endif
#include "cdrModulation.h"

#include <stdlib.h>
#include <math.h>

static int FindNearestIndex (cdr_complex symbol, const cdr_complex* constellationIQ, int constellationSize)
{
    int min_index = 0;
    float min_amp_error = 100.0f;
    float amp_error;

    for (int i =0; i< constellationSize; i++)
    {
        amp_error = cabsf (symbol - constellationIQ[i]);
        if (amp_error < min_amp_error)
        {
            min_amp_error = amp_error;
            min_index = i;
        }
    }
    return min_index;
}

void DeConstellation(cdr_complexArray* symbols, cdr_byteArray* bits, const ConstellationInfo* constellation)
{
    int i;
    int ampMinIndex;
    int bitIndex = 0;
    int constellationSize = 2 << (constellation->bitsPerSymbol);
    for (i = 0; i < symbols->validLength; i++)
    {
        ampMinIndex = FindNearestIndex (symbols->handle[i], constellation->constellationIQ, constellationSize);
        for (int j=0; j<constellation->bitsPerSymbol; j++ )
        {
            bits->handle[bitIndex] = (cdr_byte)(ampMinIndex & 0b0001);
            ampMinIndex = ampMinIndex >> 1;
            bitIndex++;
        }
    }

    bits->validLength = bitIndex;
    
    return;
}

static void GetIQIndex(int data, int* iIndex, int* qIndex, int bitLength)
{
    int i;

    if (bitLength <= 0) { bitLength = 32; }

    iIndex = 0;
    qIndex = 0;
    bitLength /= 2;

    for (i = 0; i < bitLength; i++)
    {
        iIndex += (cdr_byte)( (data & 0x01) << i);
        qIndex += (cdr_byte)(((data & 0x02) >> 1) << i);
        data >>= 2;
    }
}

cdr_complex* GenerateConstellation(float* amplitude, int length, double scale, int *bitsPerSymbol)
{
    int i;
    int iIndex=0;
    int qIndex=0;

    cdr_complex* constellationIQ;
    int constellationLength;
    
    constellationLength = length * length;
    *bitsPerSymbol = log2(constellationLength);
        
    constellationIQ = (cdr_complex*) malloc (sizeof (cdr_complex) * constellationLength);    
    for (i = 0; i < constellationLength; i++)
    {
        GetIQIndex(i, &iIndex, &qIndex, *bitsPerSymbol);
        constellationIQ[i] = (amplitude[iIndex]*scale) + (amplitude[qIndex]*scale)*I;
    }

    return constellationIQ;
}

cdr_complex* GeneratePilotQPSKConstellation(void)
{
    float amp [] = { 1.0f, -1.0f };
    int bitsPerSymbol; 
    return GenerateConstellation(amp, 2, 1, &bitsPerSymbol);
}

cdr_complex* GenerateQPSKConstellation(void)
{
    float amp [] = { 1.0f, -1.0f };
    int bitsPerSymbol;
    return GenerateConstellation(amp, 2, (1.0 / sqrt(2.0)), &bitsPerSymbol);    
}

cdr_complex* Generate16QAMConstellation(void)
{
    float amp [] = { 3.0f, -3.0f, 1.0f, -1.0f };
    int bitsPerSymbol;
    return GenerateConstellation(amp, 4, (1.0 / sqrt(10)), &bitsPerSymbol);
}

cdr_complex* Generate16QAMAlpha2Constellation(void)
{
    float amp [] = { 4.0f, -4.0f, 2.0f, -2.0f };
    int bitsPerSymbol;
    return GenerateConstellation(amp, 4, (1.0 / sqrt(20)), &bitsPerSymbol);
}

cdr_complex* Generate16QAMAlpha4Constellation(void)
{
    float amp [] = { 6.0f, -6.0f, 4.0f, -4.0f };
    int bitsPerSymbol;
    return GenerateConstellation(amp, 4, (1.0 / sqrt(52)), &bitsPerSymbol);
}

cdr_complex* Generate64QAMConstellation(void)
{
    float amp [] = { 7.0f, -7.0f, 1.0f, -1.0f, 5.0f, -5.0f, 3.0f, -3.0f };
    int bitsPerSymbol;
    return GenerateConstellation(amp, 8, sqrt(42), &bitsPerSymbol);
}

cdr_complex* Generate64QAMAlpha2Constellation(void)
{
    float amp [] = { 8.0f, -8.0f, 2.0f, -2.0f, 6.0f, -6.0f, 4.0f, -4.0f };
    int bitsPerSymbol;
    return GenerateConstellation(amp, 8, sqrt(60), &bitsPerSymbol);
}

cdr_complex* Generate64QAMAlpha4Constellation(void)
{
    float amp [] = { 10.0f, -10.0f, 4.0f, -4.0f, 8.0f, -8.0f, 6.0f, -6.0f };
    int bitsPerSymbol;
    return GenerateConstellation(amp, 8, sqrt(108), &bitsPerSymbol);
}


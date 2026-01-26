#ifndef CDR_VITERBI_H 
#define CDR_VITERBI_H  

#include <cdrDemod.h>

typedef struct DeConvolutionPath
{
    int length;
    cdr_byte** estimatePath;
} DeConvolutionPath;

void GenerateConvolutionStates(cdr_byte** outputBits);

DeConvolutionPath* GenerateDeConvolutionPath (int length);

void ReleaseDeConvolutionPath (DeConvolutionPath* state);

int DecodeConvolution(cdr_byteArray* inputBits, DeConvolutionPath* path);

#endif
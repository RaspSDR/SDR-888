#include "draMDCT.h"
#include <fftw3.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI  
#define M_PI 3.14159265358979323846  
#endif

enum fftw_direction
{
	Forward = -1,
	Backward = 1
};

enum fftw_flags
{
	Measure = 0,
	DestroyInput = 1,
	Unaligned = 2,
	ConserveMemory = 4,
	Exhaustive = 8,
	PreserveInput = 16,
	Patient = 32,
    Estimate = 64
};

draMDCT* dra_MDCT_Init(int N)
{
    double alpha, omiga, scale;
    int n;
    double add, mul, fma;
    
    
    if ( (N % 4) != 0)
    {
    }

    draMDCT* handle = malloc (sizeof(draMDCT));

    handle->mdctSize = N;
    handle->fftSize  = handle->mdctSize / 4;

    handle->arrayBoard  = handle->fftSize;
    handle->arrayBoard2 = handle->arrayBoard * 2;
    handle->arrayBoard3 = handle->arrayBoard * 3;
    handle->arrayBoard4 = handle->arrayBoard * 4;
    handle->arrayBoard5 = handle->arrayBoard * 5;

    handle->twiddle = fftwf_malloc(sizeof(float) * 2 * handle->fftSize);
    alpha = 2 * M_PI / (8 * handle->mdctSize);
    omiga = 2 * M_PI / handle->mdctSize;
    scale = sqrt(32768.0);

    for (n = 0; n < handle->fftSize; n++)
    {
        handle->twiddle[2 * n + 0] = (float)(scale * cos(omiga * n + alpha));
        handle->twiddle[2 * n + 1] = (float)(scale * sin(omiga * n + alpha));
    }
    
    void* fftIn_buffer   = fftwf_malloc(sizeof(float) * 2 * handle->fftSize);
    void* fftOut_buffer  = fftwf_malloc(sizeof(float) * 2 * handle->fftSize);
    handle->fftIn   = fftIn_buffer;
    handle->fftOut  = fftOut_buffer;
    handle->fftPlan = fftwf_plan_dft_1d(handle->fftSize, fftIn_buffer, fftOut_buffer, Forward, Measure);

    add = 0;
    mul = 0;
    fma = 0;
    fftwf_flops(handle->fftPlan, &add, &mul, &fma);
    handle->flops =  add + mul + 2 * fma;

    return handle;
}

void dra_MDCT_Release(draMDCT* handle)
{
    fftwf_destroy_plan(handle->fftPlan);
    fftwf_free(handle->fftIn);
    fftwf_free(handle->fftOut);
    fftwf_free(handle->twiddle);
    free(handle);
}

void dra_MDCT_ForwardProccess(draMDCT* handle, float* timeInput, float* mdctOutput)
{
    int n;
    float r0, i0, c, s;

    for (n = 0; n < handle->arrayBoard; n += 2)
    {
        r0 = timeInput[handle->arrayBoard3 - 1 - n] + timeInput[handle->arrayBoard3 + n];
        i0 = timeInput[handle->arrayBoard  + n]     - timeInput[handle->arrayBoard  - 1 - n];

        c = handle->twiddle[n];
        s = handle->twiddle[n+1];

        handle->fftIn[n]   = r0 * c + i0 * s;
        handle->fftIn[n+1] = i0 * c - r0 * s;
    }

    for (; n < handle->arrayBoard2; n += 2)
    {
        r0 = timeInput[handle->arrayBoard3 - 1 - n] - timeInput[-handle->arrayBoard  + n];
        i0 = timeInput[handle->arrayBoard  + n]     + timeInput[ handle->arrayBoard5 - 1 - n];

        c = handle->twiddle[n];
        s = handle->twiddle[n+1];

        handle->fftIn[n]     = r0 * c + i0 * s;
        handle->fftIn[n + 1] = i0 * c - r0 * s;
    }

    fftwf_execute(handle->fftPlan);

    for (n = 0; n < handle->arrayBoard2; n += 2)
    {
        r0 = handle->fftOut[n];
        i0 = handle->fftOut[n+1];

        c = handle->twiddle[n];
        s = handle->twiddle[n + 1];

        mdctOutput[n]                           = -r0 * c - i0 * s;
        mdctOutput[handle->arrayBoard2 - 1 - n] = -r0 * s + i0 * c;
    }
}

void dra_MDCT_InvertProcess(draMDCT* handle, float* mdctInput, float* timeOutput)
{
    int  n;
    float r0, i0, r1, i1, c, s;

    for (n = 0; n < handle->arrayBoard2; n += 2)
    {
        r0 = mdctInput[n];
        i0 = mdctInput[handle->arrayBoard2 - 1 - n];

        c = handle->twiddle[n];
        s = handle->twiddle[n+1];

        handle->fftIn[n]     = -2 * (i0 * s + r0 * c);
        handle->fftIn[n + 1] = -2 * (i0 * c - r0 * s);
    }

    fftwf_execute(handle->fftPlan);

    for (n = 0; n < handle->arrayBoard; n += 2)
    {
        r0 = handle->fftOut[n];
        i0 = handle->fftOut[n+1];

        c = handle->twiddle[n];
        s = handle->twiddle[n+1];

        r1 = r0 * c + i0 * s;
        i1 = r0 * s - i0 * c;

        timeOutput[handle->arrayBoard3 - 1 - n] =  r1;
        timeOutput[handle->arrayBoard3 + n]     =  r1;
        timeOutput[handle->arrayBoard  + n]     =  i1;
        timeOutput[handle->arrayBoard  - 1 - n] = -i1;
    }

    for (; n < handle->arrayBoard2; n += 2)
    {
        r0 = handle->fftOut[n];
        i0 = handle->fftOut[n+1];

        c = handle->twiddle[n];
        s = handle->twiddle[n+1];

        r1 = r0 * c + i0 * s;
        i1 = r0 * s - i0 * c;

        timeOutput[ handle->arrayBoard3 - 1 - n] =  r1;
        timeOutput[-handle->arrayBoard  + n]     = -r1;
        timeOutput[ handle->arrayBoard  + n]     =  i1;
        timeOutput[ handle->arrayBoard5 - 1 - n] =  i1;
    }
}


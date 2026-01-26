#include "cdrFFTW.h"

#include <fftw3.h>
#include <stdlib.h>

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

cdrFFTW* cdr_FFTW_Init(int N, int isForward)
{
    double add, mul, fma;

    cdrFFTW* handle = malloc (sizeof(cdrFFTW));

    handle->fftSize  = N;

    if (isForward)
        handle->sign = Forward;
    else
        handle->sign = Backward;

    void* fftIn_buffer   = fftwf_malloc(sizeof(cdr_complex) * handle->fftSize);
    void* fftOut_buffer  = fftwf_malloc(sizeof(cdr_complex) * handle->fftSize);
    handle->fftIn   = fftIn_buffer;
    handle->fftOut  = fftOut_buffer;
    handle->fftPlan = fftwf_plan_dft_1d(handle->fftSize, fftIn_buffer, fftOut_buffer, handle->sign, Measure);

    add = 0;
    mul = 0;
    fma = 0;
    fftwf_flops(handle->fftPlan, &add, &mul, &fma);
    handle->flops =  add + mul + 2 * fma;

    return handle;
}

void cdr_FFTW_Release(cdrFFTW* handle)
{
    fftwf_destroy_plan(handle->fftPlan);
    fftwf_free(handle->fftIn);
    fftwf_free(handle->fftOut);
    free(handle);
}

void cdr_FFTW_Proccess(cdrFFTW* handle, cdr_complex* input, cdr_complex* output)
{
    int i;
    for (i = 0; i < handle->fftSize; i++)
    {
        handle->fftIn[i] = input [i];
    }

    fftwf_execute(handle->fftPlan);

    for (i = 0; i < handle->fftSize; i++)
    {
        output[i] = handle->fftOut[i];
    }
}

void cdr_FFTW_ProccessShifted(cdrFFTW* handle, cdr_complex* input, cdr_complex* output)
{
    int i;
    
    int shifted_size = handle->fftSize / 2;

    if (handle->sign == Forward)
    {
        for (i = 0; i < handle->fftSize; i++)
        {
            handle->fftIn[i] = input [i];
        }
    }
    else
    {
        for (i = 0; i < handle->fftSize; i++)
        {
            handle->fftIn[i] = input [(i + shifted_size) % handle->fftSize];
        }
    }

    fftwf_execute(handle->fftPlan);

    
    if (handle->sign == Forward)
    {
        for (i = 0; i < handle->fftSize; i++)
        {
            output[i] = handle->fftOut[(i + shifted_size) % handle->fftSize];
        }
    }
    else
    {
        for (i = 0; i < handle->fftSize; i++)
        {
            output[i] = handle->fftOut[i];
        }
    }
}



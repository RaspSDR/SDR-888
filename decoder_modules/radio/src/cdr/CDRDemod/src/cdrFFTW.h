#ifndef CDR_FFTW_H
#define CDR_FFTW_H

#include <cdrDemod.h>

typedef struct cdrFFTW {
    cdr_complex *fftIn;           
    cdr_complex *fftOut;          
    void        *fftPlan;        
    int         fftSize;
    int         sign;
    double      flops;
} cdrFFTW;

cdrFFTW* cdr_FFTW_Init(int N, int isForward);

void cdr_FFTW_Release(cdrFFTW *handle);

void cdr_FFTW_Proccess(cdrFFTW *handle, cdr_complex *input, cdr_complex *output);

void cdr_FFTW_ProccessShifted(cdrFFTW *handle, cdr_complex *input, cdr_complex *output);

#endif
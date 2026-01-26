#ifndef DRA_MDCT_H
#define DRA_MDCT_H

typedef struct draMDCT {
    int arrayBoard;
    int arrayBoard2;
    int arrayBoard3;
    int arrayBoard4;
    int arrayBoard5;
    float* twiddle;        
    float* fftIn;           
    float* fftOut;          
    void* fftPlan;         
    int mdctSize;             
    int fftSize;
    double flops;
} draMDCT;

draMDCT* dra_MDCT_Init(int N);

void dra_MDCT_Release(draMDCT* handle);

void dra_MDCT_ForwardProccess(draMDCT* handle, float* timeInput, float* mdctOutput);

void dra_MDCT_InvertProcess(draMDCT* handle, float* mdctInput, float* timeOutput);

#endif
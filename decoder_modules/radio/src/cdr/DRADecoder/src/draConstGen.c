#include "draConst.h"
#include <stdio.h>  
#include <math.h>

#ifndef M_PI  
#define M_PI 3.14159265358979323846  
#endif

void GenerateQuantizationStepSize(void)
 {
    double aunStepSize_Test [QuantizationStepLength];
    for (int i = 0; i < QuantizationStepLength; i++)
    {
        aunStepSize_Test[i] = (double)QuantizationStep[i] / QuantizationMaxStep;
    }

    printf(" const float aunStepSize [116] = {");
    for (int i = 0; i < QuantizationStepLength; i++)
    {
        if (i % 5 == 0)  printf("\n\t");
        printf("%.16e", aunStepSize_Test[i]);
    }
    printf(" };");
 }

int GenerateLongWindowLong2Long(double* window)
{
    int windowSize = 2048;
    for (int i = 0; i < windowSize; i++)
    {
        window[i] = sin((i + 0.5) / 2048 * M_PI);
    }
    return windowSize;
}

int GenerateLongWindowLong2Short(double* window)
{
    int windowSize = 2048;
    for (int i = 0; i < 1024; i++)
    {
        window[i] = sin((i + 0.5) / 2048 * M_PI);
    }
    for (int i = 1024; i < 1472; i++)
    {
        window[i] = 1;
    }
    for (int i = 1472; i < 2048; i++)
    {
        window[i] = sin((i - 1344 + 0.5) / 256 * M_PI);
    }
    return windowSize;
}

int GenerateLongWindowShort2Long(double* window)
{
    int windowSize = 2048;
    int i = 0;
    for (; i < 448; i++)
    {
        window[i] = 0;
    }
    for (; i < 576; i++)
    {
        window[i] = sin((i - 448 + 0.5) / 256 * M_PI);
    }
    for (; i < 1024; i++)
    {
        window[i] = 1;
    }
    for (; i < 2048; i++)
    {
        window[i] = sin((i + 0.5) / 2048 * M_PI);
    }
    return windowSize;
}

int GenerateLongWindowShort2Short(double* window)
{
    int windowSize = 2048;
    int i = 0;
    for (; i < 448; i++)
    {
        window[i] = 0;
    }
    for (; i < 576; i++)
    {
        window[i] = sin((i - 448 + 0.5) / 256 * M_PI);
    }
    for (; i < 1472; i++)
    {
        window[i] = 1;
    }
    for (; i < 1600; i++)
    {
        window[i] = sin((i - 1344 + 0.5) / 256 * M_PI);
    }
    for (; i < 2048; i++)
    {
        window[i] = 0;
    }
    return windowSize;
}

int GenerateShortWindowShort2Short(double* window)
{
    int windowSize = 256;
    int i = 0;
    for (; i < 256; i++)
    {
        window[i] = sin((i + 0.5) / 256 * M_PI);
    }
    return windowSize;
}

int GenerateShortWindowBrief2Brief(double* window)
{
    int windowSize = 256;
    int i = 0;
    for (; i < 48; i++)
    {
        window[i] = 0;
    }
    for (; i < 80; i++)
    {
        window[i] = sin((i - 48 + 0.5) / 64 * M_PI);
    }
    for (; i < 176; i++)
    {
        window[i] = 1;
    }
    for (; i < 208; i++)
    {
        window[i] = sin((i - 144 + 0.5) / 64 * M_PI);
    }
    for (; i < 256; i++)
    {
        window[i] = 0;
    }
    return windowSize;
}

int GenerateLongWindowLong2Brief(double* window)
{
    int windowSize = 2048;
    int i = 0;
    for (; i < 1024; i++)
    {
        window[i] = sin((i + 0.5) / 2048 * M_PI);
    }
    for (; i < 1520; i++)
    {
        window[i] = 1;
    }
    for (; i < 1552; i++)
    {
        window[i] = sin((i - 1488 + 0.5) / 64 * M_PI);
    }
    for (; i < 2048; i++)
    {
        window[i] = 0;
    }
    return windowSize;
}

int GenerateLongWindowBrief2Long(double* window)
{
    int windowSize = 2048;
    int i = 0;
    for (; i < 496; i++)
    {
        window[i] = 0;
    }
    for (; i < 528; i++)
    {
        window[i] = sin((i - 496 + 0.5) / 64 * M_PI);
    }
    for (; i < 1024; i++)
    {
        window[i] = 1;
    }
    for (; i < 2048; i++)
    {
        window[i] = sin((i + 0.5) / 2048 * M_PI);
    }
    return windowSize;
}

int GenerateLongWindowBrief2Brief(double* window)
{
    int windowSize = 2048;
    int i = 0;
    for (; i < 496; i++)
    {
        window[i] = 0;
    }
    for (; i < 528; i++)
    {
        window[i] = sin((i - 496 + 0.5) / 64 * M_PI);
    }
    for (; i < 1520; i++)
    {
        window[i] = 1;
    }
    for (; i < 1552; i++)
    {
        window[i] = sin((i - 1488 + 0.5) / 64 * M_PI);
    }
    for (; i < 2048; i++)
    {
        window[i] = 0;
    }
    return windowSize;
}

int GenerateLongWindowShort2Brief(double* window)
 {
    int windowSize = 2048;
    int i = 0;
    for (; i < 448; i++)
    {
        window[i] = 0;
    }
    for (; i < 576; i++)
    {
        window[i] = sin((i - 448 + 0.5) / 256 * M_PI);
    }
    for (; i < 1520; i++)
    {
        window[i] = 1;
    }
    for (; i < 1552; i++)
    {
        window[i] = sin((i - 1488 + 0.5) / 64 * M_PI);
    }
    for (; i < 2048; i++)
    {
        window[i] = 0;
    }
    return windowSize;
}

int GenerateLongWindowBrief2Short(double* window)
{
    int windowSize = 2048;
    int i = 0;
    for (; i < 496; i++)
    {
        window[i] = 0;
    }
    for (; i < 528; i++)
    {
        window[i] = sin((i - 496 + 0.5) / 64 * M_PI);
    }
    for (; i < 1472; i++)
    {
        window[i] = 1;
    }
    for (; i < 1600; i++)
    {
        window[i] = sin((i - 1344 + 0.5) / 256 * M_PI);
    }
    for (; i < 2048; i++)
    {
        window[i] = 0;
    }
    return windowSize;
}

int GenerateShortWindowShort2Brief(double* window)
{
    int windowSize = 256;
    int i = 0;
    for (; i < 128; i++)
    {
        window[i] = sin((i + 0.5) / 256 * M_PI);
    }
    for (; i < 176; i++)
    {
        window[i] = 1;
    }
    for (; i < 208; i++)
    {
        window[i] = sin((i - 144 + 0.5) / 64 * M_PI);
    }
    for (; i < 256; i++)
    {
        window[i] = 0;
    }
    return windowSize;
}

int GenerateShortWindowBrief2Short(double* window)
{
    int windowSize = 256; 
    int i = 0;
    for (; i < 48; i++)
    {
        window[i] = 0;
    }
    for (; i < 80; i++)
    {
        window[i] = sin((i - 48 + 0.5) / 64 * M_PI);
    }
    for (; i < 128; i++)
    {
        window[i] = 1;
    }
    for (; i < 256; i++)
    {
        window[i] = sin((i + 0.5) / 256 * M_PI);
    }
    return windowSize;
}

void GenerateWindows(void)
{
    double gen_Windows[13][2048]; 
    int windowSize[13];
    windowSize[0]  = GenerateLongWindowLong2Long   (gen_Windows[0]);
    windowSize[1]  = GenerateLongWindowLong2Short  (gen_Windows[1]);
    windowSize[2]  = GenerateLongWindowShort2Long  (gen_Windows[2]);
    windowSize[3]  = GenerateLongWindowShort2Short (gen_Windows[3]);
    windowSize[4]  = GenerateLongWindowLong2Brief  (gen_Windows[4]);
    windowSize[5]  = GenerateLongWindowBrief2Long  (gen_Windows[5]);
    windowSize[6]  = GenerateLongWindowBrief2Brief (gen_Windows[6]);
    windowSize[7]  = GenerateLongWindowShort2Brief (gen_Windows[7]);
    windowSize[8]  = GenerateLongWindowBrief2Short (gen_Windows[8]);
    windowSize[9]  = GenerateShortWindowShort2Short(gen_Windows[9]);
    windowSize[10] = GenerateShortWindowShort2Brief(gen_Windows[10]);
    windowSize[11] = GenerateShortWindowBrief2Brief(gen_Windows[11]);
    windowSize[12] = GenerateShortWindowBrief2Short(gen_Windows[12]);

    printf("double* Windows[13] = {\n");
        
    printf("\t(double[]){");    
    for (int i = 0; i < 13; i++) 
    { 
        for (int j = 0; j < windowSize[i]; j++) 
        {
            printf("\t(double[]){");
            if (j % 5 == 0 && j != 0)
            {
                printf("\n\t");
            }
            printf("%.16E, ", gen_Windows[i][j]);  
        }  
  
        printf("\n\t},\n");  
    }

    printf("\t };");
}


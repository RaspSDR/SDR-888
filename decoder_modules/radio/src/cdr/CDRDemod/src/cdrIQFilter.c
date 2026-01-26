#include "cdrIQFilter.h"

#include "cdrUtility.h"
#include <stdlib.h>

#define FILTER_NO_ERROR  cdr_NO_ERROR

static cdr_complex Push (firFilter* filter, cdr_complex input)
{    
    filter->histroyCircle [filter->circleStart] = input;    
    filter->circleStart = (filter->circleStart + 1) % filter->filterLength;  

    cdr_complex sum = cdr_complex_Zero;
    for (int i=1; i<filter->filterLength; i++)
    {
        sum += filter->coe[i] * filter->histroyCircle[(i+filter->circleStart) % filter->filterLength];
    }
    return sum;
}

#if _USE_Liquid_
static cdr_complex Push_2 (firFilter* filter, cdr_complex input)
{
    int error;
    cdr_complex output;
    error = firfilt_crcf_execute_one(filter->liquid_FirFilter, (liquid_float_complex)input, &output);
    return output;
}
#endif

static cdr_complex Push_1 (firFilter* filter, cdr_complex input)
{    
    filter->histroyCircle [filter->circleStart] = input;    
    filter->circleStart = (filter->circleStart + 1) & filter->historyMask;
    if (filter->circleStart == 0)
    {
        int mid = filter->historyLength >> 1;
        ArrayCopy_complex (filter->histroyCircle + mid, filter->histroyCircle, mid);
        filter->circleStart = mid;
    }

    cdr_complex sum = cdr_complex_Zero;
    int historyIndex = filter->circleStart - filter->filterLength;
    for (int i=0; i<filter->filterLength; i++)
    {
        sum += filter->coe[i] * filter->histroyCircle[historyIndex];
        historyIndex++;
    }
    return sum;
}

static void IQFilter_ResetBuffer(firFilter* handle, enum SpectrumType spectrumType)
{
    int filterLength, historyLength, historyMask;
    float* coe;
    switch (spectrumType)
    {
        case SpecMode1:
            coe = Coefficients_Spectrum1;
            filterLength  = LENGTH_Spectrum1;
            historyLength = (MASK_Spectrum1+1);
            historyMask   = MASK_Spectrum1;
            break;
        case SpecMode2:
            coe = Coefficients_Spectrum2;
            filterLength  = LENGTH_Spectrum2;
            historyLength = (MASK_Spectrum2+1);
            historyMask   = MASK_Spectrum2;
            break; 
        case SpecMode10:
            coe = Coefficients_Spectrum10;
            filterLength  = LENGTH_Spectrum10;
            historyLength = (MASK_Spectrum10+1);
            historyMask   = MASK_Spectrum10;
            break;
        case SpecMode22:
            coe = Coefficients_Spectrum22;
            filterLength  = LENGTH_Spectrum22;
            historyLength = (MASK_Spectrum22+1);
            historyMask   = MASK_Spectrum22;
            break;  
        case SpecMode23:
            coe = Coefficients_Spectrum23;
            filterLength  = LENGTH_Spectrum23;
            historyLength = (MASK_Spectrum23+1);
            historyMask   = MASK_Spectrum23;
            break;  
        case SpecMode9:
        default:
            coe = Coefficients_Spectrum9;
            filterLength  = LENGTH_Spectrum9;
            historyLength = (MASK_Spectrum9+1);
            historyMask   = MASK_Spectrum9;
            break;
    }

    handle->coe           = coe;
    handle->filterLength  = filterLength;
    handle->historyMask   = historyMask;
    if (handle->historyLength != historyLength)
    {
        free (handle->histroyCircle);
        handle->historyLength = historyLength;
        handle->histroyCircle = (cdr_complex*) malloc (sizeof(cdr_complex) * (historyLength));
    }

#if _USE_Liquid_
    firfilt_crcf_destroy(handle->liquid_FirFilter);
    handle->liquid_FirFilter = firfilt_crcf_create(coe, filterLength);
#endif

    IQFilter_Reset (handle);

    return;
}

firFilter* IQFilter_Init (void)
{
    firFilter *handle = (firFilter*) malloc (sizeof(firFilter));

    handle->coe           = Coefficients_Spectrum9;
    handle->filterLength  = LENGTH_Spectrum9;
    handle->histroyCircle = (cdr_complex*) malloc (sizeof(cdr_complex) * (MASK_Spectrum9+1));
    handle->historyLength = (MASK_Spectrum9+1);
    handle->historyMask   = MASK_Spectrum9;

#if _USE_Liquid_
    handle->liquid_FirFilter = firfilt_crcf_create(Coefficients_Spectrum9, LENGTH_Spectrum9);
#endif
    IQFilter_Reset (handle);

    return handle;
}

void IQFilter_Release (firFilter* handle)
{
    free (handle->histroyCircle);

#if _USE_Liquid_
    firfilt_crcf_destroy(handle->liquid_FirFilter);
#endif

    free (handle);
    return;
}

void IQFilter_Reset (firFilter* handle)
{
    ArrraySetZero_complex (handle->histroyCircle, handle->historyLength);
    handle->circleStart = handle->historyLength >> 1;
}

int IQFilter_Process (firFilter* handle, cdr_complex* input, cdr_complex* output, int length)
{
    for (int i=0; i<length; i++)
    {
#if _USE_Liquid_
        output[i] = Push_2 (handle, input[i]);
#else
        output[i] = Push_1 (handle, input[i]);
#endif
    }

    return FILTER_NO_ERROR;
}

void IQFilter_SetSpectrumMode(firFilter* handle, enum SpectrumType spectrumMode)
{
    IQFilter_ResetBuffer (handle, spectrumMode);
}
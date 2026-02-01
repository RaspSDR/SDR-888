#include "cdrUtility.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

#define CRC_NO_ERROR  cdr_NO_ERROR
#define CRC_ERROR     -1001

int ErrorHandling (int error, int newErrorCode, int lastError)
{
    return (  (lastError < cdr_NO_ERROR)
            ||((lastError > cdr_NO_ERROR) && (error >= cdr_NO_ERROR))) 
            ? lastError : (error >= cdr_NO_ERROR ? error : newErrorCode); 
}


cdr_byteArray* cdr_byteArray_Init(int length)
{
    cdr_byteArray *arrayHandle = (cdr_byteArray*) malloc (sizeof (cdr_byteArray));
    arrayHandle->handle        = (cdr_byte*) malloc (sizeof (cdr_byte)*length);
    arrayHandle->totalSize     = length;
    arrayHandle->validLength   = 0;
    return arrayHandle;
}

void cdr_byteArray_Release(cdr_byteArray* arrayHandle)
{
    free (arrayHandle->handle);
    free (arrayHandle);
}

cdr_complexArray* cdr_complexArray_Init(int length)
{
    cdr_complexArray *arrayHandle = (cdr_complexArray*) malloc (sizeof (cdr_complexArray));
    arrayHandle->handle           = (cdr_complex*) malloc (sizeof (cdr_complex)*length);
    arrayHandle->totalSize        = length;
    arrayHandle->validLength      = 0;
    return arrayHandle;
}

void cdr_complexArray_Release(cdr_complexArray* arrayHandle)
{
    free (arrayHandle->handle);
    free (arrayHandle);
}

cdr_complexFrame* cdr_complexFrame_Init(int totalLength, int numOfLines)
{
    cdr_complexFrame *frameHanle = (cdr_complexFrame*) malloc (sizeof (cdr_complexFrame));

    frameHanle->dataHandle   = cdr_complexArray_Init (totalLength);
    frameHanle->linePosition = (cdr_complexArraySub**) malloc (sizeof (cdr_complexArraySub*) * numOfLines);

    for (int i =0; i<numOfLines; i++)
    {
        frameHanle->linePosition[i] = (cdr_complexArraySub*) malloc (sizeof (cdr_complexArraySub));
        frameHanle->linePosition[i]->handle = frameHanle->dataHandle->handle;
        frameHanle->linePosition[i]->subLength = 0;
    }

    frameHanle->numOfLines = numOfLines;

    return frameHanle;
}

void cdr_complexFrame_Release(cdr_complexFrame* frameHanle)
{
    cdr_complexArray_Release (frameHanle->dataHandle);
    for (int i=0;i<frameHanle->numOfLines;i++)
    {
        free (frameHanle->linePosition[i]);
    }
    free(frameHanle->linePosition);

    free (frameHanle);
}


void BitsToBytes(cdr_byteArray* inputBits, cdr_byteArray* outputBytes)
{
    int length = inputBits->validLength / 8;
    int j = 0;
    int i = 0;
    for (i = 0; i < length; i++)
    {
        outputBytes->handle[i] =  (cdr_byte)(inputBits->handle[j] << 7);
        outputBytes->handle[i] += (cdr_byte)(inputBits->handle[j + 1] << 6);
        outputBytes->handle[i] += (cdr_byte)(inputBits->handle[j + 2] << 5);
        outputBytes->handle[i] += (cdr_byte)(inputBits->handle[j + 3] << 4);
        outputBytes->handle[i] += (cdr_byte)(inputBits->handle[j + 4] << 3);
        outputBytes->handle[i] += (cdr_byte)(inputBits->handle[j + 5] << 2);
        outputBytes->handle[i] += (cdr_byte)(inputBits->handle[j + 6] << 1);
        outputBytes->handle[i] += (cdr_byte)(inputBits->handle[j + 7]);
        j += 8;
    }

    if (j < inputBits->validLength)
    {
        outputBytes->handle[i] = 0;
        for (int k = 7; j < inputBits->validLength; j++)
        {
            outputBytes->handle[i] += (cdr_byte)(inputBits->handle[j] << k);
            k--;
        }
        i++;
    }

    outputBytes->validLength = i;
    return;
}

void BitsToUInt(cdr_byteArray* inputBits, cdr_uintArray* outputUInts)
{
    int integratedBits = 32;
    int highBit = integratedBits - 1;
    int length = inputBits->validLength / integratedBits;
    int j = 0;
    int i = 0;
    for (i = 0; i < length; i++)
    {
        outputUInts->handle[i] = 0;
        for (int k = 0; k < integratedBits; k++)
        {
            outputUInts->handle[i] += (cdr_byte)(inputBits->handle[j] << (highBit - (j % integratedBits)));
            j++;
        }
    }
    if (j < inputBits->validLength)
    {
        outputUInts->handle[i] = 0;
        for (; j < inputBits->validLength; j++)
        {
            outputUInts->handle[i] += (cdr_byte)(inputBits->handle[j] << (highBit - (j % integratedBits)));
        }
        i++;
    }
    outputUInts->validLength = i;
    return;
}

int BitArrayToInt(cdr_byteArray* bitArray, int start, int length)
{
    int result = 0;
    for (int i = 0; i < length; i++)
    {
        result <<= 1;
        result += bitArray->handle[start + i];
    }

    return result;
}


const cdr_byte CRC8_Table [256] = {
    0x00,  0x31,  0x62,  0x53,  0xC4,  0xF5,  0xA6,  0x97,
    0xB9,  0x88,  0xDB,  0xEA,  0x7D,  0x4C,  0x1F,  0x2E,
    0x43,  0x72,  0x21,  0x10,  0x87,  0xB6,  0xE5,  0xD4,
    0xFA,  0xCB,  0x98,  0xA9,  0x3E,  0x0F,  0x5C,  0x6D,
    0x86,  0xB7,  0xE4,  0xD5,  0x42,  0x73,  0x20,  0x11,
    0x3F,  0x0E,  0x5D,  0x6C,  0xFB,  0xCA,  0x99,  0xA8,
    0xC5,  0xF4,  0xA7,  0x96,  0x01,  0x30,  0x63,  0x52,
    0x7C,  0x4D,  0x1E,  0x2F,  0xB8,  0x89,  0xDA,  0xEB,
    0x3D,  0x0C,  0x5F,  0x6E,  0xF9,  0xC8,  0x9B,  0xAA,
    0x84,  0xB5,  0xE6,  0xD7,  0x40,  0x71,  0x22,  0x13,
    0x7E,  0x4F,  0x1C,  0x2D,  0xBA,  0x8B,  0xD8,  0xE9,
    0xC7,  0xF6,  0xA5,  0x94,  0x03,  0x32,  0x61,  0x50,
    0xBB,  0x8A,  0xD9,  0xE8,  0x7F,  0x4E,  0x1D,  0x2C,
    0x02,  0x33,  0x60,  0x51,  0xC6,  0xF7,  0xA4,  0x95,
    0xF8,  0xC9,  0x9A,  0xAB,  0x3C,  0x0D,  0x5E,  0x6F,
    0x41,  0x70,  0x23,  0x12,  0x85,  0xB4,  0xE7,  0xD6,
    0x7A,  0x4B,  0x18,  0x29,  0xBE,  0x8F,  0xDC,  0xED,
    0xC3,  0xF2,  0xA1,  0x90,  0x07,  0x36,  0x65,  0x54,
    0x39,  0x08,  0x5B,  0x6A,  0xFD,  0xCC,  0x9F,  0xAE,
    0x80,  0xB1,  0xE2,  0xD3,  0x44,  0x75,  0x26,  0x17,
    0xFC,  0xCD,  0x9E,  0xAF,  0x38,  0x09,  0x5A,  0x6B,
    0x45,  0x74,  0x27,  0x16,  0x81,  0xB0,  0xE3,  0xD2,
    0xBF,  0x8E,  0xDD,  0xEC,  0x7B,  0x4A,  0x19,  0x28,
    0x06,  0x37,  0x64,  0x55,  0xC2,  0xF3,  0xA0,  0x91,
    0x47,  0x76,  0x25,  0x14,  0x83,  0xB2,  0xE1,  0xD0,
    0xFE,  0xCF,  0x9C,  0xAD,  0x3A,  0x0B,  0x58,  0x69,
    0x04,  0x35,  0x66,  0x57,  0xC0,  0xF1,  0xA2,  0x93,
    0xBD,  0x8C,  0xDF,  0xEE,  0x79,  0x48,  0x1B,  0x2A,
    0xC1,  0xF0,  0xA3,  0x92,  0x05,  0x34,  0x67,  0x56,
    0x78,  0x49,  0x1A,  0x2B,  0xBC,  0x8D,  0xDE,  0xEF,
    0x82,  0xB3,  0xE0,  0xD1,  0x46,  0x77,  0x24,  0x15,
    0x3B,  0x0A,  0x59,  0x68,  0xFF,  0xCE,  0x9D,  0xAC,
};

const cdr_uint CRC32_Table [256] = {
    0x00000000,  0x04C11DB7,  0x09823B6E,  0x0D4326D9,  0x130476DC,  0x17C56B6B,  0x1A864DB2,  0x1E475005,
    0x2608EDB8,  0x22C9F00F,  0x2F8AD6D6,  0x2B4BCB61,  0x350C9B64,  0x31CD86D3,  0x3C8EA00A,  0x384FBDBD,
    0x4C11DB70,  0x48D0C6C7,  0x4593E01E,  0x4152FDA9,  0x5F15ADAC,  0x5BD4B01B,  0x569796C2,  0x52568B75,
    0x6A1936C8,  0x6ED82B7F,  0x639B0DA6,  0x675A1011,  0x791D4014,  0x7DDC5DA3,  0x709F7B7A,  0x745E66CD,
    0x9823B6E0,  0x9CE2AB57,  0x91A18D8E,  0x95609039,  0x8B27C03C,  0x8FE6DD8B,  0x82A5FB52,  0x8664E6E5,
    0xBE2B5B58,  0xBAEA46EF,  0xB7A96036,  0xB3687D81,  0xAD2F2D84,  0xA9EE3033,  0xA4AD16EA,  0xA06C0B5D,
    0xD4326D90,  0xD0F37027,  0xDDB056FE,  0xD9714B49,  0xC7361B4C,  0xC3F706FB,  0xCEB42022,  0xCA753D95,
    0xF23A8028,  0xF6FB9D9F,  0xFBB8BB46,  0xFF79A6F1,  0xE13EF6F4,  0xE5FFEB43,  0xE8BCCD9A,  0xEC7DD02D,
    0x34867077,  0x30476DC0,  0x3D044B19,  0x39C556AE,  0x278206AB,  0x23431B1C,  0x2E003DC5,  0x2AC12072,
    0x128E9DCF,  0x164F8078,  0x1B0CA6A1,  0x1FCDBB16,  0x018AEB13,  0x054BF6A4,  0x0808D07D,  0x0CC9CDCA,
    0x7897AB07,  0x7C56B6B0,  0x71159069,  0x75D48DDE,  0x6B93DDDB,  0x6F52C06C,  0x6211E6B5,  0x66D0FB02,
    0x5E9F46BF,  0x5A5E5B08,  0x571D7DD1,  0x53DC6066,  0x4D9B3063,  0x495A2DD4,  0x44190B0D,  0x40D816BA,
    0xACA5C697,  0xA864DB20,  0xA527FDF9,  0xA1E6E04E,  0xBFA1B04B,  0xBB60ADFC,  0xB6238B25,  0xB2E29692,
    0x8AAD2B2F,  0x8E6C3698,  0x832F1041,  0x87EE0DF6,  0x99A95DF3,  0x9D684044,  0x902B669D,  0x94EA7B2A,
    0xE0B41DE7,  0xE4750050,  0xE9362689,  0xEDF73B3E,  0xF3B06B3B,  0xF771768C,  0xFA325055,  0xFEF34DE2,
    0xC6BCF05F,  0xC27DEDE8,  0xCF3ECB31,  0xCBFFD686,  0xD5B88683,  0xD1799B34,  0xDC3ABDED,  0xD8FBA05A,
    0x690CE0EE,  0x6DCDFD59,  0x608EDB80,  0x644FC637,  0x7A089632,  0x7EC98B85,  0x738AAD5C,  0x774BB0EB,
    0x4F040D56,  0x4BC510E1,  0x46863638,  0x42472B8F,  0x5C007B8A,  0x58C1663D,  0x558240E4,  0x51435D53,
    0x251D3B9E,  0x21DC2629,  0x2C9F00F0,  0x285E1D47,  0x36194D42,  0x32D850F5,  0x3F9B762C,  0x3B5A6B9B,
    0x0315D626,  0x07D4CB91,  0x0A97ED48,  0x0E56F0FF,  0x1011A0FA,  0x14D0BD4D,  0x19939B94,  0x1D528623,
    0xF12F560E,  0xF5EE4BB9,  0xF8AD6D60,  0xFC6C70D7,  0xE22B20D2,  0xE6EA3D65,  0xEBA91BBC,  0xEF68060B,
    0xD727BBB6,  0xD3E6A601,  0xDEA580D8,  0xDA649D6F,  0xC423CD6A,  0xC0E2D0DD,  0xCDA1F604,  0xC960EBB3,
    0xBD3E8D7E,  0xB9FF90C9,  0xB4BCB610,  0xB07DABA7,  0xAE3AFBA2,  0xAAFBE615,  0xA7B8C0CC,  0xA379DD7B,
    0x9B3660C6,  0x9FF77D71,  0x92B45BA8,  0x9675461F,  0x8832161A,  0x8CF30BAD,  0x81B02D74,  0x857130C3,
    0x5D8A9099,  0x594B8D2E,  0x5408ABF7,  0x50C9B640,  0x4E8EE645,  0x4A4FFBF2,  0x470CDD2B,  0x43CDC09C,
    0x7B827D21,  0x7F436096,  0x7200464F,  0x76C15BF8,  0x68860BFD,  0x6C47164A,  0x61043093,  0x65C52D24,
    0x119B4BE9,  0x155A565E,  0x18197087,  0x1CD86D30,  0x029F3D35,  0x065E2082,  0x0B1D065B,  0x0FDC1BEC,
    0x3793A651,  0x3352BBE6,  0x3E119D3F,  0x3AD08088,  0x2497D08D,  0x2056CD3A,  0x2D15EBE3,  0x29D4F654,
    0xC5A92679,  0xC1683BCE,  0xCC2B1D17,  0xC8EA00A0,  0xD6AD50A5,  0xD26C4D12,  0xDF2F6BCB,  0xDBEE767C,
    0xE3A1CBC1,  0xE760D676,  0xEA23F0AF,  0xEEE2ED18,  0xF0A5BD1D,  0xF464A0AA,  0xF9278673,  0xFDE69BC4,
    0x89B8FD09,  0x8D79E0BE,  0x803AC667,  0x84FBDBD0,  0x9ABC8BD5,  0x9E7D9662,  0x933EB0BB,  0x97FFAD0C,
    0xAFB010B1,  0xAB710D06,  0xA6322BDF,  0xA2F33668,  0xBCB4666D,  0xB8757BDA,  0xB5365D03,  0xB1F740B4,
};

int CRC8(const cdr_byte *data, int start, int length)
{
    cdr_byte crcResult = 0xFF;
    for (int i = 0; i < length - 1; i++)   
        crcResult = CRC8_Table[crcResult ^ data[start + i]];
    crcResult ^= 0xFF;

    cdr_byte crcInput = data[start + length - 1];

    if (crcResult != crcInput)
        return CRC_ERROR;

    return CRC_NO_ERROR;
}

cdr_byte GenCRC8(cdr_byte gFunction, cdr_byte input)
{
    cdr_byte initStatus = 0;
    cdr_byte crcResult = initStatus;
    for (int i = 0; i < 8; i++)
    {
        cdr_byte newBit       = (cdr_byte)((input >> (7 - i)) & 0x01);             
        cdr_byte highBit      = (cdr_byte)(crcResult >> 7);                        
        cdr_byte newRemainder = (cdr_byte)((highBit ^ newBit) * gFunction);         
        crcResult <<= 1;                                                           
        crcResult ^= newRemainder;                                                
    }
    return crcResult;
}

int CRC32(const cdr_byte *data, int start, int length)
{
    cdr_uint crcResult = 0xFFFFFFFF;
    for (int i = 0; i < length - 4; i++)   
        crcResult = (crcResult << 8) ^ (CRC32_Table[(data[start + i] ^ (crcResult >> 24))]);
    crcResult ^= 0xFFFFFFFF;

    cdr_uint crcInput = 0;
    for (int i = 0; i < 4; i++)
        crcInput = (crcInput << 8) + data[start + length - 4 + i];

    if (crcResult != crcInput)
        return CRC_ERROR;

    return CRC_NO_ERROR;
}

cdr_uint GenCRC32(cdr_uint gFunction, cdr_byte input)
{
    cdr_uint initStatus = 0;
    cdr_uint crcResult = initStatus;
    for (int i = 0; i < 8; i++)
    {
        cdr_uint newBit       = ((cdr_uint)input >> (7 - i)) & 0x01;       
        cdr_uint highBit      = crcResult >> 31;                          
        cdr_uint newRemainder = (highBit ^ newBit) * gFunction;             
        crcResult <<= 1;                                                   
        crcResult ^= newRemainder;                                        
    }
    return crcResult;
}

void GenerateCRCTable (cdr_byte** CRC8_table, cdr_uint** CRC32_table)
{
    int tableSize = 256;

    *CRC8_table = malloc (sizeof(cdr_byte) * tableSize);
    for (int i = 0; i < tableSize; i++)
        (*CRC8_table)[i] = GenCRC8(0x31, (cdr_byte)i);

    *CRC32_table = malloc (sizeof(cdr_uint) * tableSize);
    for (int i = 0; i < tableSize; i++)
        (*CRC32_table)[i] = GenCRC32(0x04C11DB7, (cdr_byte)i);
    
    return;
}


void ComputeCrossCorrelation (const cdr_complex *a, int aLength,
                              const cdr_complex *b, int bLength,
                              cdr_complex *result)
{
    int maxLength = aLength + bLength - 1;
    for (int i = 0; i < maxLength; i++)
    {
        result[i] = 0;
        for (int j = 0; j < aLength; j++)
        {
            if (i - j >= 0 && i - j < bLength)
            {
                result[i] += a[j] * lv_conj(b[i - j]);
            }
        }
    }
}

void CrossCorrelationAmplitude (const cdr_complex *a, int aLength,
                                const cdr_complex *b, int bLength,
                                float *result)
{
    int maxLength = aLength + bLength - 1;
    for (int i = 0; i < maxLength; i++)
    {
        cdr_complex sum = 0;
        for (int j = 0; j < aLength; j++)
        {
            if (i - j >= 0 && i - j < bLength)
            {
                sum += a[j] * lv_conj(b[i - j]);
            }
        }
        result[i] = cdr_cabsf(sum);  
    }
}

void CrossCorrelationAmplitudePeak (const cdr_complex *a, int aLength,
                                    const cdr_complex *b, int bLength,
                                    float *peak, int *peakIndex)
{
    *peakIndex = 0;
    *peak = 0.0f;

    int offset = aLength-1;
    int maxLength = aLength + bLength - 1;
    for (int i = 0; i < maxLength; i++)
    {
        float amp;
        cdr_complex sum = 0; 
        int xIndex = i - offset;
        for (int j = 0; j < aLength; j++)
        {
            int bIndex = j + xIndex;
            if ( bIndex >= 0 && bIndex < bLength )
            {
                sum += a[j] * lv_conj(b[bIndex]);
            }
        }

        amp = cdr_cabsf(sum);
        if (amp > *peak)
        {
            *peak = amp;
            *peakIndex = xIndex;
        }
    }
}

cdr_complex ComputeDotConjProduct (const cdr_complex *a,  const cdr_complex *b, int length)
{
    if (length <= 0) {
        return cdr_complex_Zero;
    }

    cdr_complex result = cdr_complex_Zero;
    volk_32fc_x2_conjugate_dot_prod_32fc(&result, a, b, (unsigned int)length);
    return result;
}

float GetComplexPhase(cdr_complex z)
{
    return atan2f(lv_cimag(z), lv_creal(z));
}

void GetComplexPhaseArray(const cdr_complex *z, int n, float *phaseArray)
{
    for (int i=0; i<n; i++)
        phaseArray[i] = atan2f(lv_cimag(z[i]), lv_creal(z[i]));
    return;
}

void Modulator (cdr_complex *inout, int length, float frequency, float sampleRate, float *phase)
{
    if (length <= 0) {
        return;
    }

    float deltaPhase = 2.0f * M_PI * frequency / sampleRate;

    // VOLK rotator is not guaranteed to be in-place safe, so use a temp buffer.
    if (length >= 64) {
        cdr_complex *tmp = (cdr_complex*)volk_malloc((size_t)length * sizeof(cdr_complex), volk_get_alignment());
        if (tmp) {
            cdr_complex phase_state = lv_cmake(cosf(*phase), sinf(*phase));
            const cdr_complex phase_inc = lv_cmake(cosf(deltaPhase), sinf(deltaPhase));
            volk_32fc_s32fc_x2_rotator2_32fc(tmp, inout, &phase_inc, &phase_state, (unsigned int)length);
            memcpy(inout, tmp, (size_t)length * sizeof(cdr_complex));
            volk_free(tmp);

            *phase = atan2f(lv_cimag(phase_state), lv_creal(phase_state));
            if (*phase < -M_PI) { *phase += 2.0f * M_PI; }
            if (*phase >  M_PI) { *phase -= 2.0f * M_PI; }
            return;
        }
    }

    for (int i = 0; i < length; i++)
    {
        inout[i] *= lv_cmake(cosf(*phase), sinf(*phase));
        *phase += deltaPhase;
        if (*phase < -M_PI)
        {
            *phase += 2.0f * M_PI;
        }
        if (*phase > M_PI)
        {
            *phase -= 2.0f * M_PI;
        }
    }
}

void ArrayCopy_byte(cdr_byte *source, cdr_byte *destination, int length)
{
    memcpy(destination, source, length*sizeof(cdr_byte));
}

void ArrayCopy_complex (cdr_complex *source, cdr_complex *destination, int length)
{
    memcpy(destination, source, length*sizeof(cdr_complex));
}

void ArrayMove_byte(cdr_byte *source, cdr_byte *destination, int length)
{
    memmove(destination, source, length*sizeof(cdr_byte));
}

void ArrayMove_complex (cdr_complex *source, cdr_complex *destination, int length)
{
    memmove(destination, source, length*sizeof(cdr_complex));
}

void ArrraySetZero_complex(cdr_complex *array, int length)
{
    memset(array, 0, length*sizeof(cdr_complex));
}

void ArrraySetZero_int(int *array, int length)
{
    memset(array, 0, length*sizeof(int));
}

void ArrrayMulti_complex(cdr_complex *a, cdr_complex *b, cdr_complex *c, int length)
{
    if (length <= 0) {
        return;
    }

    // Only use VOLK when output doesn't overlap inputs.
    if (c != a && c != b) {
        volk_32fc_x2_multiply_32fc(c, a, b, (unsigned int)length);
        return;
    }

    for (int i = 0; i < length; i++)
    {
        c[i] = a[i] * b[i];
    }
}

void ArrrayAmplitudePeak_complex (const cdr_complex *a, int length, float *peak, int *peakIndex)
{
    *peakIndex = 0;
    *peak = 0.0f;

    if (length <= 0) {
        return;
    }

    // Use VOLK to compute |a[i]|^2, then find max.
    float *mag2 = (float*)malloc((size_t)length * sizeof(float));
    if (mag2) {
        volk_32fc_magnitude_squared_32f(mag2, a, (unsigned int)length);
        float bestMag2 = mag2[0];
        int bestIdx = 0;
        for (int i = 1; i < length; i++) {
            if (mag2[i] > bestMag2) {
                bestMag2 = mag2[i];
                bestIdx = i;
            }
        }
        free(mag2);
        *peakIndex = bestIdx;
        *peak = sqrtf(bestMag2);
        return;
    }

    for (int i = 0; i < length; i++)
    {
        float amp = cdr_cabsf(a[i]);
        if (amp > *peak)
        {
            *peak = amp;
            *peakIndex = i;
        }
    }
}

void ArrrayConjunction_complex (cdr_complex *a, int length)
{
    if (length <= 0) {
        return;
    }
    volk_32fc_conjugate_32fc(a, a, (unsigned int)length);
}

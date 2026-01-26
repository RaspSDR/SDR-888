#include "cdrLDPCCodec.h"

#include "cdrUtility.h"

#include <string.h>
#include <stdlib.h>

#define LDPC_NO_ERROR  cdr_NO_ERROR
#define LDPC_CAL_FAIL  -1026

#define BlockSize 9216

#define RETRY_Interation 1000

extern const CheckMatrixInfo CheckMatrix_1_4;
extern const CheckMatrixInfo CheckMatrix_1_3;
extern const CheckMatrixInfo CheckMatrix_1_2;
extern const CheckMatrixInfo CheckMatrix_3_4;

static void ArrayFindMax_int (const int *array, int length, int *max, int *maxIndex)
{
	int i;
	*max = array[0];
	*maxIndex = 0;
	for (i=1; i<length; i++)
	{
		if (*max < array[i])
		{
			*max = array[i];
			*maxIndex = i;
		}
	}
}

static void ArrayFindMax_int_invert (const int *array, int length, int *max, int *maxIndex)
{
	int i;
	*max = array[length-1];
	*maxIndex = length-1;
	for (i=length-2; i>=0; i--)
	{
		if (*max < array[i])
		{
			*max = array[i];
			*maxIndex = i;
		}
	}
}

static int Decoder(cdr_byte* inputBits, cdr_byte* outputBits, const CheckMatrixInfo* checkMatrix)
{
	int i, j, k;

	int checkSize;
	cdr_byte checkSum;

	int retry;
	int max, maxIndex;

	int errorCounter [BlockSize];

	checkSize = checkMatrix->totalLines;
	for (retry = 0; retry < RETRY_Interation; retry++)
	{
		ArrraySetZero_int (errorCounter, BlockSize);
		
		for (i = 0; i < checkSize; i++)
		{
			checkSum = 0;
			for (j = 0; j < checkMatrix->lineData[i].size; j++)
			{
				checkSum ^= inputBits[checkMatrix->lineData[i].checkPosition[j]];
			}

			if (checkSum != 0)
			{
				for (k = 0; k < checkMatrix->lineData[i].size; k++)
				{
					errorCounter[checkMatrix->lineData[i].checkPosition[k]]++;
				}
			}
		}

		if (retry %2 == 0)
		{
			ArrayFindMax_int(errorCounter, BlockSize, &max, &maxIndex);
		}
		else
		{
			ArrayFindMax_int_invert(errorCounter, BlockSize, &max, &maxIndex);
		}
		
		if (max != 0)
		{
			inputBits[maxIndex] = (cdr_byte)((~inputBits[maxIndex]) & 0x01);
		}
		else    
			break;
	}

	if (outputBits != inputBits)
		ArrayMove_byte (inputBits, outputBits, (BlockSize - checkSize));
	
	if (max == 0)
	{
		return retry;
	}
	else
	{
		return LDPC_CAL_FAIL; 
	}
}

int LdpcDecoder(cdr_byteArray* inputBits, cdr_byteArray* outputBits, enum LDPCRate coderate)
{
	int error = LDPC_NO_ERROR;

	const CheckMatrixInfo *checkMatrix;
	switch (coderate)
	{
		case Rate1_4:
			checkMatrix = &CheckMatrix_1_4;
			break;
		case Rate1_3:
			checkMatrix = &CheckMatrix_1_3;
			break;
		case Rate1_2:
			checkMatrix = &CheckMatrix_1_2;
			break;
		case Rate3_4:
		default:
			checkMatrix = &CheckMatrix_3_4;
			break;
	}

	int inputBlockSize = BlockSize;
	int totalBlock = inputBits->validLength / inputBlockSize;
	int outputBlockSize = BlockSize - checkMatrix->totalLines;

	outputBits->validLength = 0;
	int retry = 0;
	cdr_byte *tempInput, *tempeOutput;
	for (int i = 0; i < totalBlock; i++)
	{
		tempInput   = inputBits->handle  + i*BlockSize;
		tempeOutput = outputBits->handle + i*outputBlockSize;
		retry = Decoder(tempInput, tempeOutput, checkMatrix);
		if (retry < LDPC_NO_ERROR)
		{
			error = retry;
			break;
		}
		outputBits->validLength += outputBlockSize;
	}

	return error;
}


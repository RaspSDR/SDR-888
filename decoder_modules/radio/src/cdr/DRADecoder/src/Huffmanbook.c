#include "Huffmanbook.h"
#include <stdio.h>
#include <math.h>

#define HuffmanPageInfo_BitIncrement  0
#define HuffmanPageInfo_CodeValue     1 
#define HuffmanPageInfo_Index         2

#define Max_PageSize  289

int HuffmanDecode(const HuffmanStruct *huffmanBook, void* streamInput, GetBitsFunc fucUnpack)
{
    unsigned int unBits = 0;
    for (int n = 0; n < huffmanBook->PageSize; n++)
    {
        int nShift = huffmanBook->BookPages[n][HuffmanPageInfo_BitIncrement];
        if (nShift > 0)
        {
            unBits = unBits << nShift;
            unBits |= fucUnpack(streamInput, nShift);
        }
        if ( (int)unBits == huffmanBook->BookPages[n][HuffmanPageInfo_CodeValue])
        {
            return huffmanBook->BookPages[n][HuffmanPageInfo_Index];
        }
    }
    return 0;
}

int HuffDecDiff(const HuffmanStruct *huffmanBook, void* streamInput, GetBitsFunc fucUnpack, int nIndex_histroy)
{
    int nDiff = HuffmanDecode(huffmanBook, streamInput, fucUnpack);
    int nIndex = (nIndex_histroy + nDiff) % (huffmanBook->PageSize);
    return nIndex;
}

int HuffDecRecursive(const HuffmanStruct *huffmanBook, void* streamInput, GetBitsFunc fucUnpack)
{
    int k = -1;
    int nQIndex;
    do
    {
        k++;
        nQIndex = HuffmanDecode(huffmanBook, streamInput, fucUnpack);
    } while (nQIndex == (huffmanBook->PageSize - 1));
    
    nQIndex = k * (huffmanBook->PageSize - 1) + nQIndex;
    return nQIndex;
}

void testHuffmanBook(const HuffmanStruct *testBook)
{
    int dataLongBits [Max_PageSize];
    int tailBits [Max_PageSize];
    int totalBits = 0;
    int bits = 0;

    for (int j = 0; j < testBook->PageSize; j++)
    {
        totalBits += testBook->BookPages[j][HuffmanPageInfo_BitIncrement];
    }

    for (int j = 0; j < testBook->PageSize; j++)
    {
        bits += testBook->BookPages[j][HuffmanPageInfo_BitIncrement];
        tailBits[j] = totalBits - bits;
        dataLongBits[j] = testBook->BookPages[j][HuffmanPageInfo_CodeValue] * (int)(pow(2.0, tailBits[j]));
    }

    for (int i = 0; i < testBook->PageSize; i++)
    {
        for (int j = i + 1; j < testBook->PageSize; j++)
        {
            int mask = ((int)(pow(2.0, totalBits)) - 1) - ((int)(pow(2.0, tailBits[i])) - 1);
            if (dataLongBits[i] == (dataLongBits[j] & mask))
            {
                printf(" i=%d, j=%d; data[i]: [i,0]=%d,[i,1]=%d,[i,2]=%d; data[j]: [j,0]=%d,[j,1]=%d,[j,2]=%d, \r\n",
			             i,  j, testBook->BookPages[i][0], testBook->BookPages[i][1], testBook->BookPages[i][2], testBook->BookPages[j][0], testBook->BookPages[j][1], testBook->BookPages[j][2]);
            }
        }
    }

    for (int i = 0; i < testBook->PageSize; i++)
    {
        int j = 0;
        for (; j < testBook->PageSize; j++)
        {
            if (i == testBook->BookPages[j][HuffmanPageInfo_Index])
                break;
        }
        if (j == testBook->PageSize)
        {
             printf("{%d} not found ", i);
        }
    }
}

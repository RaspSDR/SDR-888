#ifndef _HUFFMANBOOK_H_
#define _HUFFMANBOOK_H_

typedef struct HuffmanStruct
{  
    const int (*BookPages)[3];

    int PageSize;

    int Dimension;

    int DimensionLength;
    
    int MidThread;    
} HuffmanStruct;

typedef int (*GetBitsFunc)(void* steamInput, int length); 

int HuffmanDecode(const HuffmanStruct *huffmanBook, void* streamInput, GetBitsFunc fucUnpack);

int HuffDecDiff(const HuffmanStruct *huffmanBook, void* streamInput, GetBitsFunc fucUnpack, int nIndex);

int HuffDecRecursive(const HuffmanStruct *huffmanBook, void* streamInput, GetBitsFunc fucUnpack);

#endif
#ifndef CDR_LDPC_H 
#define CDR_LDPC_H 

#include <cdrDemod.h>

typedef struct CheckLineInfo
{  
    const int* checkPosition;      
    int size;                      
} CheckLineInfo;

typedef struct CheckMatrixInfo
{  
    const CheckLineInfo* lineData;   	  
    int totalLines;						  
} CheckMatrixInfo;

int LdpcDecoder(cdr_byteArray* inputBits, cdr_byteArray* outputBits, enum LDPCRate coderate);

extern const CheckMatrixInfo CheckMatrix_1_4;
extern const CheckMatrixInfo CheckMatrix_1_3;
extern const CheckMatrixInfo CheckMatrix_1_2;
extern const CheckMatrixInfo CheckMatrix_3_4;

#endif
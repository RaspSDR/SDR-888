#ifndef CDR_MODULATION_H 
#define CDR_MODULATION_H 

#include <cdrDemod.h>

typedef struct ConstellationInfo
{
    const cdr_complex* constellationIQ;
    int bitsPerSymbol;
}ConstellationInfo;

extern  const cdr_complex mod_pilot[4];

extern  const cdr_complex mod_QPSK[4];
 
extern  const cdr_complex mod_16QAM[16];
 
extern  const cdr_complex *const mod_16QAM_Alpha1;

extern  const cdr_complex mod_16QAM_Alpha2[16];

extern  const cdr_complex mod_16QAM_Alpha4[16];

extern  const cdr_complex mod_64QAM[64];

extern  const cdr_complex *const mod_64QAM_Alpha1;

extern  const cdr_complex mod_64QAM_Alpha2[64];

extern  const cdr_complex mod_64QAM_Alpha4[64];

void DeConstellation(cdr_complexArray* symbols, cdr_byteArray* bits, const ConstellationInfo* constellation);

#endif
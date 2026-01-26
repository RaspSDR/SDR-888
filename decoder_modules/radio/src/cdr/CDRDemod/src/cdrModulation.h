#ifndef CDR_MODULATION_H 
#define CDR_MODULATION_H 

#include <cdrDemod.h>

typedef struct ConstellationInfo
{
    const cdr_complex* constellationIQ;
    int bitsPerSymbol;
}ConstellationInfo;

extern  cdr_complex mod_pilot[];

extern  cdr_complex mod_QPSK[];
 
extern  cdr_complex mod_16QAM[];
 
extern  cdr_complex *const mod_16QAM_Alpha1;

extern  cdr_complex mod_16QAM_Alpha2[];

extern  cdr_complex mod_16QAM_Alpha4[];

extern  cdr_complex mod_64QAM[];

extern  cdr_complex *const mod_64QAM_Alpha1;

extern  cdr_complex mod_64QAM_Alpha2[];

extern  cdr_complex mod_64QAM_Alpha4[];

void DeConstellation(cdr_complexArray* symbols, cdr_byteArray* bits, const ConstellationInfo* constellation);

#endif
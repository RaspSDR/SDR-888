#ifndef CDR_INTERLEAVE_H
#define CDR_INTERLEAVE_H

#include <cdrDemod.h>

typedef struct DeInterleaveTable
{
    int N_mux;
    int *table;
} DeInterleaveTable;

void DeInterleave_complex32(cdr_complexArray *input, cdr_complexArray *output, int N_mux);

void DeInterleave_byte(cdr_byteArray *input, cdr_byteArray *output, int N_mux);

DeInterleaveTable* GenerateDeInterleaveTable (int N_mux);

void ReleaseDeInterleaveTable (DeInterleaveTable *table);

void DeInterleaveByTable_byte (cdr_byteArray *input, DeInterleaveTable *deInterleaveTable);

void DeInterleaveByTable_complex32 (cdr_complexArray *input, DeInterleaveTable *deInterleaveTable);

#endif

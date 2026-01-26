#include "cdrInterleave.h"

#include <math.h>
#include <stdlib.h>

#define DEINTERLEAVE(T) \
void DeInterleave_##T(T* input, T* output, int* pattern, int length) { \
    for (int i = 0; i < length; i++) { \
        output[pattern[i]] = input[i]; \
    } \
}

DEINTERLEAVE(int)       
DEINTERLEAVE(float)     


void DeInterleave(int* input, int* output, int* pattern, int length)
{
	int i;
    for (i = 0; i < length; i++)
	{
		output[pattern[i]] = input[i];
    }
}

void DeInterleave_byte(cdr_byteArray* input, cdr_byteArray* output, int N_mux)
{
	int n, p0, s, g ;
	int i;
	int pattern_n;

	n  = 0;
	p0 = 0;
	s  = (int)pow(2, ceil((log10(N_mux) / log10(2))));
	g  = s / 4 - 1;
	
	for (i = 0; i < s; i++)
	{
		if (p0 < N_mux)
		{
			pattern_n = p0;
			output->handle[pattern_n] = input->handle[n];
			n++;
		}

		p0 = (p0 * 5 + g) % s;
	}
	
	output->validLength = N_mux;
	return;
}

void DeInterleave_complex32(cdr_complexArray* input, cdr_complexArray* output, int N_mux)
{
	int n, p0, s, g ;
	int i;
	int pattern_n;

	n  = 0;
	p0 = 0;
	s  = (int)pow(2, ceil((log10(N_mux) / log10(2))));
	g  = s / 4 - 1;
	
	for (i = 0; i < s; i++)
	{
		if (p0 < N_mux)
		{
			pattern_n = p0;
			output->handle[pattern_n] = input->handle[n];
			n++;
		}

		p0 = (p0 * 5 + g) % s;
	}

	output->validLength = N_mux;
	return;
}


static void GenerateInterleavePattern(int N_mux, int*pattern )
{
	int n, p0, s, g ;
	int i;

	n  = 0;
	p0 = 0;
	s  = (int)pow(2, ceil((log10(N_mux) / log10(2))));
	g  = s / 4 - 1;
	
	for (i = 0; i < s; i++)
	{
		if (p0 < N_mux)
		{
			pattern[n] = p0;
			n++;
		}

		p0 = (p0 * 5 + g) % s;
	}

	return;
}

static void ReverseInterleavePattern(int N_mux, int *pattern, int *reverse)
{
	int temp;
	for (int i = 0; i < N_mux; i++)
	{
		reverse[i] = i;
	}

    for (int i = 0; i < N_mux - 1; i++)
	{
        for (int j = 0; j < N_mux - i - 1; j++)
		{
            if (pattern[reverse[j]] > pattern[reverse[j + 1]])
			{
				temp   	       = reverse[j];
				reverse[j]     = reverse[j + 1];
				reverse[j + 1] = temp;
            }
        }
    }
}

DeInterleaveTable* GenerateDeInterleaveTable (int N_mux)
{
	int *table   = (int*)malloc(sizeof(int)*N_mux);
	int *pattern = (int*)malloc(sizeof(int)*N_mux);
	int *reverse = (int*)malloc(sizeof(int)*N_mux);
	int *check;

	GenerateInterleavePattern (N_mux, pattern);
	ReverseInterleavePattern  (N_mux, pattern, reverse);

	check = pattern;
	int i = 0;
	int loopStrat = 0;	  
	while (i<N_mux)
	{	
		int next_index = loopStrat;
		do
		{
			next_index = reverse[next_index];

			table[i] = next_index;
			i++;

			check[next_index] = -1;			
		}while (next_index != loopStrat);


		while (loopStrat<N_mux && check[loopStrat]<0 )
		{
			loopStrat++;
		}
		table[i-1] = -loopStrat;
	}

	free (pattern);
	free (reverse);

	DeInterleaveTable *deInterleaveTable = (DeInterleaveTable*)malloc(sizeof(DeInterleaveTable));
	deInterleaveTable->N_mux = N_mux;
	deInterleaveTable->table = table;
	return deInterleaveTable;
}

void ReleaseDeInterleaveTable (DeInterleaveTable *table)
{
	free (table->table);
	free (table);
}

void DeInterleaveByTable_byte (cdr_byteArray *input, DeInterleaveTable *deInterleaveTable)
{
	cdr_byte *p = input->handle;
	cdr_byte temp;

	int length = deInterleaveTable->N_mux;
	int *table = deInterleaveTable->table;

	int index, last;
	index = 0;
	temp = p[index];
	for (int i=0;i<length;i++)
	{
		last  = index;
		index = table[i];
		if (index > 0)
		{
			p[last] = p[index];
		}
		else
		{
			p[last] = temp;
			index = -index;
			temp = p[index];
		}
	}
	return;
}

void DeInterleaveByTable_complex32 (cdr_complexArray *input, DeInterleaveTable *deInterleaveTable)
{
	cdr_complex *p = input->handle;
	cdr_complex temp = p[0];
	
	int length = deInterleaveTable->N_mux;
	int *table = deInterleaveTable->table;

	int index, last;
	index = 0;
	temp = p[index];
	for (int i=0;i<length;i++)
	{
		last  = index;
		index = table[i];
		if (index > 0)
		{
			p[last] = p[index];
		}
		else
		{
			p[last] = temp;
			index = -index;
			temp = p[index];
		}
	}
	return;
}



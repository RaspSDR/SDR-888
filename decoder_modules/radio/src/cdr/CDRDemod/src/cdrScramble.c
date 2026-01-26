#include "cdrScramble.h"

#define SCRAMBLE_LFSR_WIDTH 12

#define SCRAMBLE_TAP_12 0       
#define SCRAMBLE_TAP_11 1       
#define SCRAMBLE_TAP_8  4       
#define SCRAMBLE_TAP_6  6       
#define SCRAMBLE_TAP_FEEDBAK (SCRAMBLE_LFSR_WIDTH-1)

#define SCRAMBLE_SEED 0x0800

static int GenerateLFSR(int *state)
{
    int feedback = 0;

    feedback = ((*state >> SCRAMBLE_TAP_12) & 1 ) ^
               ((*state >> SCRAMBLE_TAP_11) & 1 ) ^
               ((*state >> SCRAMBLE_TAP_8)  & 1 ) ^
               ((*state >> SCRAMBLE_TAP_6)  & 1 );

    *state = (*state >> 1) | (feedback << SCRAMBLE_TAP_FEEDBAK);

    return *state;
}

void DeScramble(cdr_byteArray* input, cdr_byteArray* output)
{
    int i;
	int state = SCRAMBLE_SEED;
	for (i = 0; i < input->validLength; i++)
	{
		output->handle[i] = (cdr_byte)(input->handle[i] ^ (state & 1));
		state = GenerateLFSR (&state);
	}

    output->validLength = input->validLength;
}

#ifndef TS_STREAM_H  
#define TS_STREAM_H 

#define TS_BLOCK_SIZE 188

typedef void* tsHandle;

typedef unsigned char  ts_byte;

tsHandle ts_Init (void);

void ts_Release (tsHandle handle);

int ts_GenES (tsHandle handle, int id, ts_byte *input, int inputLength, ts_byte *output);

int ts_GenPAT (tsHandle handle, ts_byte *output);

int ts_GenPMT (tsHandle handle, int id, ts_byte *output);

#endif
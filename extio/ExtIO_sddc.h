#pragma once

#ifdef EXTIO_EXPORTS
#define EXTIO_API __declspec(dllexport) __stdcall
#else
#define EXTIO_API __stdcall
#endif

#define EXPORT_EXTIO_TUNE_FUNCTIONS 0

#include "LC_ExtIO_Types.h"

extern "C" bool EXTIO_API InitHW(char* name, char* model, int& type);
extern "C" int EXTIO_API StartHW64(int64_t freq);
extern "C" int EXTIO_API StartHWdbl(double freq);

extern "C" bool EXTIO_API OpenHW(void);
extern "C" int  EXTIO_API StartHW(long freq);

extern "C" void EXTIO_API StopHW(void);
extern "C" void EXTIO_API CloseHW(void);
extern "C" void EXTIO_API SwitchGUI(void);
extern "C" void EXTIO_API ShowGUI(void);
extern "C" void EXTIO_API HideGUI(void);

extern "C" int  EXTIO_API SetHWLO(long LOfreq);
extern "C" int64_t EXTIO_API SetHWLO64(int64_t LOfreq);
extern "C" double EXTIO_API SetHWLOdbl(double LOfreq);

extern "C" int  EXTIO_API GetStatus(void);
extern "C" void EXTIO_API SetCallback(pfnExtIOCallback funcptr);

extern "C" long EXTIO_API GetHWLO(void);
extern "C" int64_t EXTIO_API GetHWLO64(void);
extern "C" double EXTIO_API GetHWLOdbl(void);

extern "C" long EXTIO_API GetHWSR(void);
extern "C" double EXTIO_API GetHWSRdbl(void);

#if EXPORT_EXTIO_TUNE_FUNCTIONS
extern "C" long EXTIO_API GetTune(void);
extern "C" int64_t EXTIO_API GetTune64(void);
extern "C" double EXTIO_API GetTunedbl(void);

extern "C" void    EXTIO_API TuneChanged(long freq);
extern "C" void    EXTIO_API TuneChanged64(int64_t freq);
extern "C" void    EXTIO_API TuneChangeddbl(double freq);
#endif

extern "C" void EXTIO_API VersionInfo(const char* progname, int ver_major, int ver_minor);

extern "C" int EXTIO_API GetAttenuators(int idx, float* attenuation);
extern "C" int EXTIO_API GetActualAttIdx(void);
extern "C" int EXTIO_API SetAttenuator(int idx);

extern "C" int EXTIO_API ExtIoGetAGCs(int agc_idx, char* text);
extern "C" int EXTIO_API ExtIoGetActualAGCidx(void);
extern "C" int EXTIO_API ExtIoSetAGC(int agc_idx);
extern "C" int EXTIO_API ExtIoShowMGC(int agc_idx);

extern "C" int EXTIO_API ExtIoGetMGCs(int mgc_idx, float* gain);
extern "C" int EXTIO_API ExtIoGetActualMgcIdx(void);
extern "C" int EXTIO_API ExtIoSetMGC(int mgc_idx);

extern "C" int  EXTIO_API ExtIoGetSrates(int idx, double* samplerate);
extern "C" int  EXTIO_API ExtIoSrateSelText(int idx, char* text);
extern "C" int  EXTIO_API ExtIoGetActualSrateIdx(void);
extern "C" int  EXTIO_API ExtIoSetSrate(int idx);

extern "C" int  EXTIO_API ExtIoGetSetting(int idx, char* description, char* value);
extern "C" void EXTIO_API ExtIoSetSetting(int idx, const char* value);

extern "C" void EXTIO_API SetPPMvalue(double new_ppm_value);
extern "C" double EXTIO_API GetPPMvalue(void);
extern "C" void EXTIO_API IFrateInfo(int rate);

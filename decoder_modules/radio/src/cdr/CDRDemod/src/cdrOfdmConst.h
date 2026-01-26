#ifndef CDR_OFDM_CONST_H
#define CDR_OFDM_CONST_H

#include "cdrDemod.h"

#define SampleRate 816000

#define SubFrameTime 0.160

#define SubFrameLength 130560

#define SYNC_BCPLength_Mode1  384
#define SYNC_BCPLength_Mode2  332
#define SYNC_BCPLength_Mode3  168

#define SYNC_NbLength_Mode1  1024
#define SYNC_NbLength_Mode2   512
#define SYNC_NbLength_Mode3  1024

#define SYNC_L_Mode1     120
#define SYNC_L_Mode2      60
#define SYNC_L_Mode3     120

#define SYNC_m_Mode1      48
#define SYNC_m_Mode2      12
#define SYNC_m_Mode3      48

#define SYNC_Nzc_Mode1   967
#define SYNC_Nzc_Mode2   487
#define SYNC_Nzc_Mode3   967

extern const int SyncBin_Mode1_Spec1  [2][2];
extern const int SyncBin_Mode1_Spec2  [4][2];
extern const int SyncBin_Mode1_Spec9  [2][2];
extern const int SyncBin_Mode1_Spec10 [4][2];
extern const int SyncBin_Mode1_Spec22 [2][2];
extern const int SyncBin_Mode1_Spec23 [4][2];

extern const int SyncBin_Mode2_Spec1  [2][2];
extern const int SyncBin_Mode2_Spec2  [4][2];
extern const int SyncBin_Mode2_Spec9  [2][2];
extern const int SyncBin_Mode2_Spec10 [4][2];
extern const int SyncBin_Mode2_Spec22 [2][2];
extern const int SyncBin_Mode2_Spec23 [4][2];

extern const int SyncBin_Mode3_Spec1  [2][2];
extern const int SyncBin_Mode3_Spec2  [4][2];
extern const int SyncBin_Mode3_Spec9  [2][2];
extern const int SyncBin_Mode3_Spec10 [4][2];
extern const int SyncBin_Mode3_Spec22 [2][2];
extern const int SyncBin_Mode3_Spec23 [4][2];

#define OFDM_TuLength_Mode1  2048
#define OFDM_TuLength_Mode2  1024
#define OFDM_TuLength_Mode3  2048

#define OFDM_TcpLength_Mode1  240
#define OFDM_TcpLength_Mode2  140
#define OFDM_TcpLength_Mode3   56

#define OFDM_SN_Mode1   56
#define OFDM_SN_Mode2  111
#define OFDM_SN_Mode3   61

#define OFDM_Nv_Mode1  242
#define OFDM_Nv_Mode2  122
#define OFDM_Nv_Mode3  242

extern const int Carrier_Mode1_Spec1  [2][2];
extern const int Carrier_Mode1_Spec2  [4][2];
extern const int Carrier_Mode1_Spec9  [2][2];
extern const int Carrier_Mode1_Spec10 [4][2];
extern const int Carrier_Mode1_Spec22 [2][2];
extern const int Carrier_Mode1_Spec23 [4][2];

extern const int Carrier_Mode2_Spec1  [2][2];
extern const int Carrier_Mode2_Spec2  [4][2];
extern const int Carrier_Mode2_Spec9  [2][2];
extern const int Carrier_Mode2_Spec10 [4][2];
extern const int Carrier_Mode2_Spec22 [2][2];
extern const int Carrier_Mode2_Spec23 [4][2];

extern const int Carrier_Mode3_Spec1  [2][2];
extern const int Carrier_Mode3_Spec2  [4][2];
extern const int Carrier_Mode3_Spec9  [2][2];
extern const int Carrier_Mode3_Spec10 [4][2];
extern const int Carrier_Mode3_Spec22 [2][2];
extern const int Carrier_Mode3_Spec23 [4][2];

#define Pilot_InfoSize 3

extern const int Pilot_Start_Position [Pilot_InfoSize][2];

#define Pilot_TotalBins 62
#define Pilot_TotalBins_Mode1  62
#define Pilot_TotalBins_Mode2  32
#define Pilot_TotalBins_Mode3  62

#define Pilot_ofdmSymbol_Distance 12
#define Pilot_TransferH_Distance  4 

#define OFDM_InfoLength_Mode1 4
#define OFDM_InfoLength_Mode2 2
#define OFDM_InfoLength_Mode3 4

extern const int Info_Position_Increase_Mode1 [2][5];
extern const int Info_Position_Increase_Mode2 [2][5];
extern const int Info_Position_Increase_Mode3 [2][5];

extern const int Info_Position_Mode1 [8];
extern const int Info_Position_Mode2 [4];
extern const int Info_Position_Mode3 [8];


#endif

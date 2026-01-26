
#include "cdrOfdmConst.h"

const int SyncBin_Mode1_Spec1  [2][2] = {{ 452,  512}, { 513,   573}};
const int SyncBin_Mode1_Spec2  [4][2] = {{ 389,  449}, { 450,   510}, { 515,  575}, { 576,  636}};
const int SyncBin_Mode1_Spec9  [2][2] = {{ 264,  324}, { 701,   761}};
const int SyncBin_Mode1_Spec10 [4][2] = {{ 201,  261}, { 262,   322}, { 703,  763}, { 764,  824}};
const int SyncBin_Mode1_Spec22 [2][2] = {{ 327,  387}, { 638,   698}};
const int SyncBin_Mode1_Spec23 [4][2] = {{ 264,  324}, { 325,   385}, { 640,  700}, { 701,  761}};

const int SyncBin_Mode2_Spec1  [2][2] = {{ 226,  256}, { 257,   287}};
const int SyncBin_Mode2_Spec2  [4][2] = {{ 195,  225}, { 226,   256}, { 257,  287}, { 288,  318}};
const int SyncBin_Mode2_Spec9  [2][2] = {{ 132,  162}, { 351,   381}};
const int SyncBin_Mode2_Spec10 [4][2] = {{ 101,  131}, { 132,   162}, { 351,  381}, { 382,  412}};
const int SyncBin_Mode2_Spec22 [2][2] = {{ 163,  193}, { 320,   350}};
const int SyncBin_Mode2_Spec23 [4][2] = {{ 132,  162}, { 163,   193}, { 320,  350}, { 351,  381}};

const int SyncBin_Mode3_Spec1  [2][2] = {{ 452,  512}, { 513,   573}};
const int SyncBin_Mode3_Spec2  [4][2] = {{ 389,  449}, { 450,   510}, { 515,  575}, { 576,  636}};
const int SyncBin_Mode3_Spec9  [2][2] = {{ 264,  324}, { 701,   761}};
const int SyncBin_Mode3_Spec10 [4][2] = {{ 201,  261}, { 262,   322}, { 703,  763}, { 764,  824}};
const int SyncBin_Mode3_Spec22 [2][2] = {{ 327,  387}, { 638,   698}};
const int SyncBin_Mode3_Spec23 [4][2] = {{ 264,  324}, { 325,   385}, { 640,  700}, { 701,  761}};


const int Carrier_Mode1_Spec1  [2][2] = {{ 903, 1023}, {1025,  1145}};
const int Carrier_Mode1_Spec2  [4][2] = {{ 777,  897}, { 899,  1019}, {1029, 1149}, {1151, 1271}};
const int Carrier_Mode1_Spec9  [2][2] = {{ 527,  647}, {1401,  1521}};
const int Carrier_Mode1_Spec10 [4][2] = {{ 401,  521}, { 523,   643}, {1405, 1525}, {1527, 1647}};
const int Carrier_Mode1_Spec22 [2][2] = {{ 653,  773}, {1275,  1395}};
const int Carrier_Mode1_Spec23 [4][2] = {{ 527,  647}, { 649,   769}, {1279, 1399}, {1401, 1521}};

const int Carrier_Mode2_Spec1  [2][2] = {{ 451,  511}, { 513,   573}};
const int Carrier_Mode2_Spec2  [4][2] = {{ 389,  449}, { 451,   511}, { 513,  573}, { 575,  635}};
const int Carrier_Mode2_Spec9  [2][2] = {{ 263,  323}, { 701,   761}};
const int Carrier_Mode2_Spec10 [4][2] = {{ 201,  261}, { 263,   323}, { 701,  761}, { 763,  823}};
const int Carrier_Mode2_Spec22 [2][2] = {{ 325,  385}, { 639,   699}};
const int Carrier_Mode2_Spec23 [4][2] = {{ 263,  323}, { 325,   385}, { 639,  699}, { 701,  761}};

const int Carrier_Mode3_Spec1  [2][2] = {{ 903, 1023}, {1025,  1145}};
const int Carrier_Mode3_Spec2  [4][2] = {{ 777,  897}, { 899,  1019}, {1029, 1149}, {1151, 1271}};
const int Carrier_Mode3_Spec9  [2][2] = {{ 527,  647}, {1401,  1521}};
const int Carrier_Mode3_Spec10 [4][2] = {{ 401,  521}, { 523,   643}, {1405, 1525}, {1527, 1647}};
const int Carrier_Mode3_Spec22 [2][2] = {{ 653,  773}, {1275,  1395}};
const int Carrier_Mode3_Spec23 [4][2] = {{ 527,  647}, { 649,   769}, {1279, 1399}, {1401, 1521}};

const int Pilot_Start_Position [Pilot_InfoSize][2] = {{0, 0}, {8, 4}, {4, 8}};

const int Info_Position_Increase_Mode1 [2][5] = { {10,  44,  20,  28,  -1}, {22,  20,  28,  36,  -1}};
const int Info_Position_Increase_Mode2 [2][5] = { {14,  28,  -1,  -1,  -1}, {22,  20,  -1,  -1,  -1}};
const int Info_Position_Increase_Mode3 [2][5] = { {10,  44,  20,  28,  -1}, {22,  20,  28,  36,  -1}};

const int Info_Position_Mode1 [8] = { 11,  55,  75, 103, 144, 164, 192, 228};
const int Info_Position_Mode2 [4] = { 15,  43,  84, 104};
const int Info_Position_Mode3 [8] = { 11,  55,  75, 103, 144, 164, 192, 228};

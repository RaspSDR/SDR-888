#include "cdrDiscretePilot.h"

#include "cdrOfdmConst.h"
#include "cdrOFDMDemod.h"

#include <stdlib.h>

static void configPolitConst (carrierConst* handle, enum TransmissionMode transMode, enum SpectrumType specMode)
{
    int numOfHalfBand;
    int N_v;
    int pilot_length;
    int infoLength_half;
    const int (*validCarrier)[2];   
    const int (*infoCarrier)[5];    

    switch (transMode)
    {
        case TransMode2:
            N_v             = OFDM_Nv_Mode2;
            pilot_length    = Pilot_TotalBins_Mode2;
            infoCarrier     = Info_Position_Increase_Mode2;
            infoLength_half = OFDM_InfoLength_Mode2;
            switch (specMode)
            {
                case SpecMode1:
                    validCarrier  = Carrier_Mode2_Spec1;
                    break;
                case SpecMode2:
                    validCarrier  = Carrier_Mode2_Spec2;
                    break;
                case SpecMode10:
                    validCarrier  = Carrier_Mode2_Spec10;
                    break;
                case SpecMode22:
                    validCarrier  = Carrier_Mode2_Spec22;
                    break;
                case SpecMode23:
                    validCarrier  = Carrier_Mode2_Spec23;
                    break;
                case SpecMode9:
                default:
                    validCarrier  = Carrier_Mode2_Spec9;
                    break;
            }            
            break;
        case TransMode3:
            N_v             = OFDM_Nv_Mode3;
            pilot_length    = Pilot_TotalBins_Mode3;
            infoCarrier     = Info_Position_Increase_Mode3;
            infoLength_half = OFDM_InfoLength_Mode3;
            switch (specMode)
            {
                case SpecMode1:
                    validCarrier  = Carrier_Mode3_Spec1;
                    break;
                case SpecMode2:
                    validCarrier  = Carrier_Mode3_Spec2;
                    break;
                case SpecMode10:
                    validCarrier  = Carrier_Mode3_Spec10;
                    break;
                case SpecMode22:
                    validCarrier  = Carrier_Mode3_Spec22;
                    break;
                case SpecMode23:
                    validCarrier  = Carrier_Mode3_Spec23;
                    break;
                case SpecMode9:
                default:
                    validCarrier  = Carrier_Mode3_Spec9;
                    break;
            }            
            break;
        case TransMode1:           
        default:
            N_v             = OFDM_Nv_Mode1;
            pilot_length    = Pilot_TotalBins_Mode1;
            infoCarrier     = Info_Position_Increase_Mode1;
            infoLength_half = OFDM_InfoLength_Mode1;
            switch (specMode)
            {
                case SpecMode1:
                    validCarrier  = Carrier_Mode1_Spec1;
                    break;
                case SpecMode2:
                    validCarrier  = Carrier_Mode1_Spec2;
                    break;
                case SpecMode10:
                    validCarrier  = Carrier_Mode1_Spec10;
                    break;
                case SpecMode22:
                    validCarrier  = Carrier_Mode1_Spec22;
                    break;
                case SpecMode23:
                    validCarrier  = Carrier_Mode1_Spec23;
                    break;
                case SpecMode9:
                default:
                    validCarrier  = Carrier_Mode1_Spec9;
                    break;
            }
            break;
    }
    numOfHalfBand = getNumOfHalfBand (specMode);

    handle->numOfHalfBands       = numOfHalfBand;
    handle->validSubCarrier      = validCarrier;
    handle->N_v_all              = N_v / 2 * numOfHalfBand;
    handle->N_v_half             = N_v / 2;
    handle->N_v                  = N_v;

    handle->pilotGen.halfBandSize= pilot_length / 2;
    handle->pilotGen.totalSize   = pilot_length * numOfHalfBand / 2;

    handle->infoCarrier          = infoCarrier;
    handle->infoLength_half      = infoLength_half;
    return;
}

carrierConst* CarrierConst_Init (enum TransmissionMode transMode, enum SpectrumType specMode)
{
    carrierConst *handle = (carrierConst*) malloc (sizeof(carrierConst));

    configPolitConst (handle, transMode, specMode);

    return handle;
}

void CarrierConst_Release(carrierConst* handle)
{
    free (handle);
}

void CarrierConst_Reset (carrierConst* handle, enum TransmissionMode transMode, enum SpectrumType specMode)
{
    configPolitConst (handle, transMode, specMode);
}


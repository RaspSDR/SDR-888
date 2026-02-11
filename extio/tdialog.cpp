#include "framework.h"
#include "shellapi.h"
#include "tdialog.h"
#include <stdint.h>
#include <string.h>
#include <commctrl.h>
#include "ExtIO_sddc.h"
#include "extio_config.h"
#include "uti.h"
#include "LC_ExtIO_Types.h"

#include <windowsx.h>

extern HWND h_dialog;
extern int adcnominalfreq;
extern bool saveADCsamplesflag;
extern double gfFreqCorrectionPpm;

extern int SetOverclock(uint32_t adcfreq);

static COLORREF clrBackground = RGB(158, 188, 188);
static HBRUSH g_hbrBackground = CreateSolidBrush(clrBackground);
static unsigned int cntime = 0;

void UpdatePPM(HWND hWnd) {
    char lbuffer[64];
    sprintf(lbuffer, "%3.2f", gfFreqCorrectionPpm);
    SetWindowText(GetDlgItem(hWnd, IDC_EDIT2), lbuffer);
}

static void UpdateGain(HWND hControl, int current, const float* gains, int length) {
    char ebuffer[128];

    EnableWindow(hControl, length > 0);

    if (length > 0) {
        if (current >= length)
            current = length - 1;
        if (current < 0)
            current = 0;

        if (gains[current] >= 0)
            sprintf(ebuffer, "%+d", (int)(gains[current] + 0.5));
        else
            sprintf(ebuffer, "%+d", (int)(gains[current] - 0.5));
    }
    else {
        sprintf(ebuffer, "NA");
    }

    SetWindowText(hControl, ebuffer);
}

INT_PTR CALLBACK DlgMainFn(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    HICON hIcon = NULL;
    char ebuffer[32];

    switch (uMsg) {
    case WM_DESTROY:
        return TRUE;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return TRUE;

    case WM_INITDIALOG: {
        HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
        hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDB_ICON1));
        if (hIcon) {
            SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }
#ifndef _DEBUG
        ShowWindow(GetDlgItem(hWnd, IDC_ADCSAMPLES), FALSE);
#endif
        char lbuffer[64];
        sprintf(lbuffer, "%d", adcnominalfreq);
        SetWindowText(GetDlgItem(hWnd, IDC_EDIT1), lbuffer);
        UpdatePPM(hWnd);
        SetTimer(hWnd, 0, 200, NULL);
        return TRUE;
    }

    case WM_TIMER: {
        char lbuffer[64];
        if (cntime-- <= 0) {
            cntime = 5;
            sprintf(lbuffer, "%3.1fMsps", adcnominalfreq / 1000000.0f);
            SetWindowText(GetDlgItem(hWnd, IDC_STATIC13), lbuffer);
            sprintf(lbuffer, "%3.1fMsps", adcnominalfreq * 2.0f / 1000000.0f);
            SetWindowText(GetDlgItem(hWnd, IDC_STATIC14), lbuffer);
            sprintf(lbuffer, "%3.1fMsps", adcnominalfreq / 2.0f / 1000000.0f);
            SetWindowText(GetDlgItem(hWnd, IDC_STATIC16), lbuffer);
        }

        if (GetStateButton(hWnd, IDC_IFGAINP)) {
            Command(hWnd, IDC_IFGAINP, BN_CLICKED);
        }

        if (GetStateButton(hWnd, IDC_IFGAINM)) {
            Command(hWnd, IDC_IFGAINM, BN_CLICKED);
        }

        if (GetStateButton(hWnd, IDC_RFGAINP)) {
            Command(hWnd, IDC_RFGAINP, BN_CLICKED);
        }

        if (GetStateButton(hWnd, IDC_RFGAINM)) {
            Command(hWnd, IDC_RFGAINM, BN_CLICKED);
        }
        break;
    }

    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSCROLLBAR:
    case WM_CTLCOLORSTATIC: {
        HDC hDc = (HDC)wParam;
        SetBkMode(hDc, TRANSPARENT);
        return (LONG)g_hbrBackground;
    }

    case WM_USER + 1: {
        switch (wParam) {
        case extHw_READY:
            if (!bSupportDynamicSRate) {
                EnableWindow(GetDlgItem(hWnd, IDC_BANDWIDTH), TRUE);
                EnableWindow(GetDlgItem(hWnd, IDC_OVERCLOCK), TRUE);
            }
            break;
        case extHw_RUNNING:
            if (!bSupportDynamicSRate) {
                EnableWindow(GetDlgItem(hWnd, IDC_BANDWIDTH), FALSE);
                EnableWindow(GetDlgItem(hWnd, IDC_OVERCLOCK), FALSE);
            }
            break;
        case extHw_Changed_MGC:
        case extHw_Changed_ATT:
        case extHw_Changed_RF_IF: {
            float gains[EXTIO_MAX_ATT_GAIN_VALUES];
            int length = 0;
            for (; length < EXTIO_MAX_ATT_GAIN_VALUES; ++length) {
                if (GetAttenuators(length, &gains[length]) != 0) {
                    break;
                }
            }
            UpdateGain(GetDlgItem(hWnd, IDC_RFGAIN), GetActualAttIdx(), gains, length);

            float mgc[EXTIO_MAX_MGC_VALUES];
            int mgc_len = 0;
            for (; mgc_len < EXTIO_MAX_MGC_VALUES; ++mgc_len) {
                if (ExtIoGetMGCs(mgc_len, &mgc[mgc_len]) != 0) {
                    break;
                }
            }
            UpdateGain(GetDlgItem(hWnd, IDC_IFGAIN), ExtIoGetActualMgcIdx(), mgc, mgc_len);
            break;
        }
        case extHw_Changed_SRATES: {
            double rate;
            ComboBox_ResetContent(GetDlgItem(hWnd, IDC_BANDWIDTH));
            for (int i = 0;; i++) {
                if (ExtIoGetSrates(i, &rate) == -1) {
                    break;
                }
                sprintf(ebuffer, "%.0fM", rate / 1000000);
                ComboBox_InsertString(GetDlgItem(hWnd, IDC_BANDWIDTH), i, ebuffer);
            }
        }
        case extHw_Changed_SampleRate: {
            int index = ExtIoGetActualSrateIdx();
            double rate;
            ExtIoGetSrates(index, &rate);
            sprintf(ebuffer, "%.0fM", rate / 1000000);
            ComboBox_SelectItemData(GetDlgItem(hWnd, IDC_BANDWIDTH), -1, ebuffer);
            RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
            break;
        }
        }
        break;
    }

    case WM_NOTIFY: {
        UINT uNotify = ((LPNMHDR)lParam)->code;
        switch (uNotify) {
        case NM_CLICK:
        case NM_RETURN: {
            if (((LPNMHDR)lParam)->hwndFrom == GetDlgItem(hWnd, IDC_SYSLINK41)) {
                ShellExecute(NULL, "open", URL1, NULL, NULL, SW_SHOW);
            }
            if (((LPNMHDR)lParam)->hwndFrom == GetDlgItem(hWnd, IDC_SYSLINK42)) {
                ShellExecute(NULL, "open", URL_HDSR, NULL, NULL, SW_SHOW);
            }
            break;
        }
        }
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_DITHER:
            if (HIWORD(wParam) == BN_CLICKED) {
                ExtIoSetSetting(21, GetStateButton(hWnd, IDC_DITHER) ? "1" : "0");
            }
            break;
        case IDC_PGA:
            if (HIWORD(wParam) == BN_CLICKED) {
                ExtIoSetSetting(22, GetStateButton(hWnd, IDC_PGA) ? "1" : "0");
            }
            break;
        case IDC_RAND:
            if (HIWORD(wParam) == BN_CLICKED) {
                ExtIoSetSetting(23, GetStateButton(hWnd, IDC_RAND) ? "1" : "0");
            }
            break;
        case IDC_BIAS_HF:
            if (HIWORD(wParam) == BN_CLICKED) {
                ExtIoSetSetting(24, GetStateButton(hWnd, IDC_BIAS_HF) ? "1" : "0");
                ExtIoSetSetting(25, GetStateButton(hWnd, IDC_BIAS_VHF) ? "1" : "0");
            }
            break;
        case IDC_BIAS_VHF:
            if (HIWORD(wParam) == BN_CLICKED) {
                ExtIoSetSetting(24, GetStateButton(hWnd, IDC_BIAS_HF) ? "1" : "0");
                ExtIoSetSetting(25, GetStateButton(hWnd, IDC_BIAS_VHF) ? "1" : "0");
            }
            break;
        case IDC_RFGAINP:
            if (HIWORD(wParam) == BN_CLICKED) {
                float gains[EXTIO_MAX_ATT_GAIN_VALUES];
                int length = 0;
                for (; length < EXTIO_MAX_ATT_GAIN_VALUES; ++length) {
                    if (GetAttenuators(length, &gains[length]) != 0) {
                        break;
                    }
                }
                int index = GetActualAttIdx();
                index += 1;
                if (index >= 0 && index < length) {
                    SetAttenuator(index);
                }
                UpdateGain(GetDlgItem(hWnd, IDC_RFGAIN), GetActualAttIdx(), gains, length);
            }
            break;
        case IDC_RFGAINM:
            if (HIWORD(wParam) == BN_CLICKED) {
                float gains[EXTIO_MAX_ATT_GAIN_VALUES];
                int length = 0;
                for (; length < EXTIO_MAX_ATT_GAIN_VALUES; ++length) {
                    if (GetAttenuators(length, &gains[length]) != 0) {
                        break;
                    }
                }
                int index = GetActualAttIdx();
                index -= 1;
                if (index >= 0 && index < length) {
                    SetAttenuator(index);
                }
                UpdateGain(GetDlgItem(hWnd, IDC_RFGAIN), GetActualAttIdx(), gains, length);
            }
            break;
        case IDC_IFGAINP:
            if (HIWORD(wParam) == BN_CLICKED) {
                float gains[EXTIO_MAX_MGC_VALUES];
                int length = 0;
                for (; length < EXTIO_MAX_MGC_VALUES; ++length) {
                    if (ExtIoGetMGCs(length, &gains[length]) != 0) {
                        break;
                    }
                }
                int index = ExtIoGetActualMgcIdx();
                index += 1;
                if (index >= 0 && index < length) {
                    ExtIoSetMGC(index);
                }
                UpdateGain(GetDlgItem(hWnd, IDC_IFGAIN), ExtIoGetActualMgcIdx(), gains, length);
            }
            break;
        case IDC_IFGAINM:
            if (HIWORD(wParam) == BN_CLICKED) {
                float gains[EXTIO_MAX_MGC_VALUES];
                int length = 0;
                for (; length < EXTIO_MAX_MGC_VALUES; ++length) {
                    if (ExtIoGetMGCs(length, &gains[length]) != 0) {
                        break;
                    }
                }
                int index = ExtIoGetActualMgcIdx();
                index -= 1;
                if (index >= 0 && index < length) {
                    ExtIoSetMGC(index);
                }
                UpdateGain(GetDlgItem(hWnd, IDC_IFGAIN), ExtIoGetActualMgcIdx(), gains, length);
            }
            break;
        case IDC_BANDWIDTH:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                int index = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_BANDWIDTH));
                ExtIoSetSrate(index);
            }
            break;
        case IDC_OVERCLOCK:
            if (HIWORD(wParam) == BN_CLICKED) {
                if (Button_GetCheck(GetDlgItem(hWnd, IDC_OVERCLOCK)) == BST_CHECKED) {
                    SetOverclock(DEFAULT_ADC_FREQ * 2);
                }
                else {
                    SetOverclock(DEFAULT_ADC_FREQ);
                }
                char lbuffer[64];
                sprintf(lbuffer, "%d", adcnominalfreq);
                SetWindowText(GetDlgItem(hWnd, IDC_EDIT1), lbuffer);
            }
            break;
        case IDC_ADCSAMPLES:
            if (HIWORD(wParam) == BN_CLICKED) {
                saveADCsamplesflag = true;
            }
            break;
        case IDC_FREQAPPLY:
            if (HIWORD(wParam) == BN_CLICKED) {
                char lbuffer[64];
                uint32_t adcfreq = 0;
                GetWindowText(GetDlgItem(hWnd, IDC_EDIT1), lbuffer, sizeof(lbuffer));
                sscanf(lbuffer, "%d", &adcfreq);
                if (adcfreq > MAX_ADC_FREQ) adcfreq = MAX_ADC_FREQ;
                if (adcfreq < MIN_ADC_FREQ) adcfreq = MIN_ADC_FREQ;
                sprintf(lbuffer, "%d", adcfreq);
                SetWindowText(GetDlgItem(hWnd, IDC_EDIT1), lbuffer);
                SetOverclock(adcfreq);
            }
            break;
        case IDC_FREQCANC:
            if (HIWORD(wParam) == BN_CLICKED) {
                char lbuffer[64];
                sprintf(lbuffer, "%d", adcnominalfreq);
                SetWindowText(GetDlgItem(hWnd, IDC_EDIT1), lbuffer);
            }
            break;
        case IDC_CORRUPDATE:
            if (HIWORD(wParam) == BN_CLICKED) {
                char lbuffer[64];
                float adjppm;
                float maxppm = 200.0;
                GetWindowText(GetDlgItem(hWnd, IDC_EDIT2), lbuffer, sizeof(lbuffer));
                sscanf(lbuffer, "%f", &adjppm);
                if (adjppm > maxppm) adjppm = maxppm;
                if (adjppm < -maxppm) adjppm = -maxppm;
                sprintf(lbuffer, "%3.2f", adjppm);
                SetPPMvalue(adjppm);
                SetWindowText(GetDlgItem(hWnd, IDC_EDIT2), lbuffer);
            }
            break;
        case IDC_CORRCANC:
            if (HIWORD(wParam) == BN_CLICKED) {
                char lbuffer[64];
                sprintf(lbuffer, "%.2f", gfFreqCorrectionPpm);
                SetWindowText(GetDlgItem(hWnd, IDC_EDIT2), lbuffer);
            }
            break;
        }
        break;
    }

    return FALSE;
}

INT_PTR CALLBACK DlgSelectDevice(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    int selindex = 0;
    DevContext* p_devicelist;

    switch (uMsg) {
    case WM_INITDIALOG:
        p_devicelist = (DevContext*)lParam;
        for (int i = 0; i < p_devicelist->numdev; i++) {
            ListBox_AddString(GetDlgItem(hWnd, IDC_LISTDEV), p_devicelist->dev[i]);
        }
        break;

    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSCROLLBAR:
    case WM_CTLCOLORSTATIC: {
        HDC hDc = (HDC)wParam;
        SetBkMode(hDc, TRANSPARENT);
        return (LONG)g_hbrBackground;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        case IDCANCEL:
            selindex = ListBox_GetCurSel(GetDlgItem(hWnd, IDC_LISTDEV));
            if (selindex < 0) selindex = 0;
            EndDialog(hWnd, selindex);
            break;

        case IDC_LISTDEV:
            switch (HIWORD(wParam)) {
            case LBN_SELCHANGE:
                EnableWindow(GetDlgItem(hWnd, IDOK), TRUE);
                break;
            case LBN_DBLCLK:
                selindex = ListBox_GetCurSel(GetDlgItem(hWnd, IDC_LISTDEV));
                if (selindex < 0) selindex = 0;
                EndDialog(hWnd, selindex);
                break;
            }
            break;
        }
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

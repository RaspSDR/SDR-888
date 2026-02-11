#pragma once

#include "framework.h"
#include "resource.h"
#include "extio_config.h"
#include <stdio.h>

extern bool bSupportDynamicSRate;
extern void UpdatePPM(HWND hWnd);

INT_PTR CALLBACK DlgMainFn(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DlgSelectDevice(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

struct DevContext {
    unsigned char numdev;
    char dev[MAXNDEV][MAXDEVSTRLEN];
};

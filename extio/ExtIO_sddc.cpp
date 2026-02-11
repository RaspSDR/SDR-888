#define EXTIO_EXPORTS

#include "framework.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <mutex>
#include <string>
#include <algorithm>

#include "extio_config.h"
#include "ExtIO_sddc.h"
#include "uti.h"
#include "tdialog.h"
#include "SplashWindow.h"
#include "fft_rx_vfo.h"

#include <config.h>
#include <utils/optionlist.h>
#include <utils/flog.h>
#include <json.hpp>
#include "libsddc.h"

#define snprintf _snprintf

#define EXTIO_STATUS_CHANGE(cb, status) do { if (cb) { cb(-1, status, 0.0F, 0); } } while (0)

#define SDDC_ACCUMRATE_BUFFER_COUNT 41
#define SDDC_BUFFER_SIZE            (16 * 1024 / 2)
#define TUNER_IF_FREQUENCY          4570000.0

enum Model {
    MODEL_RX888,
    MODEL_RX888mk2,
    MODEL_RX888PRO,
    MODEL_COUNT
};

static const float GainFactor[MODEL_COUNT] = {
    2.28e-4f,
    3.54e-4f,
    2.28e-4f,
};

static const uint32_t RefClockFreq[MODEL_COUNT] = {
    27000000,
    27000000,
    24576000
};

static const int gHwType = exthwUSBfloat32;
static char SDR_progname[2048 + 1] = "\0";
static int SDR_ver_major = -1;
static int SDR_ver_minor = -1;

static double gfLOfreq = 2000000.0;
#if EXPORT_EXTIO_TUNE_FUNCTIONS
static double gfTunefreq = DEFAULT_TUNE_FREQ;
#endif
static double gfFreqCorrectionPpm = 0.0;

static bool gbInitHW = false;
static bool extio_running = false;

static pfnExtIOCallback pfnCallback = nullptr;
static HWND Hconsole = nullptr;
static HWND h_dialog = nullptr;
static HMODULE hInst = nullptr;

static ConfigManager extioConfig;
static OptionList<std::string, int> devices;
static OptionList<std::string, int> ports;
static OptionList<int, uint32_t> xtalrates;
static OptionList<int, double> samplerates;

static int selectedDevId = 0;
static std::string selectedSerial;
static int model = MODEL_RX888;

static bool ext_clock = false;
static uint32_t ext_clock_freq = 27000000;
static uint32_t clock_freq = 27000000;

static uint32_t xtal_freq = 122880000;
static int xtalId = 0;

static double sampleRate = 128 * 1000 * 1000.0;
static int srId = 0;

enum Port {
    PORT_VHF,
    PORT_HF,
    PORT_FM,
    PORT_BYPASS
};

static Port port = PORT_HF;
static int portId = 0;

static int rfGainIdx = 0;
static int ifGainIdx = 0;
static int rf_steps = 0;
static int if_steps = 0;
static const float* rf_gain_steps = nullptr;
static const float* if_gain_steps = nullptr;

static bool bias = false;
static bool pga = false;
static bool dither = false;
static bool rando = false;
static bool highz = false;
static bool anti_alias = true;

static int adcnominalfreq = DEFAULT_ADC_FREQ;
static bool needSaveSettings = false;
static bool saveADCsamplesflag = false;

static int giExtSrateIdxVHF = 0;
static int giExtSrateIdxHF = 0;
static int giAttIdxHF = 0;
static int giAttIdxVHF = 0;
static int giMgcIdxHF = 0;
static int giMgcIdxVHF = 0;

bool bSupportDynamicSRate = false;

static sddc_dev_t* openDev = nullptr;
static int buffercount = 0;
static dsp::stream<int16_t> dataIn;
static dsp::channel::FFTRxVFO ddc;
static dsp::sink::Handler<dsp::complex_t> iqSink;
static bool iqSinkInit = false;
static bool ddcInit = false;
static float ddcGainFactor = GainFactor[MODEL_RX888];

static SplashWindow splashW;

static inline double FreqCorrectionFactor() {
    return 1.0 + gfFreqCorrectionPpm / 1E6;
}

static void loadConfig() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    extioConfig.setPath(".\\sddc_config.json");
    extioConfig.load(def);
}

static void saveConfig() {
    extioConfig.save();
}

static std::string getBandwdithScaled(double bw) {
    char buf[1024];
    if (bw >= 1000000.0) {
        snprintf(buf, 1024, "%.2lfMHz", bw / 1000000.0);
    }
    else if (bw >= 1000.0) {
        snprintf(buf, 1024, "%.2lfKHz", bw / 1000.0);
    }
    else {
        snprintf(buf, 1024, "%.2lfHz", bw);
    }
    return std::string(buf);
}

static void refreshDevices() {
    devices.clear();
    int devCount = sddc_get_device_count();
    if (!devCount) {
        return;
    }
    for (int i = 0; i < devCount; ++i) {
        char serial[256];
        int err = sddc_get_device_usb_strings(i, NULL, NULL, serial);
        if (err == 0) {
            devices.define(std::string(serial), std::string(serial), i);
        }
    }
}

static void selectDevice(const std::string& serial) {
    if (devices.empty()) {
        selectedSerial.clear();
        return;
    }

    int index = sddc_get_index_by_serial(serial.c_str());
    if (index < 0) {
        selectedDevId = devices[0];
        selectedSerial = devices.key(0);
    }
    else {
        selectedDevId = index;
        selectedSerial = serial;
    }

    sddc_dev_t* dev = nullptr;
    int err = sddc_open(&dev, selectedDevId);
    if (err) {
        flog::error("ExtIO: Failed to open device: {}", err);
        return;
    }

    char product[256] = { 0 };
    sddc_get_usb_strings(dev, NULL, product, NULL);
    model = MODEL_RX888;
    if (strstr(product, "RX888mk2")) {
        model = MODEL_RX888mk2;
    }
    else if (strstr(product, "RX888pro")) {
        model = MODEL_RX888PRO;
    }
    else if (strstr(product, "RX888")) {
        model = MODEL_RX888;
    }

    ports.clear();
    ports.define("hf", "HF", PORT_HF);
    ports.define("vhf", "VHF", PORT_VHF);
    if (model == MODEL_RX888PRO) {
        ports.define("fm", "FM", PORT_FM);
        ports.define("bypass", "Bypass", PORT_BYPASS);
    }

    float gainFactor = GainFactor[model];
    ddcGainFactor = gainFactor;
    if (ddcInit) {
        ddc.setGainFactor(gainFactor);
    }

    uint32_t ref_clock_freq = RefClockFreq[model];
    if (ext_clock) {
        clock_freq = ext_clock_freq;
    }
    else {
        clock_freq = ref_clock_freq;
        ext_clock_freq = clock_freq;
    }
    if (clock_freq < 10000000) {
        clock_freq = ref_clock_freq;
    }

    port = PORT_HF;
    portId = ports.valueId(port);
    rfGainIdx = 0;
    ifGainIdx = 0;
    bias = false;
    highz = false;

    extioConfig.acquire();
    if (extioConfig.conf["devices"][selectedSerial].contains("port")) {
        std::string desiredPort = extioConfig.conf["devices"][selectedSerial]["port"];
        if (ports.keyExists(desiredPort)) {
            portId = ports.keyId(desiredPort);
            port = ports[portId];
        }
    }

    xtalrates.clear();
    if (ref_clock_freq == 27000000) {
        uint32_t xtal_freq0 = 122880000;
        xtalrates.define(xtal_freq0, getBandwdithScaled(xtal_freq0), xtal_freq0);
        xtalrates.define(xtal_freq0 / 2, getBandwdithScaled(xtal_freq0 / 2), xtal_freq0 / 2);
    }
    else {
        int c = clock_freq * 80 / 16;
        xtalrates.define(c, getBandwdithScaled(c), c);
        xtalrates.define(c / 2, getBandwdithScaled(c / 2), c / 2);
        xtalrates.define(clock_freq * 3, getBandwdithScaled(clock_freq * 3), clock_freq * 3);
    }

    if (extioConfig.conf["devices"][selectedSerial].contains("xtal_freq")) {
        xtal_freq = extioConfig.conf["devices"][selectedSerial]["xtal_freq"];
    }

    if (xtalrates.valueExists(xtal_freq)) {
        xtalId = xtalrates.valueId(xtal_freq);
    }
    else {
        xtalId = 1;
        xtal_freq = xtalrates[xtalId];
    }

    sddc_set_direct_sampling(dev, (port != PORT_VHF) ? 1 : 0);

    if (port != PORT_VHF) {
        uint32_t sampleRate0 = xtal_freq / 2;
        samplerates.clear();
        for (int i = 1; i <= 6; ++i) {
            samplerates.define(sampleRate0, getBandwdithScaled(sampleRate0), sampleRate0);
            sampleRate0 /= 2;
        }
        if (!samplerates.valueExists(sampleRate)) {
            sampleRate = samplerates.key(1);
        }
        srId = samplerates.valueId(sampleRate);
    }
    else {
        uint32_t sampleRate0 = (xtal_freq / 8 > 8e8) ? (xtal_freq / 16) : (xtal_freq / 8);
        samplerates.clear();
        for (int i = 1; i <= 4; ++i) {
            samplerates.define(sampleRate0, getBandwdithScaled(sampleRate0), sampleRate0);
            sampleRate0 /= 2;
        }
        if (!samplerates.valueExists(sampleRate)) {
            sampleRate = samplerates.key(1);
        }
        srId = samplerates.valueId(sampleRate);
    }

    if (extioConfig.conf["devices"][selectedSerial].contains("samplerate")) {
        int desiredSr = extioConfig.conf["devices"][selectedSerial]["samplerate"];
        if (samplerates.keyExists(desiredSr)) {
            srId = samplerates.keyId(desiredSr);
            sampleRate = samplerates[srId];
        }
    }

    rf_steps = sddc_get_rf_gain_steps(dev, &rf_gain_steps);
    if (extioConfig.conf["devices"][selectedSerial].contains("rfGain")) {
        rfGainIdx = std::clamp<int>(extioConfig.conf["devices"][selectedSerial]["rfGain"], 0, rf_steps - 1);
    }

    if_steps = sddc_get_if_gain_steps(dev, &if_gain_steps);
    if (extioConfig.conf["devices"][selectedSerial].contains("ifGain")) {
        ifGainIdx = std::clamp<int>(extioConfig.conf["devices"][selectedSerial]["ifGain"], 0, if_steps - 1);
    }

    if (extioConfig.conf["devices"][selectedSerial].contains("bias")) {
        bias = extioConfig.conf["devices"][selectedSerial]["bias"];
    }
    if (extioConfig.conf["devices"][selectedSerial].contains("pga")) {
        pga = extioConfig.conf["devices"][selectedSerial]["pga"];
    }
    if (extioConfig.conf["devices"][selectedSerial].contains("highz")) {
        highz = extioConfig.conf["devices"][selectedSerial]["highz"];
    }
    if (extioConfig.conf["devices"][selectedSerial].contains("rando")) {
        rando = extioConfig.conf["devices"][selectedSerial]["rando"];
    }
    if (extioConfig.conf["devices"][selectedSerial].contains("dither")) {
        dither = extioConfig.conf["devices"][selectedSerial]["dither"];
    }
    extioConfig.release();

    sddc_close(dev);

    adcnominalfreq = static_cast<int>(xtal_freq);

    giExtSrateIdxHF = srId;
    giExtSrateIdxVHF = srId;

    EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_RF_IF);
    if (h_dialog) {
        SendMessage(h_dialog, WM_USER + 1, extHw_Changed_RF_IF, 0);
    }
    EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SRATES);
    if (h_dialog) {
        SendMessage(h_dialog, WM_USER + 1, extHw_Changed_SRATES, 0);
    }
    EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SampleRate);
    if (h_dialog) {
        SendMessage(h_dialog, WM_USER + 1, extHw_Changed_SampleRate, 0);
    }
}

static void applyDeviceSettings() {
    if (!openDev) {
        return;
    }

    sddc_set_xtal_freq(openDev, xtal_freq);

    if (port != PORT_VHF) {
        sddc_set_direct_sampling(openDev, 1);
        sddc_enable_bias_tee(openDev, bias ? 1 : 0);
        if (model == MODEL_RX888PRO) {
            sddc_enable_hf_highz(openDev, highz ? 1 : 0);
            switch (port) {
            case PORT_HF:
                if (xtal_freq > 64e8) {
                    sddc_set_adc_filter(openDev, Freq64MHz);
                }
                else {
                    sddc_set_adc_filter(openDev, Freq32MHz);
                }
                break;
            case PORT_FM:
                sddc_set_adc_filter(openDev, FMUndersample);
                break;
            case PORT_BYPASS:
                sddc_set_adc_filter(openDev, Bypass);
                break;
            default:
                break;
            }
        }

        ddc.setInSamplerate(xtal_freq);
        ddc.setOutSamplerate(sampleRate, sampleRate);
        if (port == PORT_HF) {
            ddc.setOffset(-TUNER_IF_FREQUENCY);
        }
        else {
            ddc.setOffset(gfLOfreq);
        }
        ddc.start();
        iqSink.start();
    }
    else {
        sddc_set_direct_sampling(openDev, 0);
        sddc_enable_bias_tee(openDev, bias ? 0x02 : 0);
        sddc_set_center_freq64(openDev, static_cast<uint64_t>(gfLOfreq));

        ddc.setInSamplerate(xtal_freq);
        ddc.setOutSamplerate(sampleRate, sampleRate);
        ddc.setOffset(-TUNER_IF_FREQUENCY);
        ddc.start();
        iqSink.start();
    }

    sddc_enable_adc_pga(openDev, pga ? 1 : 0);
    sddc_enable_adc_dither(openDev, dither ? 1 : 0);
    sddc_enable_adc_rando(openDev, rando ? 1 : 0);

    if (rf_steps > 0) {
        sddc_set_rf_gain(openDev, rf_gain_steps[rfGainIdx]);
    }
    if (if_steps > 0) {
        sddc_set_if_gain(openDev, if_gain_steps[ifGainIdx]);
    }

    ddc.setAntiAlias(anti_alias);
}

static void stopStream() {
    if (!openDev) {
        return;
    }

    dataIn.stopWriter();
    sddc_cancel_async(openDev);
    dataIn.clearWriteStop();

    iqSink.stop();
    ddc.stop();
    sddc_close(openDev);
    openDev = nullptr;
    extio_running = false;
}

static void sddc_async_callback(const int16_t* buffer, uint32_t count, void* ctx) {
    if (!extio_running) {
        return;
    }

    if (count != SDDC_BUFFER_SIZE) {
        return;
    }

    memcpy(dataIn.writeBuf + buffercount * SDDC_BUFFER_SIZE, buffer, count * sizeof(int16_t));
    buffercount++;
    if (buffercount == SDDC_ACCUMRATE_BUFFER_COUNT) {
        dataIn.swap(SDDC_BUFFER_SIZE * SDDC_ACCUMRATE_BUFFER_COUNT);
        buffercount = 0;
    }
}

static void iq_callback(dsp::complex_t* data, int count, void* ctx) {
    if (!pfnCallback) {
        return;
    }
    pfnCallback(count, 0, 0.0F, reinterpret_cast<void*>(data));
}

extern "C"
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        hInst = hModule;
        break;
    default:
        break;
    }
    return TRUE;
}

extern "C" bool EXTIO_API InitHW(char* name, char* modelName, int& type) {
    type = gHwType;
    if (!gbInitHW) {
        splashW.createSplashWindow(hInst, IDB_BITMAP2, 15, 15, 15);
        refreshDevices();
        if (devices.empty()) {
            MessageBox(NULL, "No SDDC device found", "WARNING", MB_OK | MB_ICONWARNING);
            return false;
        }

        loadConfig();
        extioConfig.acquire();
        std::string devSerial = extioConfig.conf["device"];
        extioConfig.release();
        selectDevice(devSerial);
        if (!selectedSerial.empty()) {
            extioConfig.acquire();
            extioConfig.conf["device"] = selectedSerial;
            extioConfig.release(true);
        }

        dataIn.setBufferSize(SDDC_BUFFER_SIZE * SDDC_ACCUMRATE_BUFFER_COUNT);
        ddc.init(&dataIn, 0.1f);
        ddc.setBufferSize(SDDC_BUFFER_SIZE * SDDC_ACCUMRATE_BUFFER_COUNT);
        ddcInit = true;
        ddc.setGainFactor(ddcGainFactor);
        if (!iqSinkInit) {
            iqSink.init(&ddc.out, &iq_callback, nullptr);
            iqSinkInit = true;
        }
        else {
            iqSink.setInput(&ddc.out);
        }
        gbInitHW = true;
        strcpy(name, "RX-888");
        strcpy(modelName, "RX-888");
    }

    EXTIO_STATUS_CHANGE(pfnCallback, extHw_READY);
    if (h_dialog) {
        SendMessage(h_dialog, WM_USER + 1, extHw_READY, 0);
    }
    return gbInitHW;
}

extern "C" bool EXTIO_API OpenHW(void) {
    if (!gbInitHW) {
        return false;
    }

#ifndef _DEBUG
    splashW.showWindow();
#endif

    splashW.destroySplashWindow();

    if (strstr(SDR_progname, "HDSDR") == nullptr) {
        h_dialog = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DLG_MAIN), NULL, (DLGPROC)&DlgMainFn);
        needSaveSettings = true;
        LoadSettings();
    }
    else {
        if ((SDR_ver_major >= 2) && (SDR_ver_minor >= 81))
            h_dialog = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DLG_HDSDR281), NULL, (DLGPROC)&DlgMainFn);
        else
            h_dialog = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DLG_HDSDR), NULL, (DLGPROC)&DlgMainFn);
    }

    RECT rect;
    GetWindowRect(h_dialog, &rect);
    SetWindowPos(h_dialog, HWND_TOPMOST, 0, 24, rect.right - rect.left, rect.bottom - rect.top, SWP_HIDEWINDOW);

    return gbInitHW;
}

extern "C" int EXTIO_API StartHW(long LOfreq) {
    return StartHWdbl((double)LOfreq);
}

extern "C" int EXTIO_API StartHW64(int64_t LOfreq) {
    return StartHWdbl((double)LOfreq);
}

extern "C" int EXTIO_API StartHWdbl(double LOfreq) {
    if (!gbInitHW) {
        return 0;
    }

    if (openDev) {
        stopStream();
    }

    int err = sddc_open(&openDev, selectedDevId);
    if (err) {
        MessageBox(NULL, "SDDC device open failed", "WARNING", MB_OK | MB_ICONWARNING);
        EXTIO_STATUS_CHANGE(pfnCallback, extHw_Disconnected);
        if (h_dialog) {
            SendMessage(h_dialog, WM_USER + 1, extHw_Disconnected, 0);
        }
        return 0;
    }

    gfLOfreq = LOfreq;
    applyDeviceSettings();

    buffercount = 0;
    extio_running = true;

    sddc_read_async(openDev, &sddc_async_callback, nullptr);

    EXTIO_STATUS_CHANGE(pfnCallback, extHw_RUNNING);
    if (h_dialog) {
        SendMessage(h_dialog, WM_USER + 1, extHw_RUNNING, 0);
    }

    return (int)(SDDC_BUFFER_SIZE * SDDC_ACCUMRATE_BUFFER_COUNT / 4);
}

extern "C" void EXTIO_API StopHW(void) {
    stopStream();
    EXTIO_STATUS_CHANGE(pfnCallback, extHw_READY);
    if (h_dialog) {
        SendMessage(h_dialog, WM_USER + 1, extHw_READY, 0);
    }
}

extern "C" void EXTIO_API CloseHW(void) {
    if (h_dialog != NULL) {
        DestroyWindow(h_dialog);
        h_dialog = NULL;
    }
    stopStream();
    if (needSaveSettings) {
        SaveSettings();
    }
    gbInitHW = false;
}

extern "C" void EXTIO_API ShowGUI() {
    ShowWindow(h_dialog, SW_SHOW);
    SetForegroundWindow(h_dialog);
}

extern "C" void EXTIO_API HideGUI() {
    ShowWindow(h_dialog, SW_HIDE);
}

extern "C" void EXTIO_API SwitchGUI() {
    if (IsWindowVisible(h_dialog)) {
        ShowWindow(h_dialog, SW_HIDE);
    }
    else {
        ShowWindow(h_dialog, SW_SHOW);
    }
}

extern "C" int EXTIO_API SetHWLO(long LOfreq) {
    double retDbl = SetHWLOdbl((double)LOfreq);
    int64_t ret = (int64_t)retDbl;
    return (ret & 0xFFFFFFFF);
}

extern "C" int64_t EXTIO_API SetHWLO64(int64_t LOfreq) {
    double retDbl = SetHWLOdbl((double)LOfreq);
    int64_t ret = (int64_t)retDbl;
    return (ret & 0xFFFFFFFF);
}

extern "C" double EXTIO_API SetHWLOdbl(double LOfreq) {
    gfLOfreq = LOfreq;
    if (openDev) {
        if (port == PORT_VHF) {
            sddc_set_center_freq64(openDev, static_cast<uint64_t>(gfLOfreq));
        }
        else {
            switch (port) {
            case PORT_FM: {
                double freq = gfLOfreq - xtal_freq;
                if (freq > 0 && freq < xtal_freq / 2) {
                    ddc.setOffset(freq);
                }
                break;
            }
            default:
                if (gfLOfreq < xtal_freq / 2) {
                    ddc.setOffset(gfLOfreq);
                }
                break;
            }
        }
    }
    return gfLOfreq;
}

extern "C" int EXTIO_API GetStatus(void) {
    return 0;
}

extern "C" void EXTIO_API SetCallback(pfnExtIOCallback funcptr) {
    pfnCallback = funcptr;
}

extern "C" long EXTIO_API GetHWLO(void) {
    int64_t ret = (int64_t)gfLOfreq;
    return (long)(ret & 0xFFFFFFFF);
}

extern "C" int64_t EXTIO_API GetHWLO64(void) {
    return (int64_t)gfLOfreq;
}

extern "C" double EXTIO_API GetHWLOdbl(void) {
    return gfLOfreq;
}

extern "C" long EXTIO_API GetHWSR(void) {
    double srate = GetHWSRdbl();
    return (long)srate;
}

extern "C" double EXTIO_API GetHWSRdbl(void) {
    double newSrate = sampleRate;
    return newSrate;
}

#if EXPORT_EXTIO_TUNE_FUNCTIONS
extern "C" long EXTIO_API GetTune(void) {
    int64_t ret = (int64_t)gfTunefreq;
    return (long)(ret & 0xFFFFFFFF);
}

extern "C" int64_t EXTIO_API GetTune64(void) {
    int64_t ret = (int64_t)gfTunefreq;
    return ret;
}

extern "C" double EXTIO_API GetTunedbl(void) {
    int64_t ret = (int64_t)gfTunefreq;
    return gfTunefreq;
}

extern "C" void EXTIO_API TuneChanged(long freq) {
    TuneChanged64(freq);
}

extern "C" void EXTIO_API TuneChanged64(int64_t freq) {
    gfTunefreq = (double)freq;
}

extern "C" void EXTIO_API TuneChangeddbl(double freq) {
    gfTunefreq = freq;
}
#endif

extern "C" void EXTIO_API VersionInfo(const char* progname, int ver_major, int ver_minor) {
    SDR_progname[0] = 0;
    SDR_ver_major = -1;
    SDR_ver_minor = -1;

    if (progname) {
        strncpy(SDR_progname, progname, sizeof(SDR_progname) - 1);
        SDR_ver_major = ver_major;
        SDR_ver_minor = ver_minor;

        if (strcmp(SDR_progname, "HDSDR") == 0)
            bSupportDynamicSRate = true;
        else
            bSupportDynamicSRate = false;
    }
}

extern "C" int EXTIO_API GetAttenuators(int atten_idx, float* attenuation) {
    if (!rf_gain_steps || rf_steps <= 0) {
        return 1;
    }
    if (atten_idx < rf_steps) {
        if (attenuation) {
            *attenuation = rf_gain_steps[atten_idx];
        }
        return 0;
    }
    return 1;
}

extern "C" int EXTIO_API GetActualAttIdx(void) {
    int AttIdx = (port == PORT_VHF) ? giAttIdxVHF : giAttIdxHF;
    if (rf_steps > 0 && AttIdx >= rf_steps) {
        AttIdx = rf_steps - 1;
    }
    return AttIdx;
}

extern "C" int EXTIO_API SetAttenuator(int atten_idx) {
    if (rf_steps <= 0) {
        return 1;
    }
    if (atten_idx < 0 || atten_idx >= rf_steps) {
        return 1;
    }

    if (openDev) {
        sddc_set_rf_gain(openDev, rf_gain_steps[atten_idx]);
    }

    if (port == PORT_VHF)
        giAttIdxVHF = atten_idx;
    else
        giAttIdxHF = atten_idx;

    if (!selectedSerial.empty()) {
        extioConfig.acquire();
        extioConfig.conf["devices"][selectedSerial]["rfGain"] = atten_idx;
        extioConfig.release(true);
    }

    EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_ATT);
    if (h_dialog) {
        SendMessage(h_dialog, WM_USER + 1, extHw_Changed_ATT, 0);
    }

    return 0;
}

extern "C" int EXTIO_API ExtIoGetAGCs(int agc_idx, char* text) {
    return 1;
}

extern "C" int EXTIO_API ExtIoGetActualAGCidx(void) {
    return -1;
}

extern "C" int EXTIO_API ExtIoSetAGC(int agc_idx) {
    return 1;
}

extern "C" int EXTIO_API ExtIoShowMGC(int agc_idx) {
    return 0;
}

extern "C" int EXTIO_API ExtIoGetMGCs(int mgc_idx, float* gain) {
    if (!if_gain_steps || if_steps <= 0) {
        return 1;
    }
    if (mgc_idx < if_steps) {
        if (gain) {
            *gain = if_gain_steps[mgc_idx];
        }
        return 0;
    }
    return 1;
}

extern "C" int EXTIO_API ExtIoGetActualMgcIdx(void) {
    int MgcIdx = (port == PORT_VHF) ? giMgcIdxVHF : giMgcIdxHF;
    if (if_steps > 0 && MgcIdx >= if_steps) {
        MgcIdx = if_steps - 1;
    }
    return MgcIdx;
}

extern "C" int EXTIO_API ExtIoSetMGC(int mgc_idx) {
    if (if_steps <= 0) {
        return 1;
    }
    if (mgc_idx < 0 || mgc_idx >= if_steps) {
        return 1;
    }

    if (openDev) {
        sddc_set_if_gain(openDev, if_gain_steps[mgc_idx]);
    }

    if (port == PORT_VHF)
        giMgcIdxVHF = mgc_idx;
    else
        giMgcIdxHF = mgc_idx;

    if (!selectedSerial.empty()) {
        extioConfig.acquire();
        extioConfig.conf["devices"][selectedSerial]["ifGain"] = mgc_idx;
        extioConfig.release(true);
    }

    EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_MGC);
    if (h_dialog) {
        SendMessage(h_dialog, WM_USER + 1, extHw_Changed_MGC, 0);
    }
    return 0;
}

extern "C" int EXTIO_API ExtIoGetSrates(int srate_idx, double* samplerate) {
    if (srate_idx < 0 || srate_idx >= (int)samplerates.size()) {
        return -1;
    }
    if (samplerate) {
        *samplerate = samplerates.value(srate_idx);
    }
    return 0;
}

extern "C" int EXTIO_API ExtIoSrateSelText(int srate_idx, char* text) {
    if (srate_idx < 0 || srate_idx >= (int)samplerates.size()) {
        return -1;
    }
    snprintf(text, 15, "%s", samplerates.name(srate_idx).c_str());
    return 0;
}

extern "C" int EXTIO_API ExtIoGetActualSrateIdx(void) {
    return (port == PORT_VHF) ? giExtSrateIdxVHF : giExtSrateIdxHF;
}

static int SetSrateInternal(int srate_idx, bool internal_call) {
    if (srate_idx < 0 || srate_idx >= (int)samplerates.size()) {
        return 1;
    }

    srId = srate_idx;
    sampleRate = samplerates.value(srId);

    if (port == PORT_VHF)
        giExtSrateIdxVHF = srId;
    else
        giExtSrateIdxHF = srId;

    if (!selectedSerial.empty()) {
        extioConfig.acquire();
        extioConfig.conf["devices"][selectedSerial]["samplerate"] = samplerates.key(srId);
        extioConfig.release(true);
    }

    if (openDev) {
        ddc.setOutSamplerate(sampleRate, sampleRate);
    }

    if (internal_call) {
        EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SampleRate);
    if (h_dialog) {
        SendMessage(h_dialog, WM_USER + 1, extHw_Changed_SampleRate, 0);
    }
    }
    return 0;
}

extern "C" int EXTIO_API ExtIoSetSrate(int srate_idx) {
    return SetSrateInternal(srate_idx, false);
}

extern "C" int SetOverclock(uint32_t adcfreq) {
    adcnominalfreq = adcfreq;
    xtal_freq = adcfreq;

    if (openDev) {
        sddc_set_xtal_freq(openDev, adcfreq);
        ddc.setInSamplerate(adcfreq);
    }

    EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_RF_IF);
    if (h_dialog) {
        SendMessage(h_dialog, WM_USER + 1, extHw_Changed_RF_IF, 0);
    }
    EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SampleRate);
    if (h_dialog) {
        SendMessage(h_dialog, WM_USER + 1, extHw_Changed_SampleRate, 0);
    }
    EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SRATES);
    if (h_dialog) {
        SendMessage(h_dialog, WM_USER + 1, extHw_Changed_SRATES, 0);
    }

    return 0;
}

extern "C" void EXTIO_API SetPPMvalue(double new_ppm_value) {
    gfFreqCorrectionPpm = new_ppm_value;
    SetOverclock(adcnominalfreq);
    UpdatePPM(h_dialog);
}

extern "C" double EXTIO_API GetPPMvalue(void) {
    return gfFreqCorrectionPpm;
}

extern "C" void EXTIO_API IFrateInfo(int rate) {
}

extern "C" int EXTIO_API ExtIoGetSetting(int idx, char* description, char* value) {
    switch (idx) {
    case 0:
        strcpy(description, "Identifier");
        snprintf(value, 1024, "%s", SETTINGS_IDENTIFIER);
        return 0;
    case 1:
        strcpy(description, "DeviceSerial");
        snprintf(value, 1024, "%s", selectedSerial.c_str());
        return 0;
    case 2:
        strcpy(description, "HF_SampleRateIdx");
        snprintf(value, 1024, "%d", giExtSrateIdxHF);
        return 0;
    case 3:
        strcpy(description, "VHF_SampleRateIdx");
        snprintf(value, 1024, "%d", giExtSrateIdxVHF);
        return 0;
    case 4:
        strcpy(description, "HF_AttenuationIdx");
        snprintf(value, 1024, "%d", giAttIdxHF);
        return 0;
    case 5:
        strcpy(description, "HF_VGAIdx");
        snprintf(value, 1024, "%d", giMgcIdxHF);
        return 0;
    case 6:
        strcpy(description, "VHF_AttenuationIdx");
        snprintf(value, 1024, "%d", giAttIdxVHF);
        return 0;
    case 7:
        strcpy(description, "VHF_VGAIdx");
        snprintf(value, 1024, "%d", giMgcIdxVHF);
        return 0;
    case 8:
        strcpy(description, "Bias");
        snprintf(value, 1024, "%d", bias ? 1 : 0);
        return 0;
    case 9:
        strcpy(description, "Dither");
        snprintf(value, 1024, "%d", dither ? 1 : 0);
        return 0;
    case 10:
        strcpy(description, "Rando");
        snprintf(value, 1024, "%d", rando ? 1 : 0);
        return 0;
    case 11:
        strcpy(description, "PGA");
        snprintf(value, 1024, "%d", pga ? 1 : 0);
        return 0;
    case 12:
        strcpy(description, "HighZ");
        snprintf(value, 1024, "%d", highz ? 1 : 0);
        return 0;
    case 13:
        strcpy(description, "LoFrequencyHz");
        snprintf(value, 1024, "%lf", gfLOfreq);
        return 0;
#if EXPORT_EXTIO_TUNE_FUNCTIONS
    case 14:
        strcpy(description, "TuneFrequencyHz");
        snprintf(value, 1024, "%lf", gfTunefreq);
        return 0;
#else
    case 14:
        strcpy(description, "TuneFrequencyHz");
        snprintf(value, 1024, "%lf", DEFAULT_TUNE_FREQ);
        return 0;
#endif
    case 15:
        strcpy(description, "Correction_ppm");
        snprintf(value, 1024, "%lf", gfFreqCorrectionPpm);
        return 0;
    case 16:
        strcpy(description, "ADC_nominal_freq");
        snprintf(value, 1024, "%d", adcnominalfreq);
        return 0;
    default:
        return -1;
    }
}

extern "C" void EXTIO_API ExtIoSetSetting(int idx, const char* value) {
    double tempDbl = 0.0;
    int tempInt = 0;
    uint32_t tempuint32 = 0;

    switch (idx) {
    case 0:
        if (value && strcmp(value, SETTINGS_IDENTIFIER) != 0) {
            return;
        }
        break;
    case 1:
        if (value) {
            refreshDevices();
            selectDevice(value);
            if (!selectedSerial.empty()) {
                extioConfig.acquire();
                extioConfig.conf["device"] = selectedSerial;
                extioConfig.release(true);
            }
        }
        break;
    case 2:
        tempInt = atoi(value);
        giExtSrateIdxHF = tempInt;
        if (port != PORT_VHF) {
            SetSrateInternal(giExtSrateIdxHF, true);
        }
        break;
    case 3:
        tempInt = atoi(value);
        giExtSrateIdxVHF = tempInt;
        if (port == PORT_VHF) {
            SetSrateInternal(giExtSrateIdxVHF, true);
        }
        break;
    case 4:
        tempInt = atoi(value);
        giAttIdxHF = tempInt;
        if (port != PORT_VHF) {
            SetAttenuator(giAttIdxHF);
        }
        break;
    case 5:
        tempInt = atoi(value);
        giMgcIdxHF = tempInt;
        if (port != PORT_VHF) {
            ExtIoSetMGC(giMgcIdxHF);
        }
        break;
    case 6:
        tempInt = atoi(value);
        giAttIdxVHF = tempInt;
        if (port == PORT_VHF) {
            SetAttenuator(giAttIdxVHF);
        }
        break;
    case 7:
        tempInt = atoi(value);
        giMgcIdxVHF = tempInt;
        if (port == PORT_VHF) {
            ExtIoSetMGC(giMgcIdxVHF);
        }
        break;
    case 8:
        bias = (atoi(value) != 0);
        if (!selectedSerial.empty()) {
            extioConfig.acquire();
            extioConfig.conf["devices"][selectedSerial]["bias"] = bias;
            extioConfig.release(true);
        }
        break;
    case 9:
        dither = (atoi(value) != 0);
        if (!selectedSerial.empty()) {
            extioConfig.acquire();
            extioConfig.conf["devices"][selectedSerial]["dither"] = dither;
            extioConfig.release(true);
        }
        break;
    case 10:
        rando = (atoi(value) != 0);
        if (!selectedSerial.empty()) {
            extioConfig.acquire();
            extioConfig.conf["devices"][selectedSerial]["rando"] = rando;
            extioConfig.release(true);
        }
        break;
    case 11:
        pga = (atoi(value) != 0);
        if (!selectedSerial.empty()) {
            extioConfig.acquire();
            extioConfig.conf["devices"][selectedSerial]["pga"] = pga;
            extioConfig.release(true);
        }
        break;
    case 12:
        highz = (atoi(value) != 0);
        if (!selectedSerial.empty()) {
            extioConfig.acquire();
            extioConfig.conf["devices"][selectedSerial]["highz"] = highz;
            extioConfig.release(true);
        }
        break;
    case 13:
        gfLOfreq = 2000000.0;
        if (sscanf(value, "%lf", &tempDbl) > 0) {
            gfLOfreq = tempDbl;
        }
        break;
    case 14:
#if EXPORT_EXTIO_TUNE_FUNCTIONS
        gfTunefreq = DEFAULT_TUNE_FREQ;
        if (sscanf(value, "%lf", &tempDbl) > 0) {
            gfTunefreq = tempDbl;
        }
#endif
        break;
    case 15:
        gfFreqCorrectionPpm = 0.0;
        if (sscanf(value, "%lf", &tempDbl) > 0) {
            gfFreqCorrectionPpm = tempDbl;
        }
        break;
    case 16:
        adcnominalfreq = DEFAULT_ADC_FREQ;
        if (sscanf(value, "%d", &tempuint32) > 0) {
            adcnominalfreq = tempuint32;
        }
        break;
    case 21:
        dither = (atoi(value) != 0);
        if (openDev) {
            sddc_enable_adc_dither(openDev, dither ? 1 : 0);
        }
        if (!selectedSerial.empty()) {
            extioConfig.acquire();
            extioConfig.conf["devices"][selectedSerial]["dither"] = dither;
            extioConfig.release(true);
        }
        break;
    case 22:
        pga = (atoi(value) != 0);
        if (openDev) {
            sddc_enable_adc_pga(openDev, pga ? 1 : 0);
        }
        if (!selectedSerial.empty()) {
            extioConfig.acquire();
            extioConfig.conf["devices"][selectedSerial]["pga"] = pga;
            extioConfig.release(true);
        }
        break;
    case 23:
        rando = (atoi(value) != 0);
        if (openDev) {
            sddc_enable_adc_rando(openDev, rando ? 1 : 0);
        }
        if (!selectedSerial.empty()) {
            extioConfig.acquire();
            extioConfig.conf["devices"][selectedSerial]["rando"] = rando;
            extioConfig.release(true);
        }
        break;
    case 24:
        bias = (atoi(value) != 0);
        if (openDev) {
            int flag = port == PORT_HF ? 1 : 2;
            sddc_enable_bias_tee(openDev, bias ? flag : 0);
        }
        if (!selectedSerial.empty()) {
            extioConfig.acquire();
            extioConfig.conf["devices"][selectedSerial]["bias"] = bias;
            extioConfig.release(true);
        }
        break;
    case 25:
        bias = (atoi(value) != 0);
        if (openDev) {
            int flag = port == PORT_HF ? 1 : 2;
            sddc_enable_bias_tee(openDev, bias ? flag : 0);
        }
        if (!selectedSerial.empty()) {
            extioConfig.acquire();
            extioConfig.conf["devices"][selectedSerial]["bias"] = bias;
            extioConfig.release(true);
        }
        break;
    default:
        break;
    }
}

static void SaveSettings() {
    HKEY handle;
    DWORD diposition;
    RegCreateKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\SDRPP_EXTIO", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &handle, &diposition);

    int idx = 0;
    char desc[256];
    char value[1024];
    int ret = ExtIoGetSetting(idx, desc, value);
    while (ret == 0) {
        char idxvalue[30];
        sprintf(idxvalue, "%03d_Value", idx);
        RegSetValueEx(handle, idxvalue, 0, REG_SZ, (const BYTE*)value, (DWORD)strlen(value));

        sprintf(idxvalue, "%03d_Description", idx);
        RegSetValueEx(handle, idxvalue, 0, REG_SZ, (const BYTE*)desc, (DWORD)strlen(desc));

        idx++;
        ret = ExtIoGetSetting(idx, desc, value);
    }

    CloseHandle(handle);
    saveConfig();
}

static void LoadSettings() {
    HKEY handle;
    DWORD diposition;
    RegCreateKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\SDRPP_EXTIO", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &handle, &diposition);

    int idx = 0;
    char value[1024];
    char idxvalue[30];
    DWORD len = 1023;
    DWORD type;
    sprintf(idxvalue, "%03d_Value", idx);
    LSTATUS status = RegQueryValueEx(handle, idxvalue, 0, &type, (BYTE*)value, &len);

    while (status == ERROR_SUCCESS && type == REG_SZ) {
        ExtIoSetSetting(idx, value);

        idx++;
        sprintf(idxvalue, "%03d_Value", idx);
        len = 1023;
        status = RegQueryValueEx(handle, idxvalue, 0, &type, (BYTE*)value, &len);
    }

    CloseHandle(handle);
}

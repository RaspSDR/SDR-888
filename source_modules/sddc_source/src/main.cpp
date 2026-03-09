#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/smgui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <utils/optionlist.h>
#include <atomic>
#include <assert.h>
#include "libsddc.h"

//#define FASTVFO_ENABLED

#if !defined(FASTVFO_ENABLED)
#include "fft_rx_vfo.h"
typedef dsp::channel::FFTRxVFO RxVFOType;
#else
#include "pf_rx_vfo.h"
typedef dsp::channel::PFRxVFO RxVFOType;
#endif

SDRPP_MOD_INFO{
    /* Name:            */ "sddc_source",
    /* Description:     */ "SDDC Source Module",
    /* Author:          */ "Howard Su",
    /* Version:         */ 1, 0, 0,
    /* Max instances    */ -1
};

ConfigManager config;

#define CONCAT(a, b) ((std::string(a) + b).c_str())

// The following count have to be choose as
// 3 * N = 4 * BufferCount + 1
// where N is integer, so that the FFT bins align correctly
// the values we can use 41, 80, 92, 101
#define SDDC_ACCUMRATE_BUFFER_COUNT 41
#define SDDC_BUFFER_SIZE            (16 * 1024 / 2)

#define TUNER_IF_FREQUENCY 4570000.0

enum Model {
    MODEL_RX888,
    MODEL_RX888mk2,
    MODEL_RX888PRO,
    MODEL_COUNT
};

enum ExtGPIOMode {
    EXT_GPIO_CUSTOM = 0,
    EXT_GPIO_ALEX = 1,
};

// GAINFACTORS to be adjusted with lab reference source measured with HDSDR Smeter rms mode
const float GainFactor[MODEL_COUNT] = {
    2.28e-4f,
    3.54e-4f,
    2.28e-4f,
};

const uint32_t RefClockFreq[MODEL_COUNT] = {
    27000000,
    27000000,
    24576000
};

class SDDCSourceModule : public ModuleManager::Instance {
public:
    SDDCSourceModule(std::string name) {
        this->name = name;

        sampleRate = 128 * 1000 * 1000.0;

        // Initialize the DDC
        ddc.init(&dataIn, 0.1f);

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &ddc.out;

        // Refresh devices
        refresh();

        // Select device from config
        config.acquire();
        std::string devSerial = config.conf["device"];
        config.release();
        select(devSerial);

        dataIn.setBufferSize(SDDC_BUFFER_SIZE * SDDC_ACCUMRATE_BUFFER_COUNT);
        ddc.setBufferSize(SDDC_BUFFER_SIZE * SDDC_ACCUMRATE_BUFFER_COUNT);

        sigpath::sourceManager.registerSource("RX-888", &handler);
    }

    ~SDDCSourceModule() {
        sigpath::sourceManager.unregisterSource("RX-888");
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

    enum Port {
        PORT_VHF,
        PORT_HF,
        PORT_FM,
        PORT_BYPASS
    };

private:
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

    void refresh() {
        devices.clear();

        int devCount = sddc_get_device_count();

        // If no device, give up
        if (!devCount) { return; }

        for (int i = 0; i < devCount; ++i) {
            char serial[256];
            int err = sddc_get_device_usb_strings(i, NULL, NULL, serial);
            if (err == 0) {
                devices.define(std::string(serial), std::string(serial), i);
            }
        }
    }

    void select(const std::string& serial) {

        // If there are no devices, give up
        if (devices.empty()) {
            selectedSerial.clear();
            return;
        }

        int index = sddc_get_index_by_serial(serial.c_str());

        // if the serial was not found, select the first device
        if (index < 0) {
            select(devices.key(0));
            return;
        }

        // Get the ID in the list
        int id = devices.keyId(serial);
        selectedDevId = devices[id];

        // some API needs the device specific information
        sddc_dev_t* dev;
        int err = sddc_open(&dev, selectedDevId);
        if (err) {
            flog::error("Failed to open device: {}", err);
            return;
        }

        if (id != original_index) {
            original_index = id;
            flog::info("SDDCSourceModule '{0}': Selected device index changed to {1}", name, id);

            // determine the device type
            char product[256];
            sddc_get_usb_strings(dev, NULL, product, NULL);

            model = MODEL_RX888;
            if (strstr(product, "RX888mk2")) {
                model = MODEL_RX888mk2;
            }
            else if (strstr(product, "RX888pro")) {
                model = MODEL_RX888PRO;
                has_preamp = true;
                has_ext_gpio = true;
            }
            else if (strstr(product, "RX888")) {
                model = MODEL_RX888;
            }

            // Define the ports
            ports.clear();
            ports.define("hf", "HF", PORT_HF);
            ports.define("vhf", "VHF", PORT_VHF);
            if (model == MODEL_RX888PRO) {
                ports.define("fm", "FM", PORT_FM);
                ports.define("bypass", "Bypass", PORT_BYPASS);
            }

            // Save serial number
            selectedSerial = serial;

            float gainFactor = GainFactor[model];
            ddc.setGainFactor(gainFactor);
        }

        uint32_t ref_clock_freq = RefClockFreq[model];

        if (ext_clock) {
            clock_freq = ext_clock_freq;
            sddc_enable_ext_clock(dev, 1);
        }
        else {
            clock_freq = ref_clock_freq;
            ext_clock_freq = clock_freq;
            sddc_enable_ext_clock(dev, 0);
        }

        if (clock_freq < 10000000) {
            clock_freq = ref_clock_freq;
        }

        // Load default options
        port = PORT_HF;
        portId = ports.valueId(port);
        rfGainIdx = 0;
        ifGainIdx = 0;
        bias = false;
        highz = false;
        preamp = false;

        // Load config
        config.acquire();
        if (config.conf["devices"][selectedSerial].contains("port")) {
            std::string desiredPort = config.conf["devices"][selectedSerial]["port"];
            if (ports.keyExists(desiredPort)) {
                portId = ports.keyId(desiredPort);
                port = ports[portId];
            }
        }

        xtalrates.clear();
        if (ref_clock_freq == 27000000) {
            // MK1 & MK2
            uint32_t xtal_freq0 = 122880000;
            xtalrates.define(xtal_freq0, getBandwdithScaled(xtal_freq0), xtal_freq0);
            xtalrates.define(xtal_freq0 / 2, getBandwdithScaled(xtal_freq0 / 2), xtal_freq0 / 2);
        }
        else {
            // PRO
            int c = clock_freq * 80 / 16;
            xtalrates.define(c, getBandwdithScaled(c), c);
            xtalrates.define(c / 2, getBandwdithScaled(c / 2), c / 2);
            xtalrates.define(clock_freq * 3, getBandwdithScaled(clock_freq * 3), clock_freq * 3);
        }

        if (config.conf["devices"][selectedSerial].contains("xtal_freq")) {
            xtal_freq = config.conf["devices"][selectedSerial]["xtal_freq"];
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
            uint32_t sampleRate0;
            // define supported samplerates
            samplerates.clear();
            sampleRate0 = xtal_freq / 2;
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
            uint32_t sampleRate0;
            // define supported samplerates
            samplerates.clear();
            if (xtal_freq / 8 > 8e8) {
                sampleRate0 = xtal_freq / 16;
            }
            else {
                sampleRate0 = xtal_freq / 8;
            }
            for (int i = 1; i <= 4; ++i) {
                samplerates.define(sampleRate0, getBandwdithScaled(sampleRate0), sampleRate0);
                sampleRate0 /= 2;
            }

            if (!samplerates.valueExists(sampleRate)) {
                sampleRate = samplerates.key(1);
            }
            srId = samplerates.valueId(sampleRate);
        }

        if (config.conf["devices"][selectedSerial].contains("samplerate")) {
            int desiredSr = config.conf["devices"][selectedSerial]["samplerate"];
            if (samplerates.keyExists(desiredSr)) {
                srId = samplerates.keyId(desiredSr);
                sampleRate = samplerates[srId];
            }
        }

        rf_steps = sddc_get_rf_gain_steps(dev, &rf_gain_steps);
        if (config.conf["devices"][selectedSerial].contains("rfGain")) {
            rfGainIdx = std::clamp<int>(config.conf["devices"][selectedSerial]["rfGain"], 0, rf_steps - 1);
        }

        if_steps = sddc_get_if_gain_steps(dev, &if_gain_steps);
        if (config.conf["devices"][selectedSerial].contains("ifGain")) {
            ifGainIdx = std::clamp<int>(config.conf["devices"][selectedSerial]["ifGain"], 0, if_steps - 1);
        }
        if (config.conf["devices"][selectedSerial].contains("bias")) {
            bias = config.conf["devices"][selectedSerial]["bias"];
        }
        if (config.conf["devices"][selectedSerial].contains("pga")) {
            pga = config.conf["devices"][selectedSerial]["pga"];
        }
        if (config.conf["devices"][selectedSerial].contains("highz")) {
            highz = config.conf["devices"][selectedSerial]["highz"];
        }
        if (config.conf["devices"][selectedSerial].contains("rando")) {
            rando = config.conf["devices"][selectedSerial]["rando"];
        }
        if (config.conf["devices"][selectedSerial].contains("dither")) {
            dither = config.conf["devices"][selectedSerial]["dither"];
        }
        if (has_preamp && config.conf["devices"][selectedSerial].contains("preamp")) {
            preamp = config.conf["devices"][selectedSerial]["preamp"];
        }
        if (has_ext_gpio) {
            if (config.conf["devices"][selectedSerial].contains("ext_gpio_mode")) {
                gpio_mode = (ExtGPIOMode)config.conf["devices"][selectedSerial]["ext_gpio_mode"];
            }
            if (config.conf["devices"][selectedSerial].contains("ext_gpio_bits")) {
                gpio_bits = config.conf["devices"][selectedSerial]["ext_gpio_bits"];
            }
        }
        config.release();

        sddc_close(dev);

        // Update the samplerate
        core::setInputSampleRate(sampleRate);

        // Update freq select limits
        // if (port == PORT_HF) {
        //     gui::freqSelect.minFreq = 0;
        //     gui::freqSelect.maxFreq = sampleRate <= 32e6 ? 32e6 : 64e6;
        // }
        // else {
        //     gui::freqSelect.minFreq = 30e6;
        //     gui::freqSelect.maxFreq = 5e9;
        // }
        // gui::freqSelect.limitFreq = true;
    }

    static void menuSelected(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::debug("SDDCSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        flog::debug("SDDCSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        if (_this->running) { return; }

        _this->start();
    }

    void start() {
        // Open the device
        int err = sddc_open(&openDev, selectedDevId);
        if (err) {
            flog::error("Failed to open device: {}", (int)err);
            return;
        }

        sddc_set_xtal_freq(openDev, xtal_freq);

        // set HF or VHF first
        if (port != PORT_VHF) {
            sddc_set_direct_sampling(openDev, 1);
            sddc_enable_bias_tee(openDev, bias ? 1 : 0);

            if (model == MODEL_RX888PRO) {
                sddc_enable_hf_highz(openDev, highz ? 1 : 0);

                switch (port) {
                case PORT_HF:
                    if (xtal_freq > 64e8)
                        sddc_set_adc_filter(openDev, Freq64MHz);
                    else
                        sddc_set_adc_filter(openDev, Freq32MHz);
                    break;
                case PORT_FM:
                    sddc_set_adc_filter(openDev, FMUndersample);
                    break;
                case PORT_BYPASS:
                    sddc_set_adc_filter(openDev, Bypass);
                    break;
                case PORT_VHF:
                    break;
                }
            }

            // Configure and start the DDC for decimation only
            ddc.setInSamplerate(xtal_freq);
            ddc.setOutSamplerate(sampleRate, sampleRate);

            if (port == PORT_HF) {
                ddc.setOffset(-TUNER_IF_FREQUENCY);
            }
            else {
                ddc.setOffset(freq);
            }
            ddc.start();
        }
        else {
            sddc_set_direct_sampling(openDev, 0);
            sddc_enable_bias_tee(openDev, bias ? 0x02 : 0);

            sddc_set_center_freq64(openDev, (uint64_t)freq);

            // Configure and start the DDC for decimation only
            ddc.setInSamplerate(xtal_freq);
            ddc.setOutSamplerate(sampleRate, sampleRate);
            ddc.setOffset(-TUNER_IF_FREQUENCY);
            ddc.start();
        }

        sddc_enable_adc_pga(openDev, pga ? 1 : 0);
        sddc_set_xtal_freq(openDev, xtal_freq);

        rf_steps = sddc_get_rf_gain_steps(openDev, &rf_gain_steps);
        rfGainIdx = std::clamp<int>(rfGainIdx, 0, rf_steps - 1);
        if_steps = sddc_get_if_gain_steps(openDev, &if_gain_steps);
        ifGainIdx = std::clamp<int>(ifGainIdx, 0, if_steps - 1);

        if (rf_steps > 0)
            sddc_set_rf_gain(openDev, rf_gain_steps[rfGainIdx]);
        if (if_steps > 0)
            sddc_set_if_gain(openDev, if_gain_steps[ifGainIdx]);

        if (has_preamp)
            sddc_enable_preamp(openDev, preamp ? 1 : 0);

        buffercount = 0;

        running = true;
        sddc_read_async(openDev, &sddc_async_callback, this);

        flog::info("SDDCSourceModule '{0}': Start!", name);
    }

    static void stop(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        if (!_this->running) { return; }

        _this->stop();
    }

    void stop() {
        running = false;

        dataIn.stopWriter();
        sddc_cancel_async(openDev);
        dataIn.clearWriteStop();

        // Stop the DDC
        ddc.stop();

        // Close the device
        sddc_close(openDev);

        flog::info("SDDCSourceModule '{0}': Stop!", name);
    }

    static void tune(double freq, void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        if (_this->running) {
            switch (_this->port) {
            case PORT_VHF:
                sddc_set_center_freq64(_this->openDev, (uint64_t)freq);
                break;
            case PORT_FM:
                freq -= _this->xtal_freq;
                if (freq > 0 && freq < _this->xtal_freq / 2) {
                    _this->ddc.setOffset(freq);
                }
                break;
            default:
                if (freq < _this->xtal_freq / 2) {
                    _this->ddc.setOffset(freq);
                }
                break;
            }
        }
        _this->freq = freq;
        flog::debug("SDDCSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        ImGui::PushID(CONCAT("sddc_menu_", _this->name));

        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::LeftLabel(_L("Device"));
        SmGui::ForceSync();
        if (SmGui::Combo("##_sddc_dev_sel", &_this->selectedDevId, _this->devices.txt)) {
            _this->select(_this->devices.key(_this->selectedDevId));
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["device"] = _this->selectedSerial;
            config.release(true);
        }
        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(_L("Refresh"))) {
            _this->refresh();
            _this->select(_this->selectedSerial);
            core::setInputSampleRate(_this->sampleRate);
        }

        if (SmGui::Checkbox(_L("External Clock"), &_this->ext_clock)) {
            _this->select(_this->selectedSerial);
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["ext_clock"] = _this->ext_clock;
                config.release(true);
            }
        }

        SmGui::SameLine();
        if (_this->ext_clock) {
            if (SmGui::InputInt("##_sddc_ext_clk_freq", (int*)&_this->ext_clock_freq, 1000, 1000000)) {
                _this->select(_this->selectedSerial);
                if (!_this->selectedSerial.empty()) {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerial]["ext_clock_freq"] = _this->ext_clock_freq;
                    config.release(true);
                }
            }
        }
        else {
            SmGui::Text(getBandwdithScaled(_this->clock_freq).c_str());
        }

        SmGui::LeftLabel(_L("Sample Rate"));
        SmGui::FillWidth();
        if (SmGui::Combo("##_sddc_xtal_sel", &_this->xtalId, _this->xtalrates.txt)) {
            _this->xtal_freq = _this->xtalrates.value(_this->xtalId);
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["xtal_freq"] = _this->xtalrates.key(_this->xtalId);
                config.release(true);
            }
            _this->select(_this->devices.key(_this->selectedDevId));
            core::setInputSampleRate(_this->sampleRate);
        }

        SmGui::LeftLabel(_L("Bandwidth"));
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo("##_sddc_sr_sel", &_this->srId, _this->samplerates.txt)) {
            _this->sampleRate = _this->samplerates.value(_this->srId);
            core::setInputSampleRate(_this->sampleRate);
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["samplerate"] = _this->samplerates.key(_this->srId);
                config.release(true);
            }
        }

        SmGui::LeftLabel(_L("Mode"));
        if (SmGui::Combo("##_sddc_port", &_this->portId, _this->ports.txt)) {
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["port"] = _this->ports.key(_this->portId);
                config.release(true);

                _this->select(_this->selectedSerial);
            }
        }

        SmGui::LeftLabel(_L("ADC"));
        if (SmGui::Checkbox(_L("RANDO"), &_this->rando)) {
            if (_this->running) {
                sddc_enable_adc_rando(_this->openDev, _this->rando ? 1 : 0);
            }
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["rando"] = _this->rando;
                config.release(true);
            }
        }
        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::SameLine();
        if (SmGui::Checkbox(_L("PGA"), &_this->pga)) {
            if (_this->running) {
                sddc_enable_adc_pga(_this->openDev, _this->pga ? 1 : 0);
            }
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["pga"] = _this->pga;
                config.release(true);
            }
        }

        SmGui::SameLine();
        if (SmGui::Checkbox(_L("Dither"), &_this->dither)) {
            if (_this->running) {
                sddc_enable_adc_dither(_this->openDev, _this->dither ? 1 : 0);
            }
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["dither"] = _this->dither;
                config.release(true);
            }
        }

        if (_this->rf_steps > 1) {
            SmGui::LeftLabel(_L("RF Gain"));
            SmGui::FillWidth();
            char label[32];
            snprintf(label, sizeof(label), "%.1f dB", _this->rf_gain_steps[_this->rfGainIdx]);

            if (ImGui::SliderInt("##_sddc_rf_gain",
                                 &_this->rfGainIdx, 0, _this->rf_steps - 1, label)) {
                if (_this->running) {
                    sddc_set_rf_gain(_this->openDev, _this->rf_gain_steps[_this->rfGainIdx]);
                }
                if (!_this->selectedSerial.empty()) {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerial]["rfGain"] = _this->rfGainIdx;
                    config.release(true);
                }
            }
        }

        if (_this->if_steps > 1) {
            SmGui::LeftLabel(_L("IF Gain"));
            SmGui::FillWidth();
            char label[32];
            snprintf(label, sizeof(label), "%.1f dB", _this->if_gain_steps[_this->ifGainIdx]);

            if (ImGui::SliderInt("##_sddc_if_gain",
                                 &_this->ifGainIdx, 0, _this->if_steps - 1, label)) {
                if (_this->running) {
                    sddc_set_if_gain(_this->openDev, _this->if_gain_steps[_this->ifGainIdx]);
                }
                if (!_this->selectedSerial.empty()) {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerial]["ifGain"] = _this->ifGainIdx;
                    config.release(true);
                }
            }
        }

        if (_this->has_preamp && SmGui::Checkbox(_L("Preamp"), &_this->preamp)) {
            if (_this->running) {
                sddc_enable_preamp(_this->openDev, _this->preamp ? 1 : 0);
            }
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["preamp"] = _this->preamp;
                config.release(true);
            }
        }

        if (SmGui::Checkbox(_L("Bias-T"), &_this->bias)) {
            if (_this->running) {
                int flag = _this->port == PORT_HF ? 1 : 2;
                sddc_enable_bias_tee(_this->openDev, _this->bias ? flag : 0);
            }
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["bias"] = _this->bias;
                config.release(true);
            }
        }

        if (_this->port != PORT_VHF && _this->model == MODEL_RX888PRO) {
            if (SmGui::Checkbox(_L("High-Z Antenna"), &_this->highz)) {
                if (_this->running) {
                    sddc_enable_hf_highz(_this->openDev, _this->highz ? 1 : 0);
                }
                if (!_this->selectedSerial.empty()) {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerial]["highz"] = _this->highz;
                    config.release(true);
                }
            }
        }

        if (_this->has_ext_gpio) {
            SmGui::LeftLabel(_L("Ext GPIO Mode"));
            if (SmGui::Combo("##_sddc_ext_gpio_mode", (int*)&_this->gpio_mode, "Custom\0Alex\0")) {
                if (!_this->selectedSerial.empty()) {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerial]["ext_gpio_mode"] = _this->gpio_mode;
                    config.release(true);
                }
            }

            if (_this->gpio_mode == EXT_GPIO_CUSTOM) {
                SmGui::LeftLabel(_L("Bits"));
                for (int i = 0; i < 7; ++i) {
                    char bitLabel[32];
                    bool enabled = (_this->gpio_bits & (1 << i)) != 0;
                    snprintf(bitLabel, sizeof(bitLabel), "%d", i);
                    SmGui::SameLine();
                    if (SmGui::Checkbox(bitLabel, &enabled)) {
                        if (enabled)
                            _this->gpio_bits |= (1 << i);
                        else
                            _this->gpio_bits &= ~(1 << i);
                        sddc_set_ext_io_port_state(_this->openDev, _this->gpio_bits);
                        if (!_this->selectedSerial.empty()) {
                            config.acquire();
                            config.conf["devices"][_this->selectedSerial]["ext_gpio_bits"] = _this->gpio_bits;
                            config.release(true);
                        }
                    }
                }
            }

            // display gpio bits for debug, gpio_bits is 8bits, only lower 7 bits are used
            char gpioStatus[32];
            snprintf(gpioStatus, sizeof(gpioStatus), "GPIO Bits: 0x%02X", _this->gpio_bits);
            SmGui::Text(gpioStatus);
        }

        ImGui::PopID();
    }

    static void sddc_async_callback(const int16_t* buffer, uint32_t count, void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;

        assert(SDDC_BUFFER_SIZE == count);

        memcpy(_this->dataIn.writeBuf + _this->buffercount * SDDC_BUFFER_SIZE, buffer, count * sizeof(int16_t));

        _this->buffercount++;
        // If buffer is full, swap and reset fill
        if (_this->buffercount == SDDC_ACCUMRATE_BUFFER_COUNT) {
            _this->dataIn.swap(SDDC_BUFFER_SIZE * SDDC_ACCUMRATE_BUFFER_COUNT);
            _this->buffercount = 0;
        }
    }

    std::string name;
    bool enabled = true;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;

    OptionList<std::string, int> devices;
    int selectedDevId = 0;

    // ref clock
    bool ext_clock = false;
    uint32_t ext_clock_freq;
    uint32_t clock_freq;

    uint32_t xtal_freq;
    OptionList<int, uint32_t> xtalrates;
    int xtalId = 0;

    OptionList<int, double> samplerates;
    int srId = 0;
    double sampleRate;

    OptionList<std::string, Port> ports;
    int portId = 0;
    Port port;

    std::string selectedSerial;

    int rfGainIdx;
    int rf_steps;
    const float* rf_gain_steps;

    int ifGainIdx;
    int if_steps;
    const float* if_gain_steps;

    bool pga;
    bool dither;
    bool rando;

    sddc_dev_t* openDev;

    int buffercount;
    std::thread workerThread;
    std::atomic<bool> run = false;

    bool bias;
    bool highz;
    bool anti_alias = true;

    bool has_preamp = false;
    bool preamp = false;

    bool has_ext_gpio = false;
    enum ExtGPIOMode gpio_mode;
    uint8_t gpio_bits;

    int original_index = -1;
    int model = MODEL_RX888;

    RxVFOType ddc;
    dsp::stream<int16_t> dataIn;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/sddc_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new SDDCSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (SDDCSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
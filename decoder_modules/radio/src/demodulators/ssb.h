#pragma once
#include "../demod.h"
#include <dsp/demod/ssb.h>

namespace demod {
    class SSB : public Demodulator {
    public:
        enum class Mode {
            USB,
            LSB,
            DSB
        };

        SSB() {}

        ~SSB() {
            stop();
        }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            this->name = name;
            _config = config;

            // Load config
            config->acquire();
            if (config->conf[name][getName()].contains("mode")) {
                std::string modeStr = config->conf[name][getName()]["mode"];
                if (modeStr == "LSB") { mode = Mode::LSB; }
                else if (modeStr == "DSB") { mode = Mode::DSB; }
                else { mode = Mode::USB; }
            }
            if (config->conf[name][getName()].contains("agcAttack")) {
                agcAttack = config->conf[name][getName()]["agcAttack"];
            }
            if (config->conf[name][getName()].contains("agcDecay")) {
                agcDecay = config->conf[name][getName()]["agcDecay"];
            }
            if (config->conf[name][getName()].contains("agcEnable")) {
                agcEnable = config->conf[name][getName()]["agcEnable"];
            }
            if (config->conf[name][getName()].contains("gain")) {
                gain = config->conf[name][getName()]["gain"];
            }
            config->release();

            // Define structure
            ssbDemod.init(input, getDspMode(), agcEnable, gain, bandwidth, getIFSampleRate(), agcAttack / getIFSampleRate(), agcDecay / getIFSampleRate());
        }

        void start() {
            ssbDemod.start();
        }

        void stop() {
            ssbDemod.stop();
        }

        void showMenu() {
            float menuWidth = ImGui::GetContentRegionAvail().x;

            bool usbMode = (mode == Mode::USB);
            bool lsbMode = (mode == Mode::LSB);
            bool dsbMode = (mode == Mode::DSB);
            bool modeChanged = false;

            if (ImGui::Checkbox("USB##_radio_ssb_mode_usb", &usbMode) && usbMode) {
                mode = Mode::USB;
                modeChanged = true;
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("LSB##_radio_ssb_mode_lsb", &lsbMode) && lsbMode) {
                mode = Mode::LSB;
                modeChanged = true;
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("DSB##_radio_ssb_mode_dsb", &dsbMode) && dsbMode) {
                mode = Mode::DSB;
                modeChanged = true;
            }

            if (modeChanged) {
                const char* modeStr = (mode == Mode::LSB) ? "LSB" : (mode == Mode::DSB) ? "DSB" : "USB";
                _config->acquire();
                _config->conf[name][getName()]["mode"] = modeStr;
                _config->release(true);
                reinitRequested = true;
            }

            ImGui::PushID("ssb_agc_enable");
            if (ImGui::Checkbox(_L("Enable AGC"), &agcEnable)) {
                setAGCEnableInternal(agcEnable);
                if (!agcEnable) {
                    setGainInternal(gain);
                }
                _config->acquire();
                _config->conf[name][getName()]["agcEnable"] = agcEnable;
                _config->release(true);
            }
            ImGui::PopID();

            if (agcEnable) {
                ImGui::LeftLabel(_L("AGC Attack"));
                ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
                if (ImGui::SliderFloat(("##_radio_ssb_agc_attack_" + name).c_str(), &agcAttack, 1.0f, 200.0f)) {
                    setAGCAttackInternal(agcAttack / getIFSampleRate());
                    _config->acquire();
                    _config->conf[name][getName()]["agcAttack"] = agcAttack;
                    _config->release(true);
                }
                ImGui::LeftLabel(_L("AGC Decay"));
                ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
                if (ImGui::SliderFloat(("##_radio_ssb_agc_decay_" + name).c_str(), &agcDecay, 1.0f, 20.0f)) {
                    setAGCDecayInternal(agcDecay / getIFSampleRate());
                    _config->acquire();
                    _config->conf[name][getName()]["agcDecay"] = agcDecay;
                    _config->release(true);
                }
            }
            else {
                ImGui::LeftLabel(_L("Gain"));
                ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
                if (ImGui::SliderFloat(("##_radio_ssb_gain_" + name).c_str(), &gain, 0.0f, 100.0f, "%.1f dB")) {
                    setGainInternal(gain);
                    _config->acquire();
                    _config->conf[name][getName()]["gain"] = gain;
                    _config->release(true);
                }
            }
        }

        void setBandwidth(double bandwidth) {
            ssbDemod.setBandwidth(bandwidth);
        }

        void setInput(dsp::stream<dsp::complex_t>* input) {
            ssbDemod.setInput(input);
        }

        void AFSampRateChanged(double newSR) {}

        // ============= INFO =============

        const char* getName() { return getConfigName(); }
        double getIFSampleRate() { return 24000.0; }
        double getAFSampleRate() { return getIFSampleRate(); }
        double getDefaultBandwidth() { return (mode == Mode::DSB) ? 4600.0 : 2800.0; }
        double getMinBandwidth() { return (mode == Mode::DSB) ? 1000.0 : 500.0; }
        double getMaxBandwidth() { return getIFSampleRate() / 2.0; }
        bool getBandwidthLocked() { return false; }
        double getDefaultSnapInterval() { return 100.0; }
        int getVFOReference() {
            if (mode == Mode::USB) { return ImGui::WaterfallVFO::REF_LOWER; }
            if (mode == Mode::LSB) { return ImGui::WaterfallVFO::REF_UPPER; }
            return ImGui::WaterfallVFO::REF_CENTER;
        }
        bool getDeempAllowed() { return false; }
        bool getPostProcEnabled() { return true; }
        int getDefaultDeemphasisMode() { return DEEMP_MODE_NONE; }
        bool getFMIFNRAllowed() { return false; }
        bool getNBAllowed() { return true; }
        bool getANRAllowed() { return mode != Mode::DSB; }
        bool getANFAllowed() { return mode != Mode::DSB; }
        bool takeReinitRequest() {
            bool ret = reinitRequested;
            reinitRequested = false;
            return ret;
        }
        dsp::stream<dsp::stereo_t>* getOutput() {
            return &ssbDemod.out;
        }

    private:
        void setAGCEnableInternal(bool enable) {
            ssbDemod.setAGCEnable(enable);
        }

        void setAGCAttackInternal(double attack) {
            ssbDemod.setAGCAttack(attack);
        }

        void setAGCDecayInternal(double decay) {
            ssbDemod.setAGCDecay(decay);
        }

        void setGainInternal(double gainDb) {
            ssbDemod.setGainDb(gainDb);
        }

        dsp::demod::SSB<dsp::stereo_t>::Mode getDspMode() {
            if (mode == Mode::USB) { return dsp::demod::SSB<dsp::stereo_t>::Mode::USB; }
            if (mode == Mode::LSB) { return dsp::demod::SSB<dsp::stereo_t>::Mode::LSB; }
            return dsp::demod::SSB<dsp::stereo_t>::Mode::DSB;
        }

        const char* getConfigName() { return "SSB"; }

        Mode mode = Mode::USB;
        dsp::demod::SSB<dsp::stereo_t> ssbDemod;

        ConfigManager* _config;

        bool reinitRequested = false;
        bool agcEnable = true;
        float agcAttack = 50.0f;
        float agcDecay = 5.0f;
        float gain = 0.0f;

        std::string name;
    };
}
#pragma once
#include "../demod.h"
#include <dsp/demod/am.h>
#include <dsp/demod/sam.h>

namespace demod {
    class AM : public Demodulator {
    public:
        enum Mode {
            AM_MODE,
            SAM_MODE,
            ECSS_MODE,
        };

        enum ECSSSidebandMode {
            AUTO_MODE,
            LSB_MODE,
            USB_MODE,
        };

        AM() {}

        AM(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            init(name, config, input, bandwidth, audioSR);
        }

        ~AM() { stop(); }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            this->name = name;
            _config = config;

            // Load config
            config->acquire();
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
            if (config->conf[name][getName()].contains("mode")) {
                std::string modeStr = config->conf[name][getName()]["mode"];
                if (modeStr == "SAM") {
                    mode = SAM_MODE;
                }
                else if (modeStr == "ECSS") {
                    mode = ECSS_MODE;
                }
                else {
                    mode = AM_MODE;
                }
            }
            else if (config->conf[name][getName()].contains("syncDemod")) {
                mode = config->conf[name][getName()]["syncDemod"] ? SAM_MODE : AM_MODE;
            }
            if (config->conf[name][getName()].contains("ecssSideband")) {
                std::string ecssSidebandStr = config->conf[name][getName()]["ecssSideband"];
                if (ecssSidebandStr == "LSB") {
                    ecssSidebandMode = LSB_MODE;
                }
                else if (ecssSidebandStr == "USB") {
                    ecssSidebandMode = USB_MODE;
                }
                else {
                    ecssSidebandMode = AUTO_MODE;
                }
            }
            config->release();

            // Define structure
            if (mode != AM_MODE) {
                samDemod.init(input,
                              getSAMMode(),
                              agcEnable,
                              gain,
                              dsp::demod::SAM<dsp::stereo_t>::PLLSpeed::MEDIUM,
                              bandwidth,
                              agcAttack / getIFSampleRate(),
                              agcDecay / getIFSampleRate(),
                              100.0 / getIFSampleRate(),
                              getIFSampleRate());
                samDemod.setECSSSidebandMode(getSAMECSSSidebandMode());
            }
            else {
                amDemod.init(input,
                             agcEnable,
                             gain,
                             bandwidth,
                             agcAttack / getIFSampleRate(),
                             agcDecay / getIFSampleRate(),
                             100.0 / getIFSampleRate(),
                             getIFSampleRate());
            }
        }

        void start() {
            if (mode != AM_MODE) {
                samDemod.start();
            }
            else {
                amDemod.start();
            }
        }

        void stop() {
            if (mode != AM_MODE) {
                samDemod.stop();
            }
            else {
                amDemod.stop();
            }
        }

        void showMenu() {
            float menuWidth = ImGui::GetContentRegionAvail().x;

            bool amSelected = (mode == AM_MODE);
            bool samSelected = (mode == SAM_MODE);
            bool ecssSelected = (mode == ECSS_MODE);
            if (ImGui::Checkbox(("AM##_radio_am_mode_am_" + name).c_str(), &amSelected) && amSelected && mode != AM_MODE) {
                setModeInternal(AM_MODE);
            }
            ImGui::SameLine();
            if (ImGui::Checkbox(("SAM##_radio_am_mode_sam_" + name).c_str(), &samSelected) && samSelected && mode != SAM_MODE) {
                setModeInternal(SAM_MODE);
            }
            ImGui::SameLine();
            if (ImGui::Checkbox(("ECSS##_radio_am_mode_ecss_" + name).c_str(), &ecssSelected) && ecssSelected && mode != ECSS_MODE) {
                setModeInternal(ECSS_MODE);
            }

            if (mode == ECSS_MODE) {
                ImGui::LeftLabel(_L("Sideband"));
                ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
                int ecssSidebandModeId = (int)ecssSidebandMode;
                if (ImGui::Combo(("##_radio_am_ecss_sideband_sel_" + name).c_str(), &ecssSidebandModeId, "AUTO (ECSS)\0LSB\0USB\0")) {
                    ecssSidebandMode = (ECSSSidebandMode)ecssSidebandModeId;
                    const char* ecssSidebandStr =
                        (ecssSidebandMode == LSB_MODE) ? "LSB" :
                        (ecssSidebandMode == USB_MODE) ? "USB" : "AUTO";
                    samDemod.setECSSSidebandMode(getSAMECSSSidebandMode());
                    _config->acquire();
                    _config->conf[name][getName()]["ecssSideband"] = ecssSidebandStr;
                    _config->release(true);
                }
            }

            ImGui::PushID("am_agc_enable");
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
                if (ImGui::SliderFloat(("##_radio_am_agc_attack_" + name).c_str(), &agcAttack, 1.0f, 200.0f)) {
                    setAGCAttackInternal(agcAttack / getIFSampleRate());
                    _config->acquire();
                    _config->conf[name][getName()]["agcAttack"] = agcAttack;
                    _config->release(true);
                }
                ImGui::LeftLabel(_L("AGC Decay"));
                ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
                if (ImGui::SliderFloat(("##_radio_am_agc_decay_" + name).c_str(), &agcDecay, 1.0f, 20.0f)) {
                    setAGCDecayInternal(agcDecay / getIFSampleRate());
                    _config->acquire();
                    _config->conf[name][getName()]["agcDecay"] = agcDecay;
                    _config->release(true);
                }
            }
            else {
                ImGui::LeftLabel(_L("Gain"));
                ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
                if (ImGui::SliderFloat(("##_radio_am_gain_" + name).c_str(), &gain, 0.0f, 100.0f, "%.1f dB")) {
                    setGainInternal(gain);
                    _config->acquire();
                    _config->conf[name][getName()]["gain"] = gain;
                    _config->release(true);
                }
            }
        }

        void setBandwidth(double bandwidth) {
            if (mode != AM_MODE) {
                samDemod.setBandwidth(bandwidth);
            }
            else {
                amDemod.setBandwidth(bandwidth);
            }
        }

        void setInput(dsp::stream<dsp::complex_t>* input) {
            if (mode != AM_MODE) {
                samDemod.setInput(input);
            }
            else {
                amDemod.setInput(input);
            }
        }

        void AFSampRateChanged(double newSR) {}

        // ============= INFO =============

        const char* getName() { return "AM"; }
        double getIFSampleRate() { return 15000.0; }
        double getAFSampleRate() { return getIFSampleRate(); }
        double getDefaultBandwidth() { return 10000.0; }
        double getMinBandwidth() { return 1000.0; }
        double getMaxBandwidth() { return getIFSampleRate(); }
        bool getBandwidthLocked() { return false; }
        double getDefaultSnapInterval() { return 1000.0; }
        int getVFOReference() { return ImGui::WaterfallVFO::REF_CENTER; }
        bool getDeempAllowed() { return true; }
        bool getPostProcEnabled() { return true; }
        int getDefaultDeemphasisMode() { return DEEMP_MODE_75US; }
        bool getFMIFNRAllowed() { return false; }
        bool getNBAllowed() { return true; }
        bool getANRAllowed() { return true; }
        bool getANFAllowed() { return true; }
        bool takeReinitRequest() {
            bool ret = reinitRequested;
            reinitRequested = false;
            return ret;
        }
        dsp::stream<dsp::stereo_t>* getOutput() {
            if (mode != AM_MODE) {
                return &samDemod.out;
            }
            return &amDemod.out;
        }

    private:
        void setModeInternal(Mode newMode) {
            mode = newMode;
            const char* modeStr = (mode == SAM_MODE) ? "SAM" : (mode == ECSS_MODE) ? "ECSS" : "AM";
            _config->acquire();
            _config->conf[name][getName()]["mode"] = modeStr;
            _config->release(true);
            reinitRequested = true;
        }

        dsp::demod::SAM<dsp::stereo_t>::Mode getSAMMode() const {
            return (mode == ECSS_MODE) ? dsp::demod::SAM<dsp::stereo_t>::ECSS : dsp::demod::SAM<dsp::stereo_t>::SAM_MODE;
        }

        dsp::demod::SAM<dsp::stereo_t>::ECSSSidebandMode getSAMECSSSidebandMode() const {
            switch (ecssSidebandMode) {
                case LSB_MODE:
                    return dsp::demod::SAM<dsp::stereo_t>::ECSS_LSB;
                case USB_MODE:
                    return dsp::demod::SAM<dsp::stereo_t>::ECSS_USB;
                default:
                    return dsp::demod::SAM<dsp::stereo_t>::ECSS_AUTO;
            }
        }

        void setAGCEnableInternal(bool enable) {
            if (mode != AM_MODE) {
                samDemod.setAGCEnable(enable);
            }
            else {
                amDemod.setAGCEnable(enable);
            }
        }

        void setAGCAttackInternal(double attack) {
            if (mode != AM_MODE) {
                samDemod.setAGCAttack(attack);
            }
            else {
                amDemod.setAGCAttack(attack);
            }
        }

        void setAGCDecayInternal(double decay) {
            if (mode != AM_MODE) {
                samDemod.setAGCDecay(decay);
            }
            else {
                amDemod.setAGCDecay(decay);
            }
        }

        void setGainInternal(double gainDb) {
            if (mode != AM_MODE) {
                samDemod.setGainDb(gainDb);
            }
            else {
                amDemod.setGainDb(gainDb);
            }
        }

        dsp::demod::AM<dsp::stereo_t> amDemod;
        dsp::demod::SAM<dsp::stereo_t> samDemod;

        ConfigManager* _config = NULL;

        Mode mode = AM_MODE;
        ECSSSidebandMode ecssSidebandMode = AUTO_MODE;
        bool reinitRequested = false;
        float agcAttack = 50.0f;
        float agcDecay = 5.0f;
        float gain = 0.0f;
        bool agcEnable = true;

        std::string name;
    };
}
#pragma once
#include "../demod.h"
#include <dsp/demod/ssb.h>
#include <dsp/demod/sam.h>

namespace demod {
    class SSB : public Demodulator {
    public:
        enum class Mode {
            USB,
            LSB,
            DSB
        };

        SSB(Mode mode = Mode::USB) {
            this->mode = mode;
        }

        ~SSB() {
            stop();
        }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            this->name = name;
            _config = config;

            // Load config
            config->acquire();
            if (config->conf[name][getConfigName()].contains("agcAttack")) {
                agcAttack = config->conf[name][getConfigName()]["agcAttack"];
            }
            if (config->conf[name][getConfigName()].contains("agcDecay")) {
                agcDecay = config->conf[name][getConfigName()]["agcDecay"];
            }
            if (config->conf[name][getConfigName()].contains("agcEnable")) {
                agcEnable = config->conf[name][getConfigName()]["agcEnable"];
            }
            if (config->conf[name][getConfigName()].contains("gain")) {
                gain = config->conf[name][getConfigName()]["gain"];
            }
            if (config->conf[name][getConfigName()].contains("syncDemod")) {
                syncDemod = config->conf[name][getConfigName()]["syncDemod"];
            }
            config->release();

            // Define structure
            if (syncDemod) {
                samDemod.init(input,
                              getSyncMode(),
                              agcEnable,
                              gain,
                              dsp::demod::SAM<dsp::stereo_t>::PLLSpeed::MEDIUM,
                              bandwidth,
                              agcAttack / getIFSampleRate(),
                              agcDecay / getIFSampleRate(),
                              100.0 / getIFSampleRate(),
                              getIFSampleRate());
            }
            else {
                ssbDemod.init(input, getDspMode(), agcEnable, gain, bandwidth, getIFSampleRate(), agcAttack / getIFSampleRate(), agcDecay / getIFSampleRate());
            }
        }

        void start() {
            if (syncDemod) {
                samDemod.start();
            }
            else {
                ssbDemod.start();
            }
        }

        void stop() {
            if (syncDemod) {
                samDemod.stop();
            }
            else {
                ssbDemod.stop();
            }
        }

        void showMenu() {
            float menuWidth = ImGui::GetContentRegionAvail().x;

            ImGui::PushID("ssb_sync_demod");
            if (ImGui::Checkbox("Sync Demod", &syncDemod)) {
                _config->acquire();
                _config->conf[name][getConfigName()]["syncDemod"] = syncDemod;
                _config->release(true);
                reinitRequested = true;
            }
            ImGui::PopID();

            ImGui::PushID("ssb_agc_enable");
            if (ImGui::Checkbox(_L("Enable AGC"), &agcEnable)) {
                setAGCEnableInternal(agcEnable);
                if (!agcEnable) {
                    setGainInternal(gain);
                }
                _config->acquire();
                _config->conf[name][getConfigName()]["agcEnable"] = agcEnable;
                _config->release(true);
            }
            ImGui::PopID();

            if (agcEnable) {
                ImGui::LeftLabel(_L("AGC Attack"));
                ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
                if (ImGui::SliderFloat(("##_radio_ssb_agc_attack_" + name).c_str(), &agcAttack, 1.0f, 200.0f)) {
                    setAGCAttackInternal(agcAttack / getIFSampleRate());
                    _config->acquire();
                    _config->conf[name][getConfigName()]["agcAttack"] = agcAttack;
                    _config->release(true);
                }
                ImGui::LeftLabel(_L("AGC Decay"));
                ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
                if (ImGui::SliderFloat(("##_radio_ssb_agc_decay_" + name).c_str(), &agcDecay, 1.0f, 20.0f)) {
                    setAGCDecayInternal(agcDecay / getIFSampleRate());
                    _config->acquire();
                    _config->conf[name][getConfigName()]["agcDecay"] = agcDecay;
                    _config->release(true);
                }
            }
            else {
                ImGui::LeftLabel(_L("Gain"));
                ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
                if (ImGui::SliderFloat(("##_radio_ssb_gain_" + name).c_str(), &gain, 0.0f, 100.0f, "%.1f dB")) {
                    setGainInternal(gain);
                    _config->acquire();
                    _config->conf[name][getConfigName()]["gain"] = gain;
                    _config->release(true);
                }
            }
        }

        void setBandwidth(double bandwidth) {
            if (syncDemod) {
                samDemod.setBandwidth(bandwidth);
            }
            else {
                ssbDemod.setBandwidth(bandwidth);
            }
        }

        void setInput(dsp::stream<dsp::complex_t>* input) {
            if (syncDemod) {
                samDemod.setInput(input);
            }
            else {
                ssbDemod.setInput(input);
            }
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
            if (syncDemod) {
                return &samDemod.out;
            }
            return &ssbDemod.out;
        }

    private:
        void setAGCEnableInternal(bool enable) {
            if (syncDemod) {
                samDemod.setAGCEnable(enable);
            }
            else {
                ssbDemod.setAGCEnable(enable);
            }
        }

        void setAGCAttackInternal(double attack) {
            if (syncDemod) {
                samDemod.setAGCAttack(attack);
            }
            else {
                ssbDemod.setAGCAttack(attack);
            }
        }

        void setAGCDecayInternal(double decay) {
            if (syncDemod) {
                samDemod.setAGCDecay(decay);
            }
            else {
                ssbDemod.setAGCDecay(decay);
            }
        }

        void setGainInternal(double gainDb) {
            if (syncDemod) {
                samDemod.setGainDb(gainDb);
            }
            else {
                ssbDemod.setGainDb(gainDb);
            }
        }

        dsp::demod::SSB<dsp::stereo_t>::Mode getDspMode() {
            if (mode == Mode::USB) { return dsp::demod::SSB<dsp::stereo_t>::Mode::USB; }
            if (mode == Mode::LSB) { return dsp::demod::SSB<dsp::stereo_t>::Mode::LSB; }
            return dsp::demod::SSB<dsp::stereo_t>::Mode::DSB;
        }

        dsp::demod::SAM<dsp::stereo_t>::Mode getSyncMode() {
            if (mode == Mode::USB) { return dsp::demod::SAM<dsp::stereo_t>::USB; }
            if (mode == Mode::LSB) { return dsp::demod::SAM<dsp::stereo_t>::LSB; }
            return dsp::demod::SAM<dsp::stereo_t>::SAM_MODE;
        }

        const char* getConfigName() {
            if (mode == Mode::USB) { return "USB"; }
            if (mode == Mode::LSB) { return "LSB"; }
            return "DSB";
        }

        Mode mode = Mode::USB;
        dsp::demod::SSB<dsp::stereo_t> ssbDemod;
        dsp::demod::SAM<dsp::stereo_t> samDemod;

        ConfigManager* _config;

        bool syncDemod = false;
        bool reinitRequested = false;
        bool agcEnable = true;
        float agcAttack = 50.0f;
        float agcDecay = 5.0f;
        float gain = 0.0f;

        std::string name;
    };
}
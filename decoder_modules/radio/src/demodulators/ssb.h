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
            config->release();

            // Define structure
            demod.init(input, getDspMode(), agcEnable, gain, bandwidth, getIFSampleRate(), agcAttack / getIFSampleRate(), agcDecay / getIFSampleRate());
        }

        void start() { demod.start(); }

        void stop() { demod.stop(); }

        void showMenu() {
            float menuWidth = ImGui::GetContentRegionAvail().x;

            ImGui::PushID("ssb_agc_enable");
            if (ImGui::Checkbox(_L("Enable AGC"), &agcEnable)) {
                demod.setAGCEnable(agcEnable);
                if (!agcEnable) {
                    demod.setGainDb(gain);
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
                    demod.setAGCAttack(agcAttack / getIFSampleRate());
                    _config->acquire();
                    _config->conf[name][getConfigName()]["agcAttack"] = agcAttack;
                    _config->release(true);
                }
                ImGui::LeftLabel(_L("AGC Decay"));
                ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
                if (ImGui::SliderFloat(("##_radio_ssb_agc_decay_" + name).c_str(), &agcDecay, 1.0f, 20.0f)) {
                    demod.setAGCDecay(agcDecay / getIFSampleRate());
                    _config->acquire();
                    _config->conf[name][getConfigName()]["agcDecay"] = agcDecay;
                    _config->release(true);
                }
            }
            else {
                ImGui::LeftLabel(_L("Gain"));
                ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
                if (ImGui::SliderFloat(("##_radio_ssb_gain_" + name).c_str(), &gain, 0.0f, 100.0f, "%.1f dB")) {
                    demod.setGainDb(gain);
                    _config->acquire();
                    _config->conf[name][getConfigName()]["gain"] = gain;
                    _config->release(true);
                }
            }
        }

        void setBandwidth(double bandwidth) { demod.setBandwidth(bandwidth); }

        void setInput(dsp::stream<dsp::complex_t>* input) { demod.setInput(input); }

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
        dsp::stream<dsp::stereo_t>* getOutput() { return &demod.out; }

    private:
        dsp::demod::SSB<dsp::stereo_t>::Mode getDspMode() {
            if (mode == Mode::USB) { return dsp::demod::SSB<dsp::stereo_t>::Mode::USB; }
            if (mode == Mode::LSB) { return dsp::demod::SSB<dsp::stereo_t>::Mode::LSB; }
            return dsp::demod::SSB<dsp::stereo_t>::Mode::DSB;
        }

        const char* getConfigName() {
            if (mode == Mode::USB) { return "USB"; }
            if (mode == Mode::LSB) { return "LSB"; }
            return "DSB";
        }

        Mode mode = Mode::USB;
        dsp::demod::SSB<dsp::stereo_t> demod;

        ConfigManager* _config;

        bool agcEnable = true;
        float agcAttack = 50.0f;
        float agcDecay = 5.0f;
        float gain = 0.0f;

        std::string name;
    };
}
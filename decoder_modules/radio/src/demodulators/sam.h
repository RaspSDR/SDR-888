#pragma once
#include "../demod.h"
#include <dsp/demod/sam.h>

namespace demod {
    class SAM : public Demodulator {
    public:
        SAM() {}

        SAM(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            init(name, config, input, bandwidth, audioSR);
        }

        ~SAM() { stop(); }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            this->name = name;
            _config = config;

            _mode = dsp::demod::SAM<dsp::stereo_t>::SAM_MODE;

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
            config->release();

            // Define structure
            demod.init(input,
                       _mode,
                       agcEnable,
                       gain,
                       dsp::demod::SAM<dsp::stereo_t>::PLLSpeed::MEDIUM,
                       bandwidth,
                       agcAttack / getIFSampleRate(),
                       agcDecay / getIFSampleRate(),
                       100.0 / getIFSampleRate(),
                       getIFSampleRate());
        }

        void start() { demod.start(); }

        void stop() { demod.stop(); }

        void showMenu() {
            float menuWidth = ImGui::GetContentRegionAvail().x;

            ImGui::PushID("sam_agc_enable");
            if (ImGui::Checkbox(_L("Enable AGC"), &agcEnable)) {
                demod.setAGCEnable(agcEnable);
                if (!agcEnable) {
                    demod.setGainDb(gain);
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
                    demod.setAGCAttack(agcAttack / getIFSampleRate());
                    _config->acquire();
                    _config->conf[name][getName()]["agcAttack"] = agcAttack;
                    _config->release(true);
                }
                ImGui::LeftLabel(_L("AGC Decay"));
                ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
                if (ImGui::SliderFloat(("##_radio_am_agc_decay_" + name).c_str(), &agcDecay, 1.0f, 20.0f)) {
                    demod.setAGCDecay(agcDecay / getIFSampleRate());
                    _config->acquire();
                    _config->conf[name][getName()]["agcDecay"] = agcDecay;
                    _config->release(true);
                }
            }
            else {
                ImGui::LeftLabel(_L("Gain"));
                ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
                if (ImGui::SliderFloat(("##_radio_sam_gain_" + name).c_str(), &gain, 0.0f, 100.0f, "%.1f dB")) {
                    demod.setGainDb(gain);
                    _config->acquire();
                    _config->conf[name][getName()]["gain"] = gain;
                    _config->release(true);
                }
            }

            bool stero = (_mode == dsp::demod::SAM<dsp::stereo_t>::STEREO);
            ImGui::PushID("sam_stereo_mode");
            if (ImGui::Checkbox(_L("Stereo"), &stero)) {
                if (!stero) {
                    _mode = dsp::demod::SAM<dsp::stereo_t>::SAM_MODE;
                }
                else {
                    _mode = dsp::demod::SAM<dsp::stereo_t>::STEREO;
                }
                demod.setMode(_mode);
                _config->acquire();
                _config->conf[name][getName()]["stereoMode"] = (_mode == dsp::demod::SAM<dsp::stereo_t>::STEREO);
                _config->release(true);
            }
            ImGui::PopID();
        }

        void setBandwidth(double bandwidth) { demod.setBandwidth(bandwidth); }

        void setInput(dsp::stream<dsp::complex_t>* input) { demod.setInput(input); }

        void AFSampRateChanged(double newSR) {}

        // ============= INFO =============

        const char* getName() { return "SAM"; }
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
        dsp::stream<dsp::stereo_t>* getOutput() { return &demod.out; }

    private:
        dsp::demod::SAM<dsp::stereo_t> demod;
        dsp::demod::SAM<dsp::stereo_t>::Mode _mode;
        ConfigManager* _config = NULL;

        float agcAttack = 50.0f;
        float agcDecay = 5.0f;
        float gain = 0.0f;
        bool agcEnable = true;


        std::string name;
    };
}
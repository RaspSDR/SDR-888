#pragma once
#include "../demod.h"
#include <dsp/demod/am.h>
#include <dsp/demod/sam.h>

namespace demod {
    class AM : public Demodulator {
    public:
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
            if (config->conf[name][getName()].contains("syncDemod")) {
                syncDemod = config->conf[name][getName()]["syncDemod"];
            }
            config->release();

            // Define structure
            if (syncDemod) {
                samDemod.init(input,
                              dsp::demod::SAM<dsp::stereo_t>::SAM_MODE,
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
            if (syncDemod) {
                samDemod.start();
            }
            else {
                amDemod.start();
            }
        }

        void stop() {
            if (syncDemod) {
                samDemod.stop();
            }
            else {
                amDemod.stop();
            }
        }

        void showMenu() {
            float menuWidth = ImGui::GetContentRegionAvail().x;

            ImGui::PushID("am_sync_demod");
            if (ImGui::Checkbox("Sync Demod", &syncDemod)) {
                _config->acquire();
                _config->conf[name][getName()]["syncDemod"] = syncDemod;
                _config->release(true);
                reinitRequested = true;
            }
            ImGui::PopID();

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
            if (syncDemod) {
                samDemod.setBandwidth(bandwidth);
            }
            else {
                amDemod.setBandwidth(bandwidth);
            }
        }

        void setInput(dsp::stream<dsp::complex_t>* input) {
            if (syncDemod) {
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
            if (syncDemod) {
                return &samDemod.out;
            }
            return &amDemod.out;
        }

    private:
        void setAGCEnableInternal(bool enable) {
            if (syncDemod) {
                samDemod.setAGCEnable(enable);
            }
            else {
                amDemod.setAGCEnable(enable);
            }
        }

        void setAGCAttackInternal(double attack) {
            if (syncDemod) {
                samDemod.setAGCAttack(attack);
            }
            else {
                amDemod.setAGCAttack(attack);
            }
        }

        void setAGCDecayInternal(double decay) {
            if (syncDemod) {
                samDemod.setAGCDecay(decay);
            }
            else {
                amDemod.setAGCDecay(decay);
            }
        }

        void setGainInternal(double gainDb) {
            if (syncDemod) {
                samDemod.setGainDb(gainDb);
            }
            else {
                amDemod.setGainDb(gainDb);
            }
        }

        dsp::demod::AM<dsp::stereo_t> amDemod;
        dsp::demod::SAM<dsp::stereo_t> samDemod;

        ConfigManager* _config = NULL;

        bool syncDemod = false;
        bool reinitRequested = false;
        float agcAttack = 50.0f;
        float agcDecay = 5.0f;
        float gain = 0.0f;
        bool agcEnable = true;

        std::string name;
    };
}
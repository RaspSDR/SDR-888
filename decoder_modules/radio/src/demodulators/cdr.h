#pragma once
#include "../demod.h"
#include "cdr/CDRReceiver.h"

namespace demod {
    class CDR : public Demodulator {
    public:
        CDR() {
        }

        CDR(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            init(name, config, input, bandwidth, audioSR);
        }

        ~CDR() {
            if (cdrReceiver) {
                delete cdrReceiver;
            }
        }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            this->name = name;
            audioSampleRate = audioSR;

            cdrReceiver = new CDRReceiver();
            cdrReceiver->init(input);

            Settings.Put("Receiver", "samplerateaud", (int)getIFSampleRate());
            Settings.Put("Receiver", "sampleratesig", (int)getIFSampleRate());
        }

        void start() {
            cdrReceiver->start();
        }

        void stop() {
            cdrReceiver->stop();
        }

        void showMenu() {
            if (programe_num > 1) {
                ImGui::Text("Program Number: %d", current_programe + 1);
            }
        }

        void setBandwidth(double bandwidth) {
            
        }

        void setInput(dsp::stream<dsp::complex_t>* input) {
            cdrReceiver->setInput(input);
        }

        void AFSampRateChanged(double newSR) {
            audioSampleRate = newSR;
        }

        // ============= INFO =============

        const char* getName() { return "CDR"; }
        double getIFSampleRate() { return 816000.0; }
        double getAFSampleRate() { return 48000.0; }
        double getDefaultBandwidth() { return 816000.0; }
        double getMinBandwidth() { return getDefaultBandwidth(); }
        double getMaxBandwidth() { return getDefaultBandwidth(); }
        bool getBandwidthLocked() { return true; }
        double getDefaultSnapInterval() { return 1000.0; }
        int getVFOReference() { return ImGui::WaterfallVFO::REF_CENTER; }
        bool getDeempAllowed() { return false; }
        bool getPostProcEnabled() { return false; }
        int getDefaultDeemphasisMode() { return DEEMP_MODE_NONE; }
        bool getFMIFNRAllowed() { return false; }
        bool getNBAllowed() { return false; }
        dsp::stream<dsp::stereo_t>* getOutput() { return &cdrReceiver->out; }

    private:
        double audioSampleRate;
        CDRReceiver* cdrReceiver = nullptr;
        
        CSettings Settings;

        std::string name;

        int programe_num = 1;
        int current_programe = 0;
    };
}
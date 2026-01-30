#pragma once
#include "../demod.h"
#include "cdr/CDRReceiver.h"

namespace demod {
    class CDR : public Demodulator {
    public:
        CDR() {
            cdrReceiver = new CDRReceiver();
        }

        CDR(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            init(name, config, input, bandwidth, audioSR);
        }

        ~CDR() {
            stop();

            if (cdrReceiver) {
                delete cdrReceiver;
            }
        }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            this->name = name;
            audioSampleRate = audioSR;

            cdrReceiver->init(input, audioSR);
        }

        void start() {
            cdrReceiver->start();
        }

        void stop() {
            cdrReceiver->stop();
        }

        void showMenu() {
            int currentProg = cdrReceiver->getCurrentProgreme();
            int programe_num = cdrReceiver->getProgremeNum();

            if (programe_num == 0)
                return;

            if (programe_num <= 1)
                ImGui::BeginDisabled();

            // Create program list string for combo box
            std::string programList = "";
            for (int i = 0; i < programe_num; i++) {
                programList += "Program " + std::to_string(i + 1);
                if (i < programe_num - 1) {
                    programList += '\0';
                }
            }
            programList += '\0';

            ImGui::LeftLabel(_L("Program"));
            ImGui::FillWidth();
            if (ImGui::Combo("##_cdr_program", &currentProg, programList.c_str())) {
                cdrReceiver->setCurrentProgreme(currentProg);
            }

            if (programe_num <= 1)
                ImGui::EndDisabled();
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

        std::string name;
    };
}
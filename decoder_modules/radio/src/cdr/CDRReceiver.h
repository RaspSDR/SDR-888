#pragma once

#include "include/cdrDemod.h"
#include "include/draDecoder.h"

#include <dsp/processor.h>
#include <dsp/multirate/rational_resampler.h>

class CDRReceiver : public dsp::Processor<dsp::complex_t, dsp::stereo_t> {
    using base_type = dsp::Processor<dsp::complex_t, dsp::stereo_t>;

public:
    CDRReceiver() : cdrReceiver(nullptr), decoder(nullptr) {
    }

    ~CDRReceiver() {
        if (decoder) {
            draDecoder_Release(decoder);
            decoder = nullptr;
        }
        if (cdrReceiver) {
            CDRDemodulation_Release(cdrReceiver);
            cdrReceiver = nullptr;
        }
    }

    int process(int count, const dsp::complex_t* in, dsp::stereo_t* out) {
        int running_progreme;
        {
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            running_progreme = this->current_programe;
        }

        int err = CDRDemodulation_Process(cdrReceiver, (cdr_complex*)in, count);

        if (err != cdr_NO_ERROR) {
            return 0;
        }

        int bufferSize = CDRDemodulation_GetBufferSize(cdrReceiver);
        programe_num = CDRDemodulation_GetNumOfPrograms(cdrReceiver);
        if (running_progreme >= programe_num) {
            running_progreme = 0;
        }

        int draLength;
        cdr_byte* draStream = CDRDemodulation_GetDraStream(cdrReceiver, running_progreme, &draLength);
        if (draLength <= 0) {
            // no audio available
            return 0;
        }

        err = draDecoder_Process(decoder, draStream, draLength);
        if (err) {
            return 0; // Return 0 samples, not error
        }

        int outputLength = 0;
        const short* data = draDecoder_GetAudioStream(decoder, &outputLength);
        if (!data || outputLength <= 0) {
            return 0;
        }

        int channels = draDecoder_GetAudioChannels(decoder);
        int sampleRate = draDecoder_GetAudioSampleRate(decoder);

        if (sampleRate != input_samplerate) {
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            input_samplerate = sampleRate;
            resamp.setRates(input_samplerate, output_samplerate);
        }

        if (channels == 1) {
            size_t requiredSize = (size_t)outputLength;
            if (audioBuffer.size() < requiredSize) {
                audioBuffer.resize(requiredSize);
            }
            const float scale = 1.0 / 32768.0f;
            // Convert mono to stereo
            dsp::stereo_t* bufPtr = audioBuffer.data();
            for (int i = 0; i < outputLength; i++) {
                float sample = ((float)data[i]) * scale;
                bufPtr[i].l = sample;
                bufPtr[i].r = sample;
            }
            return resamp.process(outputLength, bufPtr, out);
        }
        else if (channels == 2) {
            size_t requiredSize = (size_t)outputLength;
            if (audioBuffer.size() < requiredSize) {
                audioBuffer.resize(requiredSize);
            }
            const float scale = 1.0 / 32768.0f;
            dsp::stereo_t* bufPtr = audioBuffer.data();
            for (int i = 0; i < outputLength / 2; i++) {
                bufPtr[i].l = ((float)data[i * 2]) * scale;
                bufPtr[i].r = ((float)data[i * 2 + 1]) * scale;
            }
            return resamp.process(outputLength / 2, bufPtr, out);
        }

        return 0;
    }

    void init(dsp::stream<dsp::complex_t>* in, double audioSampleRate) {
        cdrReceiver = CDRDemodulation_Init();
        decoder = draDecoder_Init();
        programe_num = 0;
        current_programe = 0;
        output_samplerate = audioSampleRate;
        input_samplerate = 24000.0;
        resamp.init(NULL, input_samplerate, output_samplerate);
        base_type::init(in);
    }

    DEFAULT_MULTIRATE_PROC_RUN;

    int getProgremeNum() {
        return programe_num;
    }
    void setCurrentProgreme(int prog) {
        std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
        current_programe = prog;
    }

    int getCurrentProgreme() {
        return current_programe;
    }

private:
    cdrDemodHandle cdrReceiver;
    draDecoderHandle decoder;
    dsp::multirate::RationalResampler<dsp::stereo_t> resamp;

    double output_samplerate;
    double input_samplerate;

    int current_programe = 0;
    int programe_num = 0;
    std::vector<dsp::stereo_t> audioBuffer;
};
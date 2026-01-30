#pragma once

#include "include/cdrDemod.h"
#include "include/draDecoder.h"

#include <dsp/processor.h>
#include <dsp/multirate/rational_resampler.h>

class CDRReceiver : public dsp::Processor<dsp::complex_t, dsp::stereo_t> {
    using base_type = dsp::Processor<dsp::complex_t, dsp::stereo_t>;
public:
    CDRReceiver() {
        tempbuf = new float[163840 * 2];
    }

    ~CDRReceiver() {
        delete[] tempbuf;
        draDecoder_Release(decoder);
        CDRDemodulation_Release(cdrReceiver);
    }

    int process(int count, const dsp::complex_t* in, dsp::stereo_t* out)
    {
        // fill input until it is full, process it, and repeat
        if (input_buf_pos + count > 130560) {
            size_t to_copy = 130560 - input_buf_pos;
            memcpy(&input_buf[input_buf_pos], in, to_copy * sizeof(dsp::complex_t));
            input_buf_pos += to_copy;
            in += to_copy;
            count -= to_copy;
        } else {
            memcpy(&input_buf[input_buf_pos], in, count * sizeof(dsp::complex_t));
            input_buf_pos += count;
            return 0;
        }

        int err = CDRDemodulation_Process(cdrReceiver, (cdr_complex*)input_buf, 130560);

        // copy the remaining input to the buffer
        size_t remaining = count;
        if (remaining > 0) {
            memcpy(&input_buf[0], in, remaining * sizeof(dsp::complex_t));
        }

        int bufferSize = CDRDemodulation_GetBufferSize(cdrReceiver);
        programe_num = CDRDemodulation_GetNumOfPrograms(cdrReceiver);
        if (current_programe >= programe_num) {
            current_programe = 0;
        }

        if (current_programe != running_progreme) {
            running_progreme = current_programe;
            draDecoder_Release(decoder);
            decoder = draDecoder_Init();
        }

        int draLength;
        cdr_byte* draStream = CDRDemodulation_GetDraStream(cdrReceiver, current_programe, &draLength);
        if (draLength <= 0) {
            // no audio available
            return 0;
        }

        err = draDecoder_Process(decoder, draStream, draLength);
        if (err != 0) {
            return -1;
        }

        int outputLength;
        const short* data = draDecoder_GetAudioStream(decoder, &outputLength);
        int channels = draDecoder_GetAudioChannels(decoder);

        if (channels == 1) {
            // interleave mono to stereo
            for (int i = 0; i < outputLength; i++) {
                tempbuf[i * 2] = ((float)data[i]) / 32768.0f;
                tempbuf[i * 2 + 1] = ((float)data[i]) / 32768.0f;
            }
            return outputLength;
        }
        else if (channels == 2) {
            for (int i = 0; i < outputLength / 2; i++) {
                out[i].l = ((float)data[i * 2]) / 32768.0f;
                out[i].r = ((float)data[i * 2 + 1]) / 32768.0f;
            }

            return outputLength / 2;
        }

        return 0;
    }

    void init(dsp::stream<dsp::complex_t>* in) override {
        cdrReceiver = CDRDemodulation_Init();
        decoder = draDecoder_Init();
        base_type::init(in);
    }

    DEFAULT_PROC_RUN;

private:
    cdrDemodHandle cdrReceiver;
    draDecoderHandle decoder;
    dsp::multirate::RationalResampler<dsp::stereo_t> resamp;

    bool running = false;
    int current_programe = 0;
    int programe_num = 0;
    int running_progreme = -1;
    float* tempbuf;

    dsp::complex_t input_buf[130560];
    size_t input_buf_pos = 0;
};
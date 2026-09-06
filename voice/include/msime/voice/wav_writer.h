#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <limits>
#include "stt_service.h"

namespace metasequoia::voice
{

class WavWriter
{
  public:
    // The PCM recognizer uses the default 60-second budget. Hosts encoding longer
    // bounded uploads can specify their own limit without copying this encoder.
    static std::vector<uint8_t> create_wav(const std::vector<float> &pcm_data, int sample_rate = 16000, std::size_t sample_limit = maximum_samples)
    {
        if (sample_rate != metasequoia::voice::sample_rate || pcm_data.size() > sample_limit || pcm_data.size() > ((std::numeric_limits<uint32_t>::max)() - 36u) / 2u)
            throw VoiceError("Expected mono 16 kHz audio within the selected WAV sample limit");
        std::vector<uint8_t> wav_data;

        // Convert float PCM to 16-bit integers
        std::vector<int16_t> pcm16;
        pcm16.reserve(pcm_data.size());
        for (float s : pcm_data)
        {
            if (!std::isfinite(s))
                throw VoiceError("Audio contains a non-finite sample");
            // Clip
            if (s > 1.0f)
                s = 1.0f;
            if (s < -1.0f)
                s = -1.0f;
            pcm16.push_back(static_cast<int16_t>(s * 32767.0f));
        }

        uint32_t data_size = static_cast<uint32_t>(pcm16.size() * sizeof(int16_t));
        uint32_t riff_size = 36 + data_size;

        // RIFF header
        write_cw(wav_data, "RIFF");
        write_u32(wav_data, riff_size);
        write_cw(wav_data, "WAVE");

        // fmt chunk
        write_cw(wav_data, "fmt ");
        write_u32(wav_data, 16);              // Chunk size
        write_u16(wav_data, 1);               // PCM format
        write_u16(wav_data, 1);               // Channels (Mono)
        write_u32(wav_data, sample_rate);     // Sample rate
        write_u32(wav_data, sample_rate * 2); // Byte rate (SampleRate * BlockAlign)
        write_u16(wav_data, 2);               // Block align (Channels * BitsPerSample / 8)
        write_u16(wav_data, 16);              // Bits per sample

        // data chunk
        write_cw(wav_data, "data");
        write_u32(wav_data, data_size);

        // Append PCM data
        for (int16_t sample : pcm16)
            write_u16(wav_data, static_cast<uint16_t>(sample));

        return wav_data;
    }

  private:
    static void write_cw(std::vector<uint8_t> &buf, const char *str)
    {
        buf.insert(buf.end(), str, str + 4);
    }
    static void write_u32(std::vector<uint8_t> &buf, uint32_t val)
    {
        buf.push_back(val & 0xFF);
        buf.push_back((val >> 8) & 0xFF);
        buf.push_back((val >> 16) & 0xFF);
        buf.push_back((val >> 24) & 0xFF);
    }
    static void write_u16(std::vector<uint8_t> &buf, uint16_t val)
    {
        buf.push_back(val & 0xFF);
        buf.push_back((val >> 8) & 0xFF);
    }
};

} // namespace metasequoia::voice

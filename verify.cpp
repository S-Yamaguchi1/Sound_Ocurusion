// 各フィルタのインパルス応答をCSVに出力し（FFT解析用）、
// フィルタを通した正弦スイープをWAVに出力する（聴感確認用）。
//
//   cl /utf-8 /EHsc /O2 /std:c++17 verify.cpp /Fe:verify.exe
//   g++ -O2 -std=c++17 verify.cpp -o verify
#include "dsp/Filters.h"

#include <vector>
#include <fstream>
#include <cstdint>
#include <iostream>

static constexpr float kSampleRate = 48000.0f;
static constexpr float kCutoffHz   = 1000.0f;
static constexpr int   kIRLength   = 4096;

template <typename Filter>
std::vector<float> RenderImpulseResponse(Filter filter)
{
    filter.SetSampleRate(kSampleRate);
    filter.SetCutoff(kCutoffHz);
    filter.Reset();

    std::vector<float> ir(kIRLength, 0.0f);
    for (int n = 0; n < kIRLength; ++n)
    {
        ir[n] = filter.Process(n == 0 ? 1.0f : 0.0f);
    }
    return ir;
}

void WriteCsv(const std::vector<float>& onepole,
              const std::vector<float>& biquad,
              const std::vector<float>& svf)
{
    std::ofstream f("impulse_response.csv");
    f << "index,onepole,biquad,svf\n";
    for (int n = 0; n < kIRLength; ++n)
    {
        f << n << ',' << onepole[n] << ',' << biquad[n] << ',' << svf[n] << '\n';
    }
    std::cout << "wrote impulse_response.csv\n";
}

void WriteWavMono16(const char* path, const std::vector<float>& samples, int sampleRate)
{
    auto put32 = [](std::ofstream& o, uint32_t v){ o.write(reinterpret_cast<char*>(&v), 4); };
    auto put16 = [](std::ofstream& o, uint16_t v){ o.write(reinterpret_cast<char*>(&v), 2); };

    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * 2);
    std::ofstream o(path, std::ios::binary);
    o.write("RIFF", 4); put32(o, 36 + dataBytes); o.write("WAVE", 4);
    o.write("fmt ", 4); put32(o, 16); put16(o, 1); put16(o, 1);
    put32(o, sampleRate); put32(o, sampleRate * 2); put16(o, 2); put16(o, 16);
    o.write("data", 4); put32(o, dataBytes);
    for (float s : samples)
    {
        s = s >  1.0f ?  1.0f : s;
        s = s < -1.0f ? -1.0f : s;
        put16(o, static_cast<uint16_t>(static_cast<int16_t>(s * 32767.0f)));
    }
    std::cout << "wrote " << path << '\n';
}

// 20Hz -> 20kHz の対数スイープをSVFに通す
void WriteFilteredSweep()
{
    const float duration = 5.0f;
    const int   N  = static_cast<int>(kSampleRate * duration);
    const float f0 = 20.0f, f1 = 20000.0f;
    const float K  = std::log(f1 / f0);

    dsp::SvfTPT lp;
    lp.SetSampleRate(kSampleRate);
    lp.SetCutoff(kCutoffHz);
    lp.Reset();

    std::vector<float> out(N);
    float phase = 0.0f;
    for (int n = 0; n < N; ++n)
    {
        const float t    = static_cast<float>(n) / kSampleRate;
        const float freq = f0 * std::exp(K * t / duration);
        phase += 2.0f * dsp::kPi * freq / kSampleRate;
        out[n] = lp.Process(0.5f * std::sin(phase));
    }
    WriteWavMono16("filtered_sweep.wav", out, static_cast<int>(kSampleRate));
}

int main()
{
    std::cout << "fs = " << kSampleRate << " Hz, cutoff = " << kCutoffHz << " Hz\n";

    WriteCsv(RenderImpulseResponse(dsp::OnePoleLPF{}),
             RenderImpulseResponse(dsp::BiquadLPF{}),
             RenderImpulseResponse(dsp::SvfTPT{}));
    WriteFilteredSweep();

    std::cout << "next: python plot_response.py\n";
    return 0;
}

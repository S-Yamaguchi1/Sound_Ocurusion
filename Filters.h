// ローパスフィルタ
// インパルス応答をFFTした結果はDocs/frequency_response.pngに保存
#pragma once
#include <cmath>

namespace dsp
{
    constexpr float kPi = 3.14159265358979323846f;

    // 1次 1ポール・ローパス。-6 dB/oct。
    struct OnePoleLPF
    {
        float z  = 0.0f;
        float a  = 1.0f;
        float fs = 48000.0f;

        void SetSampleRate(float sampleRate) { fs = sampleRate; }
        void SetCutoff(float cutoffHz) { a = 1.0f - std::exp(-2.0f * kPi * cutoffHz / fs); }
        void Reset() { z = 0.0f; }

        float Process(float x)
        {
            z += a * (x - z);
            return z;
        }
    };

    // 2次 Biquad ローパス
    // Q = 0.707 でバターワース（カットオフで -3 dB）　
    struct BiquadLPF
    {
        float b0=1, b1=0, b2=0, a1=0, a2=0;
        float x1=0, x2=0, y1=0, y2=0;
        float fs = 48000.0f;

        void SetSampleRate(float sampleRate) { fs = sampleRate; }

        void SetCutoff(float cutoffHz, float Q = 0.70710678f)
        {
            const float w     = 2.0f * kPi * cutoffHz / fs;
            const float cosw  = std::cos(w);
            const float alpha = std::sin(w) / (2.0f * Q);
            const float a0    = 1.0f + alpha;

            b0 = (1.0f - cosw) * 0.5f / a0;
            b1 = (1.0f - cosw)        / a0;
            b2 = b0;
            a1 = (-2.0f * cosw)       / a0;
            a2 = (1.0f - alpha)       / a0;
        }

        void Reset() { x1=x2=y1=y2=0.0f; }

        float Process(float x)
        {
            const float y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
    };

    // State Variable Filte
    //
    // 
    // 採用したふぃるた
    // TPT 構造を採用したDirect Form の Biquad は安定しない
    struct SvfTPT
    {
        float g=0, k=1, a1=0, a2=0, a3=0;
        float ic1=0, ic2=0;   // 積分器の状態
        float fs = 48000.0f;

        void SetSampleRate(float sampleRate) { fs = sampleRate; }

        void SetCutoff(float cutoffHz, float Q = 0.70710678f)
        {
            g  = std::tan(kPi * cutoffHz / fs);
            k  = 1.0f / Q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void Reset() { ic1 = ic2 = 0.0f; }

        // 出力。必要ならハイパスは v0 - k*v1 - v2 で同時に取得
        float Process(float v0)
        {
            const float v3 = v0  - ic2;
            const float v1 = a1*ic1 + a2*v3;
            const float v2 = ic2 + a2*ic1 + a3*v3;
            ic1 = 2.0f*v1 - ic1;
            ic2 = 2.0f*v2 - ic2;
            return v2;
        }
    };

}
 // namespace dsp

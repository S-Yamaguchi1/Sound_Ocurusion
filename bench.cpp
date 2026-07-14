// フィルタのコスト測定（エンジン非依存）。
//
//   カットオフ固定時の、各フィルタの1サンプルあたりコスト
//   変調時のSVF: 係数を毎サンプル再計算する場合（内側ループで tan()）と、
//       ブロック単位で更新する場合の比較。
//      
//
/
//  
#include "dsp/Filters.h"

#include <vector>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <random>
#include <fstream>

using Clock = std::chrono::high_resolution_clock;

static constexpr float kSampleRate = 48000.0f;
static constexpr int   kNumSamples = 2'000'000;
static constexpr int   kBlockSize  = 64;

// 処理が削除されないよう、結果を集約
static volatile float g_sink = 0.0f;

// 経過時間を ns/sample で返す
template <typename Fn>
double MeasureNsPerSample(Fn&& fn)
{
    const auto t0 = Clock::now();
    const float acc = fn();
    const auto t1 = Clock::now();
    g_sink += acc; // sinkに入れて削除を防ぐ
    const double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return ns / static_cast<double>(kNumSamples);
}

int main()
{
    // 入力のノイズ
    std::vector<float> in(kNumSamples);
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (float& x : in) x = dist(rng);

    std::printf("samples = %d, block = %d, fs = %.0f Hz\n\n", kNumSamples, kBlockSize, kSampleRate);
    std::printf("%-34s %12s\n", "case", "ns/sample");
    std::printf("--------------------------------------------------\n");

    std::vector<std::pair<const char*, double>> results;

    // (A) カットオフ固定時
    {
        dsp::OnePoleLPF f; f.SetSampleRate(kSampleRate); f.SetCutoff(1000.0f); f.Reset();
        const double ns = MeasureNsPerSample([&]{
            float a = 0; for (int i = 0; i < kNumSamples; ++i) a += f.Process(in[i]); return a; });
        results.push_back({"OnePole (fixed cutoff)", ns});
    }
    {
        dsp::BiquadLPF f; f.SetSampleRate(kSampleRate); f.SetCutoff(1000.0f, 0.707f); f.Reset();
        const double ns = MeasureNsPerSample([&]{
            float a = 0; for (int i = 0; i < kNumSamples; ++i) a += f.Process(in[i]); return a; });
        results.push_back({"Biquad (fixed cutoff)", ns});
    }
    {
        dsp::SvfTPT f; f.SetSampleRate(kSampleRate); f.SetCutoff(1000.0f); f.Reset();
        const double ns = MeasureNsPerSample([&]{
            float a = 0; for (int i = 0; i < kNumSamples; ++i) a += f.Process(in[i]); return a; });
        results.push_back({"SVF (fixed cutoff)", ns});
    }

    // (B) 係数更新を毎サンプル行う場合とブロック単位で行う場合の比較
    auto cutoffAt = [](int i){
        const float t = static_cast<float>(i) / kSampleRate;
        return 4500.0f + 3500.0f * std::sin(2.0f * dsp::kPi * 0.5f * t);
    };

    {   // 以前のバージョン: 毎サンプル係数を再計算
        dsp::SvfTPT f; f.SetSampleRate(kSampleRate); f.Reset();
        const double ns = MeasureNsPerSample([&]{
            float a = 0;
            for (int i = 0; i < kNumSamples; ++i) {
                f.SetCutoff(cutoffAt(i));
                a += f.Process(in[i]);
            }
            return a; });
        results.push_back({"SVF modulated: per-sample tan()", ns});
    }
    {   // 最適化版: ブロックごとに1回だけ再計算
        dsp::SvfTPT f; f.SetSampleRate(kSampleRate); f.Reset();
        const double ns = MeasureNsPerSample([&]{
            float a = 0;
            for (int i = 0; i < kNumSamples; ++i) {
                if ((i % kBlockSize) == 0) f.SetCutoff(cutoffAt(i));
                a += f.Process(in[i]);
            }
            return a; });
        results.push_back({"SVF modulated: per-block (N=64)", ns});
    }

    // 出力
    for (auto& r : results) std::printf("%-34s %12.3f\n", r.first, r.second);

    std::ofstream csv("bench_results.csv");
    csv << "case,ns_per_sample\n";
    for (auto& r : results) csv << r.first << ',' << r.second << '\n';
    std::printf("\nwrote bench_results.csv  (sink=%f)\n", g_sink);
    return 0;
}

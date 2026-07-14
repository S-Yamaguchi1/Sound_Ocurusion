#include "OcclusionSourceEffect.h"
#include "OcclusionRegistry.h"

namespace
{
    // 単一音源のデモ用。ゲームスレッド側と共通のキー。
    //
    constexpr uint64 kDemoSourceKey = 0;

    // フィルタ係数の再計算を毎サンプルではなくブロック単位で行う。
    // SetCutoff() 内の tan() が変調時のコスト
    // 約36 -> 約7 ns/sample、。カットオフは平滑化によりゆっくり動くため、
    // ブロック単位の更新でも聴感上の差は生じないように感じる。
}

void FOcclusionSourceEffect::Init(const FSoundEffectSourceInitData& InitData)
{
    SampleRate  = InitData.SampleRate;
    NumChannels = InitData.NumSourceChannels;

    Filters.SetNum(NumChannels);
    for (dsp::SvfTPT& Filter : Filters)
    {
        Filter.SetSampleRate(SampleRate);
        Filter.Reset();
    }
}

void FOcclusionSourceEffect::OnPresetChanged()
{
    GET_EFFECT_SETTINGS(OcclusionSourceEffect);

    SmoothingSeconds = FMath::Max(Settings.SmoothingTimeMs, 0.0f) * 0.001f;

    TargetCutoff = CurrentCutoff = Settings.DefaultCutoffHz;
    TargetGain   = CurrentGain   = 1.0f;
}

void FOcclusionSourceEffect::ProcessAudio(const FSoundEffectSourceInputData& InData,
                                          float* OutAudioBufferData)
{
    // ゲームスレッドが算出した最新の目標値を取得
    FOcclusionParams Params;
    if (FOcclusionRegistry::Get().GetParams(kDemoSourceKey, Params))
    {
        TargetCutoff = Params.CutoffHz;
        TargetGain   = Params.LinearGain;
    }

    const float* In        = InData.InputSourceEffectBufferPtr;
    const int32  NumFrames = InData.NumSamples / NumChannels;

    const float SmoothA = (SmoothingSeconds > 0.0f)
        ? 1.0f - FMath::Exp(-1.0f / (SmoothingSeconds * SampleRate))
        : 1.0f;

    for (int32 Frame = 0; Frame < NumFrames; ++Frame)
    {
        // 目標値へなめらかに変更し、急激な変化によるノイズを防ぐ
        CurrentCutoff += SmoothA * (TargetCutoff - CurrentCutoff);
        CurrentGain   += SmoothA * (TargetGain   - CurrentGain);

        if ((Frame % kCoeffUpdateInterval) == 0)
        {
            for (dsp::SvfTPT& Filter : Filters)
            {
                Filter.SetCutoff(CurrentCutoff);
            }
        }

        for (int32 Channel = 0; Channel < NumChannels; ++Channel)
        {
            const int32 Index = Frame * NumChannels + Channel;
            OutAudioBufferData[Index] = Filters[Channel].Process(In[Index]) * CurrentGain;
        }
    }
}

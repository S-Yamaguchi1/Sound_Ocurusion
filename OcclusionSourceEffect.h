// ローパスを音源に適用するカスタム Source Effect。
// フィルタのカットオフとゲインはゲームスレッドが算出した遮蔽パラメータでうごく。
#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundEffectSource.h"
#include "Filters.h"
#include "OcclusionSourceEffect.generated.h"

USTRUCT(BlueprintType)
struct FOcclusionSourceEffectSettings
{
    GENERATED_BODY()

    // 遮蔽データが到達する前に使うカットオフ
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion",
              meta = (ClampMin = "50.0", ClampMax = "20000.0"))
    float DefaultCutoffHz = 20000.0f;

    // カットオフとゲイン変化の平滑化時定数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion",
              meta = (ClampMin = "0.0", ClampMax = "500.0"))
    float SmoothingTimeMs = 50.0f;
};

// オーディオレンダースレッド上で動作
class FOcclusionSourceEffect : public FSoundEffectSource
{
public:
    virtual void Init(const FSoundEffectSourceInitData& InitData) override;
    virtual void OnPresetChanged() override;
    virtual void ProcessAudio(const FSoundEffectSourceInputData& InData,
                              float* OutAudioBufferData) override;

private:
    float SampleRate  = 48000.0f;
    int32 NumChannels = 1;

    float SmoothingSeconds = 0.05f;

    float TargetCutoff  = 20000.0f;
    float CurrentCutoff = 20000.0f;
    float TargetGain    = 1.0f;
    float CurrentGain   = 1.0f;

    TArray<dsp::SvfTPT> Filters; // チャンネルごとに1つ設定する
};

UCLASS()
class SOUNDOCCLUSION_API UOcclusionSourceEffectPreset : public USoundEffectSourcePreset
{
    GENERATED_BODY()

public:
    EFFECT_PRESET_METHODS(OcclusionSourceEffect)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffectPreset")
    FOcclusionSourceEffectSettings Settings;
};

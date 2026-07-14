#include "OccludedSoundSource.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "OcclusionRegistry.h"
#include "AcousticPhysicalMaterial.h"

namespace
{
    constexpr float  kMaxCutoffHz      = 20000.0f; // 遮蔽なし
    constexpr float  kMinCutoffHz      = 150.0f;   // どれだけ厚くてもこれ以下にしない
    constexpr float  kThicknessScaleCm = 40.0f;    // カットオフが 1/e になる実効厚み
    constexpr float  kGainDbPerCm      = 0.15f;    // 実効厚み1cmあたりの減衰量
    constexpr float  kMinGainDb        = -24.0f;
    constexpr uint64 kDemoSourceKey    = 0;        // Source Effect 側と共通のキー
}

// 音源→リスナーの直線が通過する固体の「実効厚み」を測定する。
//
// 順方向のマルチトレースで各壁の「入口」、逆方向のマルチトレースで「出口」が得られる。
// 双方を距離順に並べて対応づけると、各壁を貫通した区間長が求まる。
// 各区間にその面の音響密度を掛けることで、同じ50cmでもコンクリートはカーテンより
// はるかに強く遮蔽される、という材質差を表現する。
//
// OutRawCm には材質を考慮しない幾何的な厚みを返す（デバッグ表示用）。
static float ComputeEffectiveThicknessCm(UWorld* World, const FVector& Start, const FVector& End,
                                         const FCollisionQueryParams& Query, float& OutRawCm)
{
    TArray<FHitResult> Forward, Backward;
    World->LineTraceMultiByChannel(Forward,  Start, End, ECC_Visibility, Query);
    World->LineTraceMultiByChannel(Backward, End, Start, ECC_Visibility, Query);

    struct FEntry { float Dist; float Density; };
    TArray<FEntry> Entries;
    TArray<float>  Exits;

    for (const FHitResult& Hit : Forward)
    {
        float Density = 1.0f; // 音響マテリアル未設定の面は中立(1.0)として扱う
        if (const UAcousticPhysicalMaterial* Material =
                Cast<UAcousticPhysicalMaterial>(Hit.PhysMaterial.Get()))
        {
            Density = Material->AcousticDensity;
        }
        Entries.Add({ (float)FVector::Dist(Start, Hit.ImpactPoint), Density });
    }
    for (const FHitResult& Hit : Backward)
    {
        Exits.Add((float)FVector::Dist(Start, Hit.ImpactPoint));
    }

    Entries.Sort([](const FEntry& A, const FEntry& B) { return A.Dist < B.Dist; });
    Exits.Sort();

    float RawCm = 0.0f;
    float EffectiveCm = 0.0f;
    const int32 NumWalls = FMath::Min(Entries.Num(), Exits.Num());
    for (int32 i = 0; i < NumWalls; ++i)
    {
        const float Span = FMath::Max(0.0f, Exits[i] - Entries[i].Dist);
        RawCm       += Span;
        EffectiveCm += Span * Entries[i].Density;
    }

    OutRawCm = RawCm;
    return EffectiveCm;
}

AOccludedSoundSource::AOccludedSoundSource()
{
    PrimaryActorTick.bCanEverTick = true;

    AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
    RootComponent = AudioComponent;
    AudioComponent->bAutoActivate = true;
}

void AOccludedSoundSource::BeginPlay()
{
    Super::BeginPlay();
}

void AOccludedSoundSource::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UWorld* World = GetWorld();
    if (!World || !AudioComponent)
    {
        return;
    }

    APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (!CameraManager)
    {
        return;
    }

    const FVector ListenerLocation = CameraManager->GetCameraLocation();
    const FVector SourceLocation   = AudioComponent->GetComponentLocation();

    FCollisionQueryParams Query;
    Query.AddIgnoredActor(this);
    Query.bReturnPhysicalMaterial = true; // 音響マテリアルの取得に必須
    if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
    {
        Query.AddIgnoredActor(PlayerPawn);
    }

    float RawCm = 0.0f;
    const float EffectiveCm =
        ComputeEffectiveThicknessCm(World, SourceLocation, ListenerLocation, Query, RawCm);

    // 実効厚みからフィルタパラメータへ写像する（簡略化した質量則）。
    // カットオフは厚みに対して指数的に低下し、ゲインはdBで線形に減衰する。
    FOcclusionParams Params;
    Params.CutoffHz = FMath::Clamp(
        kMaxCutoffHz * FMath::Exp(-EffectiveCm / kThicknessScaleCm),
        kMinCutoffHz, kMaxCutoffHz);

    const float GainDb = FMath::Max(-EffectiveCm * kGainDbPerCm, kMinGainDb);
    Params.LinearGain = FMath::Pow(10.0f, GainDb / 20.0f);

    // オーディオレンダースレッドへ受け渡す
    FOcclusionRegistry::Get().SetParams(kDemoSourceKey, Params);

    if (bShowDebug)
    {
        const bool bOccluded = RawCm > 0.0f;
        DrawDebugLine(World, SourceLocation, ListenerLocation,
                      bOccluded ? FColor::Red : FColor::Green, false, -1.0f, 0, 2.0f);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow,
                FString::Printf(TEXT("Raw: %.0f cm  Eff: %.0f cm  ->  Cutoff: %.0f Hz   Gain: %.1f dB"),
                                RawCm, EffectiveCm, Params.CutoffHz, GainDb));
        }
    }
}

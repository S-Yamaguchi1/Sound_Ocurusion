// 自身とリスナーの間の遮蔽を測定する音源アクター。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OccludedSoundSource.generated.h"

UCLASS()
class SOUNDOCCLUSION_API AOccludedSoundSource : public AActor
{
    GENERATED_BODY()

public:
    AOccludedSoundSource();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
    class UAudioComponent* AudioComponent;

    // 音源→リスナーのトレース線と、算出値のオンスクリーン表示を有効にする
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion|Debug")
    bool bShowDebug = true;

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;
};

// 音響特性を持たせた Physical Material。
// 壁に割り当てることで、その面の透過特性を遮蔽計算に反映できる。
#pragma once

#include "CoreMinimal.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "AcousticPhysicalMaterial.generated.h"

UCLASS()
class SOUNDOCCLUSION_API UAcousticPhysicalMaterial : public UPhysicalMaterial
{
    GENERATED_BODY()

public:
    // 幾何的な厚みに掛ける重み。大きいほど強く遮蔽する。
    // 目安: カーテン 0.3 / 木材 1.0 / コンクリート 3.0 / 金属 4.0
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics",
              meta = (ClampMin = "0.0"))
    float AcousticDensity = 1.0f;
};

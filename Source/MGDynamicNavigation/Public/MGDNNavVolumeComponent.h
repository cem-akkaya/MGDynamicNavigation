// MG Dynamic Navigation plugin Created by Cem Akkaya licensed under MIT.
#pragma once
#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "MGDNNavVolumeComponent.generated.h"

class UMGDNNavDataAsset;
class UMGDNRuntimeNavMesh;

UCLASS(ClassGroup=(Navigation), meta=(BlueprintSpawnableComponent))
class MGDYNAMICNAVIGATION_API UMGDNNavVolumeComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UMGDNNavVolumeComponent();

	UPROPERTY(EditAnywhere, Category="MGDN")
	UMGDNNavDataAsset* SourceAsset = nullptr;

	UPROPERTY(Transient)
	UMGDNRuntimeNavMesh* RuntimeNav = nullptr;

	UPROPERTY(EditAnywhere, Category="MGDN|Grid")
	float CellSize = 100.f;

	UPROPERTY(EditAnywhere, Category="MGDN|Grid")
	float CellHeight = 100.f;

	UFUNCTION(CallInEditor, Category="MGDN")
	void BakeNow();

	UFUNCTION(CallInEditor, Category="MGDN")
	void VisualizeGrid();

protected:
	virtual void OnRegister() override;

private:
};

// MG Dynamic Navigation plugin Created by Cem Akkaya licensed under MIT.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MGDNNavVolumeActor.generated.h"

class UMGDNNavVolumeComponent;

UCLASS()
class MGDYNAMICNAVIGATION_API AMGDNNavVolumeActor : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere) UMGDNNavVolumeComponent* VolumeComp;

	AMGDNNavVolumeActor();
};

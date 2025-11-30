// MG Dynamic Navigation plugin Created by Cem Akkaya licensed under MIT.
#include "MGDNNavVolumeActor.h"
#include "MGDNNavVolumeComponent.h"

AMGDNNavVolumeActor::AMGDNNavVolumeActor()
{
	VolumeComp = CreateDefaultSubobject<UMGDNNavVolumeComponent>("MGDNVolume");
	RootComponent = VolumeComp;
}

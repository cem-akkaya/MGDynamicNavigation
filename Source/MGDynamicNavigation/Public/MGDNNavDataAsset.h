// MG Dynamic Navigation plugin Created by Cem Akkaya licensed under MIT.
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MGDNNavDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FMGDNGridNode
{
	GENERATED_BODY()

	// Walkable line trace result
	UPROPERTY(EditAnywhere)
	bool bWalkable = false;

	// Z height of the hit result
	UPROPERTY(EditAnywhere)
	float Height = 0.f;

	UPROPERTY(EditAnywhere)
	bool bIsRamp = false;

	// A*
	int32 Index  = -1;
	float G = 0.f;
	float H = 0.f;
	int32 Parent = -1;

	float F() const { return G + H; }
};


UCLASS(BlueprintType)
class MGDYNAMICNAVIGATION_API UMGDNNavDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MGDN")
	int32 GridX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MGDN")
	int32 GridY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MGDN")
	int32 GridZ = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MGDN")
	float CellSize = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MGDN")
	float CellHeight = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MGDN")
	FVector HalfSize = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MGDN")
	TArray<FMGDNGridNode> Nodes; // size = GridX*GridY*GridZ

	FORCEINLINE int32 Index(int32 X, int32 Y, int32 Z) const
	{
		return X + Y * GridX + Z * (GridX * GridY);
	}

	FORCEINLINE bool IsValid(int32 X, int32 Y, int32 Z) const
	{
		return X >= 0 && X < GridX &&
			   Y >= 0 && Y < GridY &&
			   Z >= 0 && Z < GridZ;
	}
};

// MG Dynamic Navigation plugin Created by Cem Akkaya licensed under MIT.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MGDNRuntimeNavMesh.generated.h"

class UMGDNNavDataAsset;

UCLASS()
class MGDYNAMICNAVIGATION_API UMGDNRuntimeNavMesh : public UObject
{
	GENERATED_BODY()

public:

	int32 GridX = 0;
	int32 GridY = 0;
	int32 GridZ = 0;

	float CellSize = 100.f;
	float CellHeight = 100.f;
	FVector HalfSize = FVector::ZeroVector;

	TArray<uint8> Walkable;

	bool BuildFromAsset(const UMGDNNavDataAsset* Asset);

	bool FindPath(
		const FTransform& PlatformTransform,
		const FVector& StartWorld,
		const FVector& EndWorld,
		TArray<FVector>& OutWorldPath
	) const;

private:

	FORCEINLINE bool IsValid(int32 X, int32 Y, int32 Z) const
	{
		return X >= 0 && X < GridX &&
			   Y >= 0 && Y < GridY &&
			   Z >= 0 && Z < GridZ;
	}

	FORCEINLINE int32 ToIndex(int32 X, int32 Y, int32 Z) const
	{
		return X + Y * GridX + Z * (GridX * GridY);
	}

	FORCEINLINE void ToXYZ(int32 Index, int32& X, int32& Y, int32& Z) const
	{
		const int32 XYCount = GridX * GridY;

		Z = Index / XYCount;

		const int32 Remainder = Index % XYCount;

		Y = Remainder / GridX;

		X = Remainder % GridX;
	}
	
	int32 FindNearestWalkable(int32 SX, int32 SY, int32 SZ, int32 SearchRadius = 2) const
	{
		for (int32 R = 0; R <= SearchRadius; R++)
		{
			for (int32 DX = -R; DX <= R; DX++)
				for (int32 DY = -R; DY <= R; DY++)
					for (int32 DZ = -R; DZ <= R; DZ++)
					{
						const int32 NX = SX + DX;
						const int32 NY = SY + DY;
						const int32 NZ = SZ + DZ;

						if (!IsValid(NX, NY, NZ))
							continue;

						const int32 NI = ToIndex(NX, NY, NZ);
						if (Walkable.IsValidIndex(NI) && Walkable[NI])
							return NI;
					}
		}

		return -1; // none found
	}

	//Find Neighbours
	void AddNeighbors6(int32 Index, TArray<int32>& Out) const;
	void AddNeighbors26(int32 Index, TArray<int32>& Out) const;

	bool AStar(int32 Start, int32 End, TArray<int32>& OutIndices) const;
};

// MG Dynamic Navigation plugin Created by Cem Akkaya licensed under MIT.
#include "MGDNRuntimeNavMesh.h"
#include "MGDNNavDataAsset.h"
#include "Algo/Reverse.h"

bool UMGDNRuntimeNavMesh::BuildFromAsset(const UMGDNNavDataAsset* Asset)
{
	if (!Asset)
	{
		UE_LOG(LogTemp, Error, TEXT("[MGDN] BuildFromAsset: Asset is null"));
		return false;
	}

	if (Asset->GridX <= 0 || Asset->GridY <= 0 || Asset->GridZ <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[MGDN] BuildFromAsset: Invalid grid size %d x %d x %d"),
			Asset->GridX, Asset->GridY, Asset->GridZ);
		return false;
	}

	const int32 Num = Asset->GridX * Asset->GridY * Asset->GridZ;
	if (Asset->Nodes.Num() != Num)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[MGDN] BuildFromAsset: Node count mismatch Nodes=%d Expected=%d"),
			Asset->Nodes.Num(), Num);
		return false;
	}

	GridX      = Asset->GridX;
	GridY      = Asset->GridY;
	GridZ      = Asset->GridZ;
	CellSize   = Asset->CellSize;
	CellHeight = Asset->CellHeight;
	HalfSize   = Asset->HalfSize;

	Walkable.SetNum(Num);
	for (int32 i = 0; i < Num; ++i)
	{
		Walkable[i] = Asset->Nodes[i].bWalkable ? 1 : 0;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[MGDN] RuntimeNav BuildFromAsset OK Grid=%dx%dx%d Cell=%.1f Height=%.1f Walkable=%d"),
		GridX, GridY, GridZ, CellSize, CellHeight, Num);

	return true;
}

void UMGDNRuntimeNavMesh::AddNeighbors6(int32 Index, TArray<int32>& Out) const
{
	Out.Reset();

	int32 X, Y, Z;
	ToXYZ(Index, X, Y, Z);

	// 6-axis neighbors: +/-X, +/-Y, +/-Z
	static const int32 DX[6] = { 1, -1,  0,  0,  0,  0 };
	static const int32 DY[6] = { 0,  0,  1, -1,  0,  0 };
	static const int32 DZ[6] = { 0,  0,  0,  0,  1, -1 };

	for (int32 i = 0; i < 6; ++i)
	{
		const int32 NX = X + DX[i];
		const int32 NY = Y + DY[i];
		const int32 NZ = Z + DZ[i];

		if (!IsValid(NX, NY, NZ))
			continue;

		const int32 NIndex = ToIndex(NX, NY, NZ);
		if (!Walkable.IsValidIndex(NIndex))
			continue;

		if (Walkable[NIndex])
		{
			Out.Add(NIndex);
		}
	}
}

void UMGDNRuntimeNavMesh::AddNeighbors26(int32 Index, TArray<int32>& Out) const
{
	Out.Reset();

	int32 X, Y, Z;
	ToXYZ(Index, X, Y, Z);

	for (int32 dZ = -1; dZ <= 1; dZ++)
		for (int32 dY = -1; dY <= 1; dY++)
			for (int32 dX = -1; dX <= 1; dX++)
			{
				if (dX == 0 && dY == 0 && dZ == 0)
					continue;

				int32 NX = X + dX;
				int32 NY = Y + dY;
				int32 NZ = Z + dZ;

				if (!IsValid(NX, NY, NZ))
					continue;

				int32 NIndex = ToIndex(NX, NY, NZ);
				if (Walkable[NIndex])
					Out.Add(NIndex);
			}
}

bool UMGDNRuntimeNavMesh::AStar(int32 Start, int32 End, TArray<int32>& OutIndices) const
{
	OutIndices.Reset();

	if (Start == End)
	{
		OutIndices.Add(Start);
		return true;
	}

	const int32 Num = GridX * GridY * GridZ;
	if (Num <= 0)
		return false;

	struct FNode
	{
		float G = FLT_MAX;
		float H = 0.f;
		int32 Parent = -1;
		bool bOpen   = false;
		bool bClosed = false;
	};

	TArray<FNode> Nodes;
	Nodes.SetNum(Num);

	auto Heuristic = [this](int32 A, int32 B) -> float
	{
		int32 AX, AY, AZ;
		int32 BX, BY, BZ;

		ToXYZ(A, AX, AY, AZ);
		ToXYZ(B, BX, BY, BZ);

		return float(
			FMath::Abs(AX - BX) +
			FMath::Abs(AY - BY) +
			FMath::Abs(AZ - BZ)
		);
	};

	TArray<int32> OpenSet;
	OpenSet.Reserve(128);

	Nodes[Start].G      = 0.f;
	Nodes[Start].H      = Heuristic(Start, End);
	Nodes[Start].Parent = -1;
	Nodes[Start].bOpen  = true;

	OpenSet.Add(Start);

	TArray<int32> Neighbors;
	Neighbors.Reserve(6);

	while (OpenSet.Num() > 0)
	{
		// find best F = G + H
		int32 BestIndex = 0;
		float BestF     = FLT_MAX;

		for (int32 i = 0; i < OpenSet.Num(); ++i)
		{
			const int32 Id = OpenSet[i];
			const FNode& N = Nodes[Id];
			const float F  = N.G + N.H;

			if (F < BestF)
			{
				BestF = F;
				BestIndex = i;
			}
		}

		const int32 Current = OpenSet[BestIndex];
		OpenSet.RemoveAtSwap(BestIndex);

		FNode& CurNode = Nodes[Current];
		CurNode.bOpen   = false;
		CurNode.bClosed = true;

		if (Current == End)
		{
			int32 Trace = End;
			while (Trace != -1)
			{
				OutIndices.Add(Trace);
				Trace = Nodes[Trace].Parent;
			}
			Algo::Reverse(OutIndices);
			return true;
		}

		AddNeighbors26(Current, Neighbors);

		for (int32 NIndex : Neighbors)
		{
			FNode& N = Nodes[NIndex];

			if (N.bClosed)
				continue;

			const float NewG = CurNode.G + 1.f;

			if (!N.bOpen || NewG < N.G)
			{
				N.G      = NewG;
				N.H      = Heuristic(NIndex, End);
				N.Parent = Current;

				if (!N.bOpen)
				{
					N.bOpen = true;
					OpenSet.Add(NIndex);
				}
			}
		}
	}

	return false;
}

bool UMGDNRuntimeNavMesh::FindPath(
	const FTransform& PlatformTransform,
	const FVector& StartWorld,
	const FVector& EndWorld,
	TArray<FVector>& OutWorldPath
) const
{
	OutWorldPath.Reset();

	if (GridX <= 0 || GridY <= 0 || GridZ <= 0 || CellSize <= 0.f || CellHeight <= 0.f)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[MGDN] FindPath: Invalid grid GX=%d GY=%d GZ=%d Cell=%.1f Height=%.1f"),
			GridX, GridY, GridZ, CellSize, CellHeight);
		return false;
	}

	const int32 Num = GridX * GridY * GridZ;
	if (Walkable.Num() != Num)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[MGDN] FindPath: Walkable size mismatch Walk=%d Expected=%d"),
			Walkable.Num(), Num);
		return false;
	}

	const FVector StartLocal = PlatformTransform.InverseTransformPosition(StartWorld);
	const FVector EndLocal   = PlatformTransform.InverseTransformPosition(EndWorld);

	auto MapToGrid = [this](const FVector& P, int32& GX, int32& GY, int32& GZ)
	{
		GX = FMath::Clamp(int32((P.X + HalfSize.X) / CellSize),   0, GridX - 1);
		GY = FMath::Clamp(int32((P.Y + HalfSize.Y) / CellSize),   0, GridY - 1);
		GZ = FMath::Clamp(int32((P.Z + HalfSize.Z) / CellHeight), 0, GridZ - 1);
	};

	int32 SX, SY, SZ;
	int32 EX, EY, EZ;

	MapToGrid(StartLocal, SX, SY, SZ);
	MapToGrid(EndLocal,   EX, EY, EZ);

	if (!IsValid(SX, SY, SZ) || !IsValid(EX, EY, EZ))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MGDN] FindPath: Start or End outside grid. Start=(%d,%d,%d) End=(%d,%d,%d)"),
			SX, SY, SZ, EX, EY, EZ);
		return false;
	}

	 int32 StartIndex = ToIndex(SX, SY, SZ);
	 int32 EndIndex   = ToIndex(EX, EY, EZ);

	if (!Walkable.IsValidIndex(StartIndex) || !Walkable[StartIndex])
	{
		const int32 Fallback = FindNearestWalkable(SX, SY, SZ, 2);

		if (Fallback < 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[MGDN] FindPath: Start cell not walkable & no fallback (%d,%d,%d) Id=%d"),
				SX, SY, SZ, StartIndex);
			return false;
		}

		UE_LOG(LogTemp, Display,
			TEXT("[MGDN] FindPath: Start fallback used → %d"), Fallback);

		StartIndex = Fallback;
	}

	// --- END CELL VALIDATION WITH FALLBACK ---
	if (!Walkable.IsValidIndex(EndIndex) || !Walkable[EndIndex])
	{
		const int32 FallbackEnd = FindNearestWalkable(EX, EY, EZ, 2);

		if (FallbackEnd < 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[MGDN] FindPath: End cell not walkable & no fallback (%d,%d,%d) Id=%d"),
				EX, EY, EZ, EndIndex);
			return false;
		}

		UE_LOG(LogTemp, Display,
			TEXT("[MGDN] FindPath: End fallback used → %d"), FallbackEnd);

		EndIndex = FallbackEnd;
	}


	UE_LOG(LogTemp, Verbose,
		TEXT("[MGDN] FindPath: Running A* Start=%d End=%d"), StartIndex, EndIndex);

	TArray<int32> IndexPath;
	if (!AStar(StartIndex, EndIndex, IndexPath) || IndexPath.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MGDN] FindPath: A* failed to find path"));
		return false;
	}

	// Convert indices back to world points (voxel centers)
	const float BaseX = -HalfSize.X + CellSize   * 0.5f;
	const float BaseY = -HalfSize.Y + CellSize   * 0.5f;
	const float BaseZ = -HalfSize.Z + CellHeight * 0.5f;

	OutWorldPath.Reserve(IndexPath.Num());

	for (int32 Id : IndexPath)
	{
		int32 GX, GY, GZ;
		ToXYZ(Id, GX, GY, GZ);

		const float LX = BaseX + GX * CellSize;
		const float LY = BaseY + GY * CellSize;
		const float LZ = BaseZ + GZ * CellHeight;

		const FVector Local(LX, LY, LZ);
		const FVector World = PlatformTransform.TransformPosition(Local);

		OutWorldPath.Add(World);
	}

	return OutWorldPath.Num() > 0;
}

// MG Dynamic Navigation plugin Created by Cem Akkaya licensed under MIT.
#include "MGDNNavVolumeComponent.h"
#include "MGDNNavDataAsset.h"
#include "MGDNRuntimeNavMesh.h"
#include "MGDynamicNavigationSubsystem.h"

#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "NavMesh/RecastNavMesh.h"

UMGDNNavVolumeComponent::UMGDNNavVolumeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Ignore);
}

void UMGDNNavVolumeComponent::OnRegister()
{
	Super::OnRegister();

	if (GetUnscaledBoxExtent().IsZero())
		SetBoxExtent(FVector(500,500,300));

	if (SourceAsset)
	{
		if (!RuntimeNav)
			RuntimeNav = NewObject<UMGDNRuntimeNavMesh>(this, UMGDNRuntimeNavMesh::StaticClass(), NAME_None, RF_Transient);

		RuntimeNav->BuildFromAsset(SourceAsset);
	}

	if (UWorld* World = GetWorld())
	{
		if (auto* S = World->GetSubsystem<UMGDynamicNavigationSubsystem>())
		{
			S->RegisterVolume(this);
		}
	}
}

void UMGDNNavVolumeComponent::OnUnregister()
{
	if (UWorld* W = GetWorld())
	{
		if (auto* S = W->GetSubsystem<UMGDynamicNavigationSubsystem>())
		{
			S->DeregisterVolume(this);
		}
	}

	Super::OnUnregister();
}

void UMGDNNavVolumeComponent::BakeNow()
{
#if WITH_EDITOR
    if (!SourceAsset) return;

    AActor* Owner = GetOwner();
    if (!Owner) return;

    UWorld* W = GetWorld();
    if (!W) return;

    const FTransform T = Owner->GetActorTransform();

    // Local bounds get
    FBox WorldBox = CalcBounds(GetComponentTransform()).GetBox();
    FBox LocalBox = WorldBox.TransformBy(T.Inverse());

    const FVector Min = LocalBox.Min;
    const FVector Max = LocalBox.Max;
    const FVector Extent = (Max - Min) * 0.5f;

    float CW = (CellSize   > 1.f) ? CellSize   : 100.f;
    float CH = (CellHeight > 1.f) ? CellHeight : 100.f;

    int32 GX = FMath::Max(1, int32((Max.X - Min.X) / CW));
    int32 GY = FMath::Max(1, int32((Max.Y - Min.Y) / CW));
    int32 GZ = FMath::Max(1, int32((Max.Z - Min.Z) / CH));

    SourceAsset->GridX = GX;
    SourceAsset->GridY = GY;
    SourceAsset->GridZ = GZ;

    SourceAsset->CellSize   = CW;
    SourceAsset->CellHeight = CH;

    SourceAsset->HalfSize = FVector(
        GX * CW * 0.5f,
        GY * CW * 0.5f,
        GZ * CH * 0.5f
    );

    const float BX = Min.X + CW * 0.5f;
    const float BY = Min.Y + CW * 0.5f;
    const float BZ = Min.Z + CH * 0.5f;

    const int32 Num = GX * GY * GZ;
    SourceAsset->Nodes.SetNum(Num);

    for (int32 Z=0; Z<GZ; Z++)
    for (int32 Y=0; Y<GY; Y++)
    for (int32 X=0; X<GX; X++)
    {
        int32 ID = SourceAsset->Index(X, Y, Z);
        auto& Node = SourceAsset->Nodes[ID];

        FVector Local(
            BX + X*CW,
            BY + Y*CW,
            BZ + Z*CH
        );

        FVector WP = T.TransformPosition(Local);

        FHitResult Hit;
        bool bHit = UKismetSystemLibrary::SphereTraceSingle(
            W,
            WP + FVector(0,0,50),
            WP - FVector(0,0,50),
            CellSize * 0.4f,
            ETraceTypeQuery::TraceTypeQuery1,
            false, {},
            EDrawDebugTrace::None,
            Hit,
            true
        );

        Node.bWalkable = bHit;

        if (Node.bWalkable)
        {
            FVector N = Hit.Normal;

            // Reject vertical
            if (N.Z < 0.6f)
            {
                Node.bWalkable = false;
            }
        }

        if (Node.bWalkable)
        {
            Node.Height = Hit.Location.Z;
        }

        // check if there is navmesh one more time
        if (Node.bWalkable)
        {
            FVector TestPoint = Hit.Location;

            UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(W);
            if (NavSys)
            {
                const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
                const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData);

                if (Recast)
                {
                    FNavLocation OutLoc;
                    const bool bNavHit = Recast->ProjectPoint(
                        TestPoint,
                        OutLoc,
                        FVector(50,50,200)
                    );

                    if (!bNavHit)
                    {
                        // NOT ON NAVMESH → reject this voxel
                        Node.bWalkable = false;
                    }
                }
            }
        }
    }

    SourceAsset->MarkPackageDirty();

    if (!RuntimeNav)
        RuntimeNav = NewObject<UMGDNRuntimeNavMesh>(this);

    RuntimeNav->BuildFromAsset(SourceAsset);

    UE_LOG(LogTemp, Warning,
        TEXT("[MGDN] BakeNow OK  Grid=%dx%dx%d  Cell=%.1f Height=%.1f"),
        GX, GY, GZ, CW, CH);

#endif
}


void UMGDNNavVolumeComponent::VisualizeGrid()
{
#if WITH_EDITOR
	if (!SourceAsset) return;
	if (SourceAsset->Nodes.Num() == 0) return;

	AActor* Owner = GetOwner();
	if (!Owner) return;

	UWorld* W = GetWorld();
	if (!W) return;

	const FTransform T = Owner->GetActorTransform();
	
	FBox WorldBox = CalcBounds(GetComponentTransform()).GetBox();
	FBox LocalBox = WorldBox.TransformBy(T.Inverse());
	const FVector Min = LocalBox.Min;

	const int32 GX = SourceAsset->GridX;
	const int32 GY = SourceAsset->GridY;
	const int32 GZ = SourceAsset->GridZ;

	const float CW = SourceAsset->CellSize;
	const float CH = SourceAsset->CellHeight;

	const float BX = Min.X + CW * 0.5f;
	const float BY = Min.Y + CW * 0.5f;
	const float BZ = Min.Z + CH * 0.5f;

	int32 Count = 0;

	for (int32 Z=0; Z<GZ; Z++)
		for (int32 Y=0; Y<GY; Y++)
			for (int32 X=0; X<GX; X++)
			{
				int32 ID = SourceAsset->Index(X,Y,Z);
				if (!SourceAsset->Nodes[ID].bWalkable)
					continue;

				FVector Local(
					BX + X*CW,
					BY + Y*CW,
					BZ + Z*CH 
				);

				FVector WP = T.TransformPosition(Local);

				DrawDebugSphere(W, WP, 20.f, 8, FColor::Green, false, 3.f, 0, 2.f);
				Count++;
			}

	UE_LOG(LogTemp, Warning,
		TEXT("[MGDN] VisualizeGrid drew %d walkable nodes"), Count);
#endif
}

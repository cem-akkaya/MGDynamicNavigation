// MG Dynamic Navigation plugin Created by Cem Akkaya licensed under MIT.
#include "MGDynamicNavigationSubsystem.h"
#include "MGDNNavVolumeComponent.h"
#include "MGDNRuntimeNavMesh.h"

#include "AIController.h"
#include "MGDNNavDataAsset.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/SplineComponent.h"
#include "GameFramework/PawnMovementComponent.h"

void UMGDynamicNavigationSubsystem::Tick(float DeltaTime)
{
    TickMGDN(DeltaTime);
}

void UMGDynamicNavigationSubsystem::RegisterVolume(UMGDNNavVolumeComponent* Volume)
{
    if (!Volume) return;

    AActor* Owner = Volume->GetOwner();
    if (!Owner) return;

    for (const FMGDNInstance& I : Instances)
    {
        if (I.VolumeComp && I.VolumeComp->GetOwner() == Owner)
            return;
    }

    for (const FMGDNInstance& I : Instances)
        if (I.VolumeComp == Volume)
            return;

    FMGDNInstance NewInst;
    NewInst.VolumeComp = Volume;
    NewInst.RuntimeNav = Volume->RuntimeNav;

    Instances.Add(NewInst);
}

void UMGDynamicNavigationSubsystem::DeregisterVolume(UMGDNNavVolumeComponent* Volume)
{
    if (!Volume) return;

    for (int32 i = Instances.Num() - 1; i >= 0; i--)
    {
        if (Instances[i].VolumeComp == Volume)
        {
            Instances.RemoveAt(i);
        }
    }
}

void UMGDynamicNavigationSubsystem::TickMGDN(float DeltaTime)
{
    for (int32 i = ActiveMoves.Num() - 1; i >= 0; i--)
    {
        FMGDNActiveMove& M = ActiveMoves[i];
        if (!M.Controller || !M.Spline) continue;

        APawn* Pawn = M.Controller->GetPawn();
        if (!Pawn) continue;

        UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Pawn->GetRootComponent());
        if (!Capsule) continue;

        // Disable pawn physics during movement
        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        auto CurrentColProfile = Capsule->GetCollisionProfileName();

        // Advance spline distance
        M.SplineDistance += M.MoveSpeed * DeltaTime;
        const float EndDist = M.Spline->GetSplineLength();

        // Target world location along spline
        FVector TargetPos = M.Spline->GetLocationAtDistanceAlongSpline(
            M.SplineDistance,
            ESplineCoordinateSpace::World
        );

        FVector PawnPos = Pawn->GetActorLocation();

        // Smooth movement (XY only)
        FVector DesiredXY = PawnPos;
        DesiredXY.X = TargetPos.X;
        DesiredXY.Y = TargetPos.Y;

        const float MoveInterpSpeed = 6.0f;

        FVector SmoothedPos = FMath::VInterpTo(
            PawnPos,
            DesiredXY,
            DeltaTime,
            MoveInterpSpeed
        );

        // Apply movement
        Pawn->SetActorLocation(SmoothedPos, false, nullptr, ETeleportType::None);

        // Restore Vel
        if (UMovementComponent* MoveComp = Pawn->GetMovementComponent())
        {
            FVector Tangent = M.Spline->GetTangentAtDistanceAlongSpline(
                M.SplineDistance, ESplineCoordinateSpace::World
            ).GetSafeNormal();

            MoveComp->Velocity = Tangent * M.MoveSpeed;
        }

        // Smooth rotation toward movement direction
        FVector MoveDir = (DesiredXY - PawnPos).GetSafeNormal2D();

        if (!MoveDir.IsNearlyZero())
        {
            FRotator TargetRot(0.f, MoveDir.Rotation().Yaw, 0.f);

            const float RotInterpSpeed = 8.0f;

            FRotator NewRot = FMath::RInterpTo(
                Pawn->GetActorRotation(),
                TargetRot,
                DeltaTime,
                RotInterpSpeed
            );

            Pawn->SetActorRotation(NewRot);
            M.Controller->SetControlRotation(NewRot);
        }

        // Arrival check
        if (M.SplineDistance >= EndDist - M.AcceptanceRadius)
        {
            Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Capsule->SetCollisionProfileName(CurrentColProfile);
            USplineComponent* S = M.Spline;
            M.Spline = nullptr;
            if (S) S->DestroyComponent();

            auto CB = M.Callback;
            ActiveMoves.RemoveAt(i);
            CB.ExecuteIfBound(EMGDNMoveResult::Success);
            // Restore Vel
            if (UMovementComponent* MoveComp = Pawn->GetMovementComponent())
            {
                MoveComp->Velocity = FVector::ZeroVector;
            }
        }
    }
}



static USplineComponent* CreateSplinePath(
    AActor* Platform,
    const FVector& PawnLocal,
    const TArray<FVector>& LocalPath
)
{
    USplineComponent* Spline = NewObject<USplineComponent>(Platform);
    Spline->RegisterComponent();
    Spline->AttachToComponent(Platform->GetRootComponent(),
        FAttachmentTransformRules::KeepRelativeTransform);

    Spline->ClearSplinePoints(false);

    Spline->AddPoint(FSplinePoint(0, PawnLocal, ESplinePointType::Linear));

    int32 I = 1;
    for (FVector P : LocalPath)
    {
        P.Z = PawnLocal.Z; 
        Spline->AddPoint(FSplinePoint(I++, P, ESplinePointType::Linear));
    }

    Spline->SetClosedLoop(false);
    Spline->UpdateSpline();

    return Spline;
}

void UMGDynamicNavigationSubsystem::MoveToLocationMGDNAsync(
    AAIController* Controller,
    const FVector& Goal,
    float AcceptanceRadius,
    float MoveSpeed,
    FMGDNMoveFinishedDynamicDelegate Callback)
{
    if (!Controller || !Controller->GetPawn())
    {
        Callback.ExecuteIfBound(EMGDNMoveResult::Failed_InvalidController);
        return;
    }

    APawn* Pawn = Controller->GetPawn();
    FVector PawnLoc = Pawn->GetActorLocation();

    AActor* Platform = nullptr;
    FMGDNInstance* Inst = nullptr;

    for (FMGDNInstance& I : Instances)
    {
        if (!I.VolumeComp || !I.VolumeComp->SourceAsset)
            continue;

        AActor* Owner = I.VolumeComp->GetOwner();
        if (!Owner) continue;

        FTransform T = Owner->GetActorTransform();
        FVector L = T.InverseTransformPosition(PawnLoc);
        FVector HS = I.VolumeComp->SourceAsset->HalfSize;

        if (L.X >= -HS.X && L.X <= HS.X &&
            L.Y >= -HS.Y && L.Y <= HS.Y &&
            L.Z >= -HS.Z && L.Z <= HS.Z)
        {
            Platform = Owner;
            Inst = &I;
            break;
        }
    }

    if (!Platform)
    {
        Callback.ExecuteIfBound(EMGDNMoveResult::Failed_NoPlatform);
        return;
    }

    if (!Inst || !Inst->RuntimeNav)
    {
        Callback.ExecuteIfBound(EMGDNMoveResult::Failed_RuntimeNavMissing);
        return;
    }

    const FTransform T = Platform->GetActorTransform();
    FVector PawnLocal = T.InverseTransformPosition(PawnLoc);
    FVector EndLocal  = T.InverseTransformPosition(Goal);

    TArray<FVector> WorldPath;
    if (!Inst->RuntimeNav->FindPath(T, PawnLoc, Goal, WorldPath))
    {
        Callback.ExecuteIfBound(EMGDNMoveResult::Failed_NoPath);
        return;
    }

    TArray<FVector> LocalPath;
    LocalPath.Reserve(WorldPath.Num());
    for (const FVector& W : WorldPath)
    {
        FVector P = T.InverseTransformPosition(W);
        P.Z = PawnLocal.Z;
        LocalPath.Add(P);
    }

    USplineComponent* Spline =
        CreateSplinePath(Platform, PawnLocal, LocalPath);

    if (!Spline)
    {
        Callback.ExecuteIfBound(EMGDNMoveResult::Failed_MoveRequest);
        return;
    }

    FMGDNActiveMove Move;
    Move.Controller = Controller;
    Move.Platform = Platform;
    Move.Goal = Goal;
    Move.LocalGoal = EndLocal;
    Move.Spline = Spline;

    Move.MoveSpeed = MoveSpeed;
    Move.SplineDistance = 0.f;
    Move.AcceptanceRadius = AcceptanceRadius;
    Move.Callback = Callback;

    ActiveMoves.Add(Move);
}

bool UMGDynamicNavigationSubsystem::IsValidGameWorld() const
{
    UWorld* W = GetWorld();
    if (!W) return false;

    return W->WorldType == EWorldType::Game
        || W->WorldType == EWorldType::PIE;
}

TArray<UMGDNNavVolumeComponent*> UMGDynamicNavigationSubsystem::GetAllNavigationVolumes() const
{
    if (!IsValidGameWorld())
        return {};

    TArray<UMGDNNavVolumeComponent*> Out;
    for (const FMGDNInstance& I : Instances)
        if (I.VolumeComp)
            Out.Add(I.VolumeComp);

    return Out;
}

bool UMGDynamicNavigationSubsystem::IsPawnOnShip(APawn* Pawn) const
{
    if (!Pawn) return false;

    const FVector P = Pawn->GetActorLocation();

    for (const FMGDNInstance& I : Instances)
    {
        if (!I.VolumeComp || !I.VolumeComp->SourceAsset) continue;

        AActor* Owner = I.VolumeComp->GetOwner();
        if (!Owner) continue;

        const FTransform T = Owner->GetActorTransform();
        const FVector L = T.InverseTransformPosition(P);

        const FVector HS = I.VolumeComp->SourceAsset->HalfSize;

        if (L.X >= -HS.X && L.X <= HS.X &&
            L.Y >= -HS.Y && L.Y <= HS.Y &&
            L.Z >= -HS.Z && L.Z <= HS.Z)
        {
            return true;
        }
    }

    return false;
}

bool UMGDynamicNavigationSubsystem::IsControllerOnShip(AAIController* Controller) const
{
    return (Controller && Controller->GetPawn()) ?
        IsPawnOnShip(Controller->GetPawn()) : false;
}

AActor* UMGDynamicNavigationSubsystem::GetPawnPlatform(APawn* Pawn) const
{
    if (!Pawn) return nullptr;

    const FVector P = Pawn->GetActorLocation();

    for (const FMGDNInstance& I : Instances)
    {
        if (!I.VolumeComp || !I.VolumeComp->SourceAsset) continue;

        AActor* Owner = I.VolumeComp->GetOwner();
        if (!Owner) continue;

        const FTransform T = Owner->GetActorTransform();
        const FVector L = T.InverseTransformPosition(P);
        const FVector HS = I.VolumeComp->SourceAsset->HalfSize;

        if (L.X >= -HS.X && L.X <= HS.X &&
            L.Y >= -HS.Y && L.Y <= HS.Y &&
            L.Z >= -HS.Z && L.Z <= HS.Z)
        {
            return Owner;
        }
    }

    return nullptr;
}
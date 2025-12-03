// MG Dynamic Navigation plugin Created by Cem Akkaya licensed under MIT.
#include "MGDynamicNavigationSubsystem.h"
#include "MGDNNavVolumeComponent.h"
#include "MGDNRuntimeNavMesh.h"

#include "AIController.h"
#include "EngineUtils.h"
#include "MGDNNavDataAsset.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/SplineComponent.h"
#include "Kismet/KismetSystemLibrary.h"
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

        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        auto CurrentColProfile = Capsule->GetCollisionProfileName();

        const FTransform PlatformTransform = M.Platform->GetActorTransform();
        
        // ---------------------------------------------------------------------
        // 0. Avoidance .. @Todo Freeze and Unfreeze pawn functions and refactor tick, freeze logic.
        // ---------------------------------------------------------------------
        
        InsertAvoidanceDetour(M, Pawn, Capsule, PlatformTransform);
        if (M.AvoidanceCooldown > 0.f)
        {
            // Check if still blocked
            bool bFrozen = HandleAvoidanceFreeze(M, Pawn, Capsule, PlatformTransform);

            if (bFrozen)
            {
                // Reduce cooldown timer
                M.AvoidanceCooldown -= DeltaTime;
                if (M.AvoidanceCooldown < 0.f) M.AvoidanceCooldown = 0.f;

                // While frozen, ground pawn and skip advance
                FVector P = Pawn->GetActorLocation();

                FVector Start = P + FVector(0,0,50);
                FVector End   = P - FVector(0,0,200);

                FHitResult Hit;
                FCollisionQueryParams Params;
                Params.AddIgnoredActor(Pawn);

                if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
                {
                    float HH = Capsule->GetScaledCapsuleHalfHeight();
                    P.Z = Hit.Location.Z + HH;
                    Pawn->SetActorLocation(P, false, nullptr, ETeleportType::None);
                }

                // Skip movement this tick
                continue;
            }
        }

        // ---------------------------------------------------------------------
        // 1. Advance spline
        // ---------------------------------------------------------------------
        M.SplineDistance += M.MoveSpeed * DeltaTime;
        const float EndDist = M.Spline->GetSplineLength();

        // ---------------------------------------------------------------------
        // 2. Query spline position
        // ---------------------------------------------------------------------
        FVector TargetPos = M.Spline->GetLocationAtDistanceAlongSpline(
            M.SplineDistance,
            ESplineCoordinateSpace::World
        );

        FVector PawnPos = Pawn->GetActorLocation();

        // ---------------------------------------------------------------------
        // 3. Smooth XY movement
        // ---------------------------------------------------------------------
        FVector DesiredXY = PawnPos;
        DesiredXY.X = TargetPos.X;
        DesiredXY.Y = TargetPos.Y;

        const float MoveInterpSpeed = 20.f;

        FVector SmoothedPos = FMath::VInterpTo(
            PawnPos,
            DesiredXY,
            DeltaTime,
            MoveInterpSpeed
        );

        Pawn->SetActorLocation(SmoothedPos, false, nullptr, ETeleportType::None);

        // ---------------------------------------------------------------------
        // 4. Velocity restore (optional)
        // ---------------------------------------------------------------------
        if (UMovementComponent* MoveComp = Pawn->GetMovementComponent())
        {
            FVector Tangent = M.Spline->GetTangentAtDistanceAlongSpline(
                M.SplineDistance,
                ESplineCoordinateSpace::World
            ).GetSafeNormal();

            MoveComp->Velocity = Tangent * M.MoveSpeed;
        }

        // ---------------------------------------------------------------------
        // 5. Grounding trace (correct final Z)
        // ---------------------------------------------------------------------
        {
            FVector P = Pawn->GetActorLocation();

            FVector Start = P + FVector(0,0,50);
            FVector End   = P - FVector(0,0,200);

            FHitResult Hit;
            FCollisionQueryParams Params;
            Params.AddIgnoredActor(Pawn);

            if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
            {
                float HH = Capsule->GetScaledCapsuleHalfHeight();
                P.Z = Hit.Location.Z + HH;
                Pawn->SetActorLocation(P, false, nullptr, ETeleportType::None);
            }
        }

        // ---------------------------------------------------------------------
        // 6. Rotation smoothing
        // ---------------------------------------------------------------------
        FVector MoveDir = (DesiredXY - PawnPos).GetSafeNormal2D();
        if (!MoveDir.IsNearlyZero())
        {
            FRotator TargetRot(0.f, MoveDir.Rotation().Yaw, 0.f);
            const float RotInterpSpeed = 8.f;

            FRotator NewRot = FMath::RInterpTo(
                Pawn->GetActorRotation(),
                TargetRot,
                DeltaTime,
                RotInterpSpeed
            );

            Pawn->SetActorRotation(NewRot);
            M.Controller->SetControlRotation(NewRot);
        }

        // ---------------------------------------------------------------------
        // 7. Arrival
        // ---------------------------------------------------------------------
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
    const TArray<FVector>& LocalPath,
    float SafeRadius)
{
    USplineComponent* Spline = NewObject<USplineComponent>(Platform);
    Spline->RegisterComponent();
    Spline->AttachToComponent(Platform->GetRootComponent(),
        FAttachmentTransformRules::KeepRelativeTransform);

    Spline->ClearSplinePoints(false);

    UMGDNNavVolumeComponent* Vol = Cast<UMGDNNavVolumeComponent>(
        Platform->GetComponentByClass(UMGDNNavVolumeComponent::StaticClass()));
    UMGDNNavDataAsset* Asset = (Vol ? Vol->SourceAsset : nullptr);

    FVector StartLocal = PawnLocal;

    if (Asset)
    {
        const FVector HS = Asset->HalfSize;

        StartLocal.X = FMath::Clamp(StartLocal.X, -HS.X + SafeRadius, HS.X - SafeRadius);
        StartLocal.Y = FMath::Clamp(StartLocal.Y, -HS.Y + SafeRadius, HS.Y - SafeRadius);

        float Z =
            UMGDynamicNavigationSubsystem::GetTrueSurfaceZ_Local(
                Asset,
                Platform->GetActorTransform(),
                StartLocal,
                Platform->GetWorld());

        StartLocal.Z = Z;
    }

    Spline->AddPoint(FSplinePoint(0, StartLocal, ESplinePointType::Linear));

    int32 I = 1;
    for (FVector P : LocalPath)
    {
        if (Asset)
        {
            const FVector HS = Asset->HalfSize;

            P.X = FMath::Clamp(P.X, -HS.X + SafeRadius, HS.X - SafeRadius);
            P.Y = FMath::Clamp(P.Y, -HS.Y + SafeRadius, HS.Y - SafeRadius);

            if (I <= 3)
            {
                float Z =
                    UMGDynamicNavigationSubsystem::GetTrueSurfaceZ_Local(
                        Asset,
                        Platform->GetActorTransform(),
                        P,
                        Platform->GetWorld());

                P.Z = Z;
            }
            else
            {
                P.Z = PawnLocal.Z;
            }
        }
        else
        {
            P.Z = PawnLocal.Z;
        }

        Spline->AddPoint(FSplinePoint(I++, P, ESplinePointType::Linear));
    }

    Spline->SetClosedLoop(false);
    Spline->UpdateSpline();

    return Spline;
}

bool UMGDynamicNavigationSubsystem::HandleAvoidanceFreeze(
    FMGDNActiveMove& M,
    APawn* Pawn,
    UCapsuleComponent* Capsule,
    const FTransform& PlatformTransform)
{
    UWorld* W = Pawn->GetWorld();
    if (!W) return false;

    FVector PawnPos = Pawn->GetActorLocation();
    FVector Forward = Pawn->GetActorForwardVector();

    const float Rad = Capsule->GetScaledCapsuleRadius();
    const float DetectDist = Rad * 2.0f;
    const float CheckRadius = Rad * 0.9f;

    TArray<AActor*> Ignore;
    Ignore.Add(Pawn);

    TArray<FHitResult> Hits;

    bool bHit = UKismetSystemLibrary::SphereTraceMulti(
        Pawn,
        PawnPos,
        PawnPos + Forward * DetectDist,
        CheckRadius,
        UEngineTypes::ConvertToTraceType(ECC_Camera),
        false,
        Ignore,
        EDrawDebugTrace::None,
        Hits,
        true
    );

    bool bStillBlocked = false;

    if (bHit)
    {
        for (const FHitResult& H : Hits)
        {
            APawn* OtherPawn = Cast<APawn>(H.GetActor());
            if (OtherPawn && OtherPawn != Pawn)
            {
                bStillBlocked = true;
                break;
            }
        }
    }

    if (!bStillBlocked)
    {
        M.FreezeTimer = 0.f;
        return false;
    }

    M.FreezeTimer += W->GetDeltaSeconds();

    if (M.FreezeTimer >= 1.f)
    {
        M.FreezeTimer = 0.f;
        return false;
    }

    return true;
}

bool UMGDynamicNavigationSubsystem::InsertAvoidanceDetour(
    FMGDNActiveMove& M,
    APawn* Pawn,
    UCapsuleComponent* Capsule,
    const FTransform& PlatformTransform)
{
    if (!Pawn || !Capsule || !M.Spline)
        return false;
    
    if (M.AvoidanceCooldown > 0.f)
    {
        if (HandleAvoidanceFreeze(M, Pawn, Capsule, PlatformTransform))
            return true;  // freezing
        return false; 
    }

    UWorld* W = Pawn->GetWorld();
    if (!W) return false;

    FVector PawnPos = Pawn->GetActorLocation();
    FVector Forward = Pawn->GetActorForwardVector();

    const float Rad = Capsule->GetScaledCapsuleRadius();
    const float DetectDist = Rad * 1.5f;

    const float CheckRadius = Rad * 0.8f;

    TArray<AActor*> Ignore;
    Ignore.Add(Pawn);

    TArray<FHitResult> Hits;

    bool bAnyHit = UKismetSystemLibrary::SphereTraceMulti(
        Pawn,
        PawnPos,
        PawnPos + Forward * DetectDist,
        CheckRadius,
        UEngineTypes::ConvertToTraceType(ECC_Camera),
        false,
        Ignore,
        EDrawDebugTrace::None,
        Hits,
        true,
        FLinearColor::Yellow,
        FLinearColor::Red,
        0.05f
    );

    if (!bAnyHit)
        return false;

    bool bHasValidPawnHit = false;
    for (const FHitResult& H : Hits)
    {
        APawn* OtherPawn = Cast<APawn>(H.GetActor());
        if (!OtherPawn || OtherPawn == Pawn)
            continue;

        UE_LOG(LogTemp, Display, TEXT("Avoidance detour hit %s"), *OtherPawn->GetName());
        bHasValidPawnHit = true;
        break;
    }

    if (!bHasValidPawnHit)
        return false;

    FVector Right = Pawn->GetActorRightVector();
    float Side = -1.f;

    float OffsetDist = Rad * 2.0f;
    FVector AvoidWorld = PawnPos + Right * Side * OffsetDist;

    FVector PawnLocal = PlatformTransform.InverseTransformPosition(PawnPos);
    FVector AvoidLocal = PlatformTransform.InverseTransformPosition(AvoidWorld);

    AvoidLocal.Z = PawnLocal.Z;

    AActor* Platform = GetPawnPlatform(Pawn);
    if (!Platform)
        return false;

    FMGDNInstance* Inst = nullptr;
    for (FMGDNInstance& I : Instances)
    {
        if (I.VolumeComp && I.VolumeComp->GetOwner() == Platform)
        {
            Inst = &I;
            break;
        }
    }

    if (!Inst || !Inst->RuntimeNav)
        return false;

    const FVector HSCheck = Inst->VolumeComp->SourceAsset->HalfSize;
    if (AvoidLocal.X < -HSCheck.X || AvoidLocal.X > HSCheck.X ||
        AvoidLocal.Y < -HSCheck.Y || AvoidLocal.Y > HSCheck.Y)
    {
        return false;
    }

    TArray<FVector> NewWorldPath;

    FVector AvoidWorldRecalc = PlatformTransform.TransformPosition(AvoidLocal);

    FVector GoalLocal = M.LocalGoal;
    GoalLocal.Z = PawnLocal.Z;

    if (GoalLocal.X < -HSCheck.X || GoalLocal.X > HSCheck.X ||
        GoalLocal.Y < -HSCheck.Y || GoalLocal.Y > HSCheck.Y)
    {
        return false;
    }

    FVector GoalWorld = PlatformTransform.TransformPosition(GoalLocal);

    if (!Inst->RuntimeNav->FindPath(
        PlatformTransform,
        AvoidWorldRecalc,
        GoalWorld,
        NewWorldPath))
    {
        return false;
    }

    TArray<FVector> NewLocal;
    NewLocal.Reserve(NewWorldPath.Num());

    for (const FVector& WP : NewWorldPath)
    {
        FVector P = PlatformTransform.InverseTransformPosition(WP);
        P.Z = PawnLocal.Z;
        NewLocal.Add(P);
    }

    TArray<FVector> FinalLocal;
    FinalLocal.Reserve(1 + NewLocal.Num());
    FinalLocal.Add(AvoidLocal);
    for (const FVector& P : NewLocal)
    {
        FinalLocal.Add(P);
    }

    USplineComponent* OldSpline = M.Spline;
    USplineComponent* NewSpline = CreateSplinePath(
        Platform,
        PawnLocal,
        FinalLocal,
        Rad * 0.25f
    );

    M.Spline = NewSpline;
    if (OldSpline)
        OldSpline->DestroyComponent();

    M.SplineDistance = 0.f;

    M.AvoidanceCooldown = 1.f;
    return true;
}

// This returns Z height local
float UMGDynamicNavigationSubsystem::GetSurfaceZ_Local(
    const UMGDNNavDataAsset* Asset,
    const FVector& Local)
{
    if (!Asset) return Local.Z;

    const FVector HS = Asset->HalfSize;

    int32 GX = FMath::Clamp(int32((Local.X + HS.X) / Asset->CellSize),   0, Asset->GridX - 1);
    int32 GY = FMath::Clamp(int32((Local.Y + HS.Y) / Asset->CellSize),   0, Asset->GridY - 1);
    int32 GZ = FMath::Clamp(int32((Local.Z + HS.Z) / Asset->CellHeight), 0, Asset->GridZ - 1);

    int32 Id = Asset->Index(GX, GY, GZ);
    if (!Asset->Nodes.IsValidIndex(Id))
        return Local.Z;

    return Asset->Nodes[Id].Height;
}

// This returns the point from data
FVector UMGDynamicNavigationSubsystem::GetSurfacePoint_Local(
    const UMGDNNavDataAsset* Asset,
    const FVector& Local)
{
    FVector P = Local;
    float Z = GetSurfaceZ_Local(Asset, Local);
    P.Z = Z;
    return P;
}

float UMGDynamicNavigationSubsystem::GetTrueSurfaceZ_Local(
    const UMGDNNavDataAsset* Asset,
    const FTransform& PlatformTransform,
    const FVector& Local,
    UWorld* World)
{
    FVector Up   = PlatformTransform.GetRotation().RotateVector(FVector::UpVector);
    FVector Down = -Up;

    FVector WorldPos = PlatformTransform.TransformPosition(Local);

    FVector Start = WorldPos + Up * 150.f;
    FVector End   = WorldPos + Down * 150.f;

    TArray<AActor*> Ignore;

    // Ignore ALL pawns
    for (TActorIterator<APawn> It(World); It; ++It)
    {
        Ignore.Add(*It);
    }

    FHitResult Hit;

    bool bHit = UKismetSystemLibrary::SphereTraceSingle(
        World,
        Start,
        End,
        5.f,
        UEngineTypes::ConvertToTraceType(ECC_Visibility),
        true,
        Ignore,
        EDrawDebugTrace::None,
        Hit,
        true
    );

    if (bHit)
    {
        FVector LocalHit = PlatformTransform.InverseTransformPosition(Hit.Location);
        return LocalHit.Z;
    }

    return GetSurfaceZ_Local(Asset, Local);
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
        FVector HSCheck = I.VolumeComp->SourceAsset->HalfSize;

        if (L.X >= -HSCheck.X && L.X <= HSCheck.X &&
            L.Y >= -HSCheck.Y && L.Y <= HSCheck.Y &&
            L.Z >= -HSCheck.Z && L.Z <= HSCheck.Z)
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

    // Get capsule sizes and make a fallback if no capsule which is an edge case
    float CapsuleR = 40.f;
    float CapsuleH = 88.f;

    if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Pawn->GetRootComponent()))
    {
        CapsuleR = Capsule->GetScaledCapsuleRadius();
        CapsuleH = Capsule->GetScaledCapsuleHalfHeight();
    }

    // A bit increase the radius
    const float SafeX = CapsuleR * 1.25f;
    const float SafeY = CapsuleR * 1.25f;

    const FTransform T = Platform->GetActorTransform();
    FVector PawnLocal = T.InverseTransformPosition(PawnLoc);
    FVector EndLocal  = T.InverseTransformPosition(Goal);

    // Cache HalfSize 
    const FVector HS = Inst->VolumeComp->SourceAsset->HalfSize;

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

        // Keep Z 
        P.Z = PawnLocal.Z;

        // Clamp path so spline not in edges
        P.X = FMath::Clamp(P.X, -HS.X + SafeX, HS.X - SafeX);
        P.Y = FMath::Clamp(P.Y, -HS.Y + SafeY, HS.Y - SafeY);

        LocalPath.Add(P);
    }

    USplineComponent* Spline =
        CreateSplinePath(Platform, PawnLocal, LocalPath, SafeX);

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
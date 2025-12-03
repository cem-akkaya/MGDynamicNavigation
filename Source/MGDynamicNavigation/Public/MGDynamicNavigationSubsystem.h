// MG Dynamic Navigation plugin Created by Cem Akkaya licensed under MIT.
#pragma once
#include "CoreMinimal.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/SplineComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "MGDynamicNavigationSubsystem.generated.h"

class AAIController;
class APawn;
class UMGDNNavVolumeComponent;
class UMGDNRuntimeNavMesh;

UENUM(BlueprintType)
enum class EMGDNMoveResult : uint8
{
    Success,
    AlreadyAtGoal,
    Failed_NoPlatform,
    Failed_NoPath,
    Failed_RuntimeNavMissing,
    Failed_InvalidController,
    Failed_MoveRequest,
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FMGDNMoveFinishedDynamicDelegate, EMGDNMoveResult, Result);


USTRUCT()
struct FMGDNInstance
{
    GENERATED_BODY()

    UPROPERTY() UMGDNNavVolumeComponent* VolumeComp = nullptr;  
    UPROPERTY() UMGDNRuntimeNavMesh* RuntimeNav = nullptr;      
};

USTRUCT()
struct FMGDNActiveMove
{
    GENERATED_BODY()

    UPROPERTY() AAIController* Controller = nullptr;
    UPROPERTY() AActor* Platform = nullptr;

    UPROPERTY() USplineComponent* Spline = nullptr;
    UPROPERTY() float SplineDistance = 0.f;
    UPROPERTY() float MoveSpeed = 400.f;

    UPROPERTY() FVector Goal = FVector::ZeroVector;      
    UPROPERTY() FVector LocalGoal = FVector::ZeroVector; 

    UPROPERTY() float AcceptanceRadius = 50.f;
    UPROPERTY() FMGDNMoveFinishedDynamicDelegate Callback;
};


UCLASS()
class MGDYNAMICNAVIGATION_API UMGDynamicNavigationSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:

    UPROPERTY() TArray<FMGDNInstance> Instances;
    UPROPERTY() TArray<FMGDNActiveMove> ActiveMoves;

    virtual void Tick(float DeltaTime) override;
    void RegisterVolume(UMGDNNavVolumeComponent* Volume);
    void DeregisterVolume(UMGDNNavVolumeComponent* Volume);
    
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMGDN_Tick, STATGROUP_Tickables); }

    virtual bool IsTickable() const override { return true; }

    void TickMGDN(float DeltaTime);

    UFUNCTION(BlueprintCallable)
    void MoveToLocationMGDNAsync(
        AAIController* Controller,
        const FVector& Goal,
        float AcceptanceRadius,
        float MoveSpeed, FMGDNMoveFinishedDynamicDelegate Callback);
};

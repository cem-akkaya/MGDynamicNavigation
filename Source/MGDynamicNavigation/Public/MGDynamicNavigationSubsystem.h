// MG Dynamic Navigation plugin Created by Cem Akkaya licensed under MIT.
#pragma once
#include "CoreMinimal.h"
#include "MGDNNavDataAsset.h"
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
    
    UPROPERTY() float AvoidanceCooldown = 0.f;
    UPROPERTY() float FreezeTimer = 0.f;
};


UCLASS()
class MGDYNAMICNAVIGATION_API UMGDynamicNavigationSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:

    UPROPERTY() TArray<FMGDNInstance> Instances;
    UPROPERTY() TArray<FMGDNActiveMove> ActiveMoves;
    
    // Helper functions for status queries
    
    /** Returns all registered MGDN volumes */
    UFUNCTION(BlueprintCallable, Category="MGDN")
    TArray<UMGDNNavVolumeComponent*> GetAllNavigationVolumes() const;

    /** Returns true if the pawn is currently inside a MGDN nav platform */
    UFUNCTION(BlueprintCallable, Category="MGDN")
    bool IsPawnOnShip(APawn* Pawn) const;

    /** Returns true if controller's pawn is inside a MGDN nav platform */
    UFUNCTION(BlueprintCallable, Category="MGDN")
    bool IsControllerOnShip(AAIController* Controller) const;

    /** Returns the platform actor the pawn is on (nullptr if none) */
    UFUNCTION(BlueprintCallable, Category="MGDN")
    AActor* GetPawnPlatform(APawn* Pawn) const;
    
    virtual void Tick(float DeltaTime) override;
    
    void RegisterVolume(UMGDNNavVolumeComponent* Volume);
    void DeregisterVolume(UMGDNNavVolumeComponent* Volume);
    
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMGDN_Tick, STATGROUP_Tickables); }

    virtual bool IsTickable() const override { return true; }

    void TickMGDN(float DeltaTime);

    bool HandleAvoidanceFreeze(FMGDNActiveMove& M, APawn* Pawn, UCapsuleComponent* Capsule,
                               const FTransform& PlatformTransform);
    
    bool InsertAvoidanceDetour(FMGDNActiveMove& M, APawn* Pawn, UCapsuleComponent* Capsule,
                               const FTransform& PlatformTransform);

    static float GetSurfaceZ_Local(const UMGDNNavDataAsset* Asset, const FVector& Local);
    
    static FVector GetSurfacePoint_Local(const UMGDNNavDataAsset* Asset, const FVector& Local);

    static float GetTrueSurfaceZ_Local(
        const UMGDNNavDataAsset* Asset,
        const FTransform& PlatformTransform,
        const FVector& Local,
        UWorld* World);

    UFUNCTION(BlueprintCallable)
    void MoveToLocationMGDNAsync(
        AAIController* Controller,
        const FVector& Goal,
        float AcceptanceRadius,
        float MoveSpeed, FMGDNMoveFinishedDynamicDelegate Callback);
    
    bool IsValidGameWorld() const;
};

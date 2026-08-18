// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DestructibleProjectile.generated.h"

class UTerrainDestructComponent;
class USphereComponent;
class UProjectileMovementComponent;

/**
 * Projectile that triggers terrain destruction upon impact or overlap.
 */
UCLASS(BlueprintType)
class DESTRUCTIBLECAVEGENERATOR_API ADestructibleProjectile : public AActor
{
	GENERATED_BODY()

public:
	ADestructibleProjectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	UFUNCTION(BlueprintCallable, Category = "DestructibleProjectile")
	void InitializeProjectile(bool bUseVisualMesh = true, bool bEnableOverlap = true, float InInitialSpeed = 1000.f, float InLifeSpan = 10.f, float InCollisionRadius = 1.0f);

	/** Whether to destroy the projectile upon any overlap */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction", meta = (ToolTip = "Whether to destroy this projectile immediately upon overlapping another actor."))
	bool DestroyOnOverlap = true;
protected:
	/** Collision sphere used for hit detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components", meta = (ToolTip = "Sphere collision component used to detect overlaps or hits."))
	USphereComponent* Collision;

	/** Optional mesh used to visually represent the projectile */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components", meta = (ToolTip = "Static mesh used to visually represent the projectile."))
	UStaticMeshComponent* VisualMesh;

	/** Component that applies terrain destruction when projectile impacts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components", meta = (ToolTip = "Handles terrain destruction behavior triggered by the projectile."))
	UTerrainDestructComponent* DestructComponent;

	/** Movement component for handling projectile velocity and simulation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components", meta = (ToolTip = "Controls projectile velocity, gravity, and movement behavior."))
	UProjectileMovementComponent* ProjectileMovement;

	float LifeSpanSeconds = 10.f;
	float InitialProjectileSpeed = 1000.f;
	bool bEnableOverlapHandling = true;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	/** Overlap handler triggered when the projectile touches another actor */
	UFUNCTION()
	void OnProjectileOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};

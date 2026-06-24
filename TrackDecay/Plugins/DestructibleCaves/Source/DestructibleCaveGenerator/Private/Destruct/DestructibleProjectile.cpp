// Copyright 2025 J2K2. All Rights Reserved.

#include "Destruct/DestructibleProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Destruct/TerrainDestructComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

ADestructibleProjectile::ADestructibleProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	// Create components
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(1.f);
	Collision->SetCollisionProfileName("BlockAll");
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetGenerateOverlapEvents(true);
	RootComponent = Collision;

	Collision->OnComponentBeginOverlap.AddDynamic(this, &ADestructibleProjectile::OnProjectileOverlap);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(Collision);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh)
	{
		VisualMesh->SetStaticMesh(SphereMesh);
		VisualMesh->SetRelativeScale3D(FVector(0.3f));
	}

	DestructComponent = CreateDefaultSubobject<UTerrainDestructComponent>(TEXT("DestructComponent"));
	DestructComponent->SetupAttachment(Collision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 1000.f;
	ProjectileMovement->MaxSpeed = 1000.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
}

void ADestructibleProjectile::InitializeProjectile(bool bUseVisualMesh, bool bEnableOverlap, float InInitialSpeed, float InLifeSpan, float InCollisionRadius)
{
	if (VisualMesh)
	{
		VisualMesh->SetVisibility(bUseVisualMesh);
	}

	if (Collision)
	{
		Collision->SetGenerateOverlapEvents(bEnableOverlap);
		Collision->SetSphereRadius(InCollisionRadius);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = InInitialSpeed;
		ProjectileMovement->MaxSpeed = InInitialSpeed;
	}

	SetLifeSpan(InLifeSpan);
}

void ADestructibleProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(LifeSpanSeconds);
	SetActorTickInterval(0.2f);
}

void ADestructibleProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (DestructComponent && !DestroyOnOverlap)
	{
		if (DestructComponent->ApplyDestruction())
		{
			//if (DestroyOnOverlap)Destroy();
		}
	}
}

void ADestructibleProjectile::OnProjectileOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (DestructComponent && DestroyOnOverlap)
	{
		DestructComponent->ApplyDestruction();
	}
	if (DestroyOnOverlap)Destroy();
}

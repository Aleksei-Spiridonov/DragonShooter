// Fill out your copyright notice in the Description page of Project Settings.


#include "Dragon.h"

#include "Components/CapsuleComponent.h"
#include "PhysicsControlComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "DrawDebugHelpers.h"

void ADragon::ToggleDebugVisualization_Implementation()
{
	if (bDebugOn)
	{
		bDebugOn = false;
	} else
	{
		bDebugOn = true;
	}
	
	for (int i = 0; i < 4; i++)
	{
		AttachPoints[i]->SetVisibility(bDebugOn);
	}
}


ADragon::ADragon()
{

	
	PrimaryActorTick.bCanEverTick = true;

	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("DragonMesh");
	SetRootComponent(SkeletalMeshComponent);
	SkeletalMeshComponent->SetSimulatePhysics(true);

	SkeletalMeshComponent->SetEnableGravity(false);
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	
	USkeletalMesh* DragonMesh = LoadObject<USkeletalMesh>(
		this,
		TEXT("/Script/Engine.SkeletalMesh'/Game/Dragon/SM_Dragon.SM_Dragon'")
	);

	Anim = LoadObject<UAnimSequence>(
		this,
		TEXT("/Script/Engine.AnimSequence'/Game/Dragon/AS_Dragon.AS_Dragon'")
	);

	FTransform DragonTransform;

	SkeletalMeshComponent->SetSkeletalMesh(DragonMesh);
	SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SkeletalMeshComponent->SetAnimation(Anim);

	

	PhysicsControl = CreateDefaultSubobject<UPhysicsControlComponent>("PhysicsControl");

	UStaticMesh* debugMesh = LoadObject<UStaticMesh>(this, TEXT("/Script/Engine.StaticMesh'/Game/LevelPrototyping/Meshes/SM_ChamferCube.SM_ChamferCube'"));
	// Create 4 attach points for legs
	for (int i = 0; i < 4; i++)
	{
		UStaticMeshComponent* NewAttachPoint = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("AttachPoint%d"), i));
		NewAttachPoint->SetupAttachment(RootComponent);
		NewAttachPoint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		NewAttachPoint->SetStaticMesh(debugMesh);
		NewAttachPoint->SetRelativeScale3D({0.1, 0.1, 0.1});
		NewAttachPoint->SetVisibility(false);
		AttachPoints.Add(NewAttachPoint);
	}
}

void ADragon::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                    FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bIsAttached)
	{
		reattach();

		if (!bIsAttached)
		{
			force += Hit.Normal * 40;

			FVector impactNormal = Hit.Normal.GetSafeNormal();

			// Direction from actor to hit point
			FVector relativeHitPoint = (Hit.ImpactPoint - SkeletalMeshComponent->GetComponentLocation()).
				GetSafeNormal();

			FVector rotationAxis = FVector::CrossProduct(relativeHitPoint, impactNormal);

			if (rotationAxis.IsNearlyZero())
			{
				rotationAxis = impactNormal;
			}

			rotationAxis.Normalize();
			float rotationAngle = NormalImpulse.Size() * 0.00002f;
			FQuat smallRotation = FQuat(rotationAxis, rotationAngle);

			targetRotation = smallRotation * targetRotation;
		}
	}
}

void ADragon::AddTranslation(FVector DeltaPos)
{
	SkeletalMeshComponent->SetRelativeLocation(SkeletalMeshComponent->GetRelativeLocation() + DeltaPos, false, nullptr,
	                                           ETeleportType::TeleportPhysics);
}


void ADragon::SetTranslation(FVector Pos)
{
	SkeletalMeshComponent->SetRelativeLocation(Pos, false, nullptr, ETeleportType::TeleportPhysics);
}


void ADragon::BeginPlay()
{
	Super::BeginPlay();

	
	targetRotation = SkeletalMeshComponent->GetComponentQuat();
	SkeletalMeshComponent->OnComponentHit.AddDynamic(this, &ADragon::OnHit);

	float forceBase = 30000;

	FPhysicsControlData controlData = FPhysicsControlData();
	controlData.LinearStrength = forceBase;
	controlData.LinearDampingRatio = 1;
	controlData.MaxForce = forceBase;

	controlData.AngularStrength = forceBase * 0.1;
	controlData.AngularDampingRatio = 1;
	controlData.MaxTorque = forceBase * 0.1;

	controlData.bUseSkeletalAnimation = true;

	TArray<FName> names = PhysicsControl->CreateControlsFromSkeletalMeshBelow(
		SkeletalMeshComponent, SkeletalMeshComponent->GetBoneName(0), true, EPhysicsControlType::WorldSpace,
		controlData,
		TEXT("All"));


	if (defaultAttachPositions.Num() != 4)
	{
		defaultAttachPositions.Empty();
		defaultLegCenter = FVector(0, 0, 0);
		/*
		for (FName legBoneName : legNames)
		{
			FRigidBodyState legState;
			SkeletalMeshComponent->GetRigidBodyState(legState, legBoneName);
			FVector legPosWorld = legState.Position;
			FVector legPosLocal = SkeletalMeshComponent->GetComponentTransform().InverseTransformPosition(legPosWorld);
			
			defaultAttachPositions.Add(legPosLocal);
			defaultLegCenter += legPosLocal;
		}
		defaultLegCenter /= legNames.Num();
		*/

		defaultAttachPositions = {
			FVector(20, 62.428681, 0),
			FVector(20, -30.550485, 0),
			FVector(-20, 62.428681, 0),
			FVector(-20, -30.550485, 0),
		};
		defaultLegCenter = {};
		for (int i = 0; i < 4; i++)
		{
			defaultLegCenter += defaultAttachPositions[i] / 4;
		}
	}
}

void ADragon::reattach()
{
	const FVector FirstRayLocalOrigin = FVector(0.f, 0.f, 100.f);
	const int NumCenterRays = 16;
	const float ConeAngleDeg = 30.f;
	const float FirstRayDistance = 200.f;
	const int MinFirstHits = 6; 
	const float PlaneFitMaxRMS = 10.0f; 
	const float AlignDotThreshold = 0.7f;
	const float MinLegSurfaceDot = 0.71f;

	// Helper: world-space origin for first rays
	const FTransform ActorTransform = GetActorTransform();
	const FVector FirstRayWorldOrigin = ActorTransform.TransformPosition(FirstRayLocalOrigin);

	// Compute the cone axis: local down in world space
	FVector LocalDownWorld = ActorTransform.TransformVector(FVector(0.f, 0.f, -1.f));
	LocalDownWorld.Normalize();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	// First raycast: cone of rays from FirstRayWorldOrigin toward local down
	TArray<FVector> firstHitPositions;
	firstHitPositions.Reserve(NumCenterRays);

	// Create directions evenly around azimuth on fixed cone angle
	const float ConeRad = FMath::DegreesToRadians(ConeAngleDeg);
	for (int i = 0; i < NumCenterRays; ++i)
	{
		// Azimuth evenly distributed
		float Azimuth = (2.f * PI) * (float(i) / float(NumCenterRays));
		FVector Perp = LocalDownWorld.GetAbs().X < 0.99f
			               ? FVector::CrossProduct(LocalDownWorld, FVector::RightVector)
			               : FVector::CrossProduct(LocalDownWorld, FVector::UpVector);
		Perp.Normalize();

		FQuat TiltQuat = FQuat(Perp, ConeRad); // tilt away from axis by cone angle
		FQuat SpinQuat = FQuat(LocalDownWorld, Azimuth); // spin around axis
		FVector Dir = SpinQuat * (TiltQuat * LocalDownWorld); // final direction
		Dir.Normalize();

		FVector RayStart = FirstRayWorldOrigin;
		FVector RayEnd = RayStart + Dir * FirstRayDistance;

		FHitResult Hit;
		bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, RayStart, RayEnd, ECollisionChannel::ECC_Visibility,
		                                                 Params);
		if (bHit)
		{
			firstHitPositions.Add(Hit.ImpactPoint);
		}
	}

	// Need at least MinFirstHits hits
	if (firstHitPositions.Num() < MinFirstHits)
	{
		bIsAttached = false;
		return;
	}

	// Fit a plane to the hit positions
	const int PN = firstHitPositions.Num();
	// Find pair A,B with maximum distance
	int idxA = 0, idxB = 1;
	float maxPairDistSq = 0.f;
	for (int i = 0; i < PN; i++)
	{
		for (int j = i + 1; j < PN; j++)
		{
			float d2 = FVector::DistSquared(firstHitPositions[i], firstHitPositions[j]);
			if (d2 > maxPairDistSq)
			{
				maxPairDistSq = d2;
				idxA = i;
				idxB = j;
			}
		}
	}

	// Find C maximizing distance to line AB
	FVector PA = firstHitPositions[idxA];
	FVector PB = firstHitPositions[idxB];
	FVector AB = PB - PA;
	float ABLen = AB.Size();
	if (ABLen < 0.0001)
	{
		bIsAttached = false;
		return;
	}
	int idxC = -1;
	float maxDistToLine = -1.f;
	for (int i = 0; i < PN; ++i)
	{
		// distance from point to line  AB: |(AP x AB)| / |AB|
		FVector AP = firstHitPositions[i] - PA;
		FVector Cross = FVector::CrossProduct(AP, AB);
		float distToLine = Cross.Size() / ABLen;
		if (distToLine > maxDistToLine)
		{
			maxDistToLine = distToLine;
			idxC = i;
		}
	}

	if (idxC == -1)
	{
		bIsAttached = false;
		return;
	}

	

	FVector PC = firstHitPositions[idxC];
	FVector Normal = FVector::CrossProduct(AB, PC - PA);
	if (Normal.IsNearlyZero())
	{
		bIsAttached = false;
		return;
	}
	Normal.Normalize();

	// plane RMS error (distance of each point to the plane defined by (PA, Normal))
	double sumSq = 0.0;
	for (const FVector& P : firstHitPositions)
	{
		double dist = FMath::Abs(FVector::DotProduct(Normal, (P - PA)));
		sumSq += dist * dist;
	}
	double rms = FMath::Sqrt(sumSq / (double)PN);

	if (rms > PlaneFitMaxRMS)
	{
		// Plane fit too poor -> no attach
		bIsAttached = false;
		return;
	}

	//Compare plane normal with skeletal mesh local up (in world)
	FVector MeshLocalUpWorld = SkeletalMeshComponent->GetComponentTransform().TransformVector(FVector(0.f, 0.f, 1.f));
	MeshLocalUpWorld.Normalize();

	float AlignDot = FVector::DotProduct(Normal, MeshLocalUpWorld);
	// If dot negative, flip normal so it's oriented toward mesh up for a consistent comparison
	if (AlignDot < 0.f)
	{
		Normal *= -1.f;
		AlignDot = -AlignDot;
	}

	if (AlignDot < AlignDotThreshold)
	{
		// Surface too slanted compared to mesh up -> cannot attach
		bIsAttached = false;
		return;
	}



	// Apply rotation to skeletal mesh immediately to align mesh up to plane normal
	FQuat DeltaQuat = FQuat::FindBetweenNormals(MeshLocalUpWorld, Normal);
	// Apply to current component rotation
	FTransform MeshTransform = SkeletalMeshComponent->GetComponentTransform();
	FQuat CurrentQuat = MeshTransform.GetRotation();
	FQuat orientedRotation = DeltaQuat * CurrentQuat;
	targetRotation = orientedRotation;

	// Second raycasts: from each default attach position straight down
	// Compute reach using your expression
	float Reach = FMath::Max(currentVelocity.Size() * lastDeltaTime * 2.f, 10.f);

	FVector MeshLocalDownWorld = SkeletalMeshComponent->GetComponentTransform().
	                                                    TransformVector(FVector(0.f, 0.f, -1.f));
	MeshLocalDownWorld.Normalize();

	TArray<FHitResult> legHits;
	legHits.SetNumZeroed(legNames.Num());

	FTransform orientedTransform = SkeletalMeshComponent->GetComponentTransform();
	orientedTransform.SetRotation(orientedRotation);

	for (int i = 0; i < legNames.Num(); ++i)
	{
		FVector LegLocal = defaultAttachPositions[i];
		FVector LegWorldPos = orientedTransform.TransformPosition(LegLocal);

		FVector RayStart = LegWorldPos - MeshLocalDownWorld * 200.f; // start a bit above
		FVector RayEnd = LegWorldPos + MeshLocalDownWorld * Reach; // cast downwards

		FHitResult LegHit;
		bool bHit = GetWorld()->LineTraceSingleByChannel(LegHit, RayStart, RayEnd, ECollisionChannel::ECC_Visibility,
		                                                 Params);

		if (!bHit || LegHit.ImpactNormal.Z < MinLegSurfaceDot)
		{
			// One leg failed -> abort attaching
			bIsAttached = false;
			return;
		}

		legHits[i] = LegHit;
	}

	

	// Attach legs (use same rules as original)
	for (int i = 0; i < legNames.Num(); ++i)
	{
		FHitResult& LegHit = legHits[i];
		USceneComponent* AttachComp = AttachPoints[i];

		if (!AttachComp)
		{
	GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, "wtf 1");
			
			bIsAttached = false;
			return;
		}

		if (!LegHit.Component.IsValid())
		{
			
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, "wtf 2");
			bIsAttached = false;
			return;
		}
		
		AttachComp->AttachToComponent(
			LegHit.Component.Get(),
			FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld,
			                          EAttachmentRule::KeepWorld, false),
			LegHit.BoneName);
		AttachComp->SetWorldLocation(LegHit.ImpactPoint);
	}
	
	


	currentVelocity = FVector::ZeroVector;
	bIsAttached = true;
}


void ADragon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsAttached)
	{
		// Gravity and motion logic unchanged
		force += FVector(0, 0, -980) * DeltaTime;
		currentVelocity += force;
		force = FVector(0, 0, 0);

		float verticalDamping = 0.3;
		verticalDamping = FMath::Pow(1 - verticalDamping, DeltaTime);
		float horizontalDamping = 0.5;
		horizontalDamping = FMath::Pow(1 - horizontalDamping, DeltaTime);

		currentVelocity.Y += horizontalDamping;
		currentVelocity.X += horizontalDamping;
		currentVelocity.Z *= verticalDamping;

		FQuat currentRot = SkeletalMeshComponent->GetComponentQuat();
		FQuat delta = targetRotation * currentRot.Inverse();
		delta.Normalize();
		FVector deltaAxis;
		float deltaAngle;
		delta.ToAxisAndAngle(deltaAxis, deltaAngle);
		float rotationPerSecond = 0.9f;
		float alpha = 1.0f - FMath::Pow(1.0f - rotationPerSecond, DeltaTime);
		float takenAngle = alpha * deltaAngle;
		FQuat stepQuat(deltaAxis, takenAngle);
		FQuat newRot = stepQuat * currentRot;

		SkeletalMeshComponent->SetWorldRotation(
			newRot, false, nullptr, ETeleportType::TeleportPhysics
		);


		AddTranslation(currentVelocity * DeltaTime);

		reattach();
	}
	else
	{
		if (AttachPoints.Num() == 4 && defaultAttachPositions.Num() == 4)
		{
			FVector sumDeltaTranslation = FVector::ZeroVector;
			FQuat totalQuat = FQuat::Identity;

			FVector selfLocationWorld = SkeletalMeshComponent->GetComponentLocation();

			for (int i = 0; i < AttachPoints.Num(); ++i)
			{
				FVector attachPointWorld = AttachPoints[i]->GetComponentLocation();
				FVector defaultPointWorld = SkeletalMeshComponent->GetComponentTransform().TransformPosition(defaultAttachPositions[i]);

				// Convert to local space relative to SkeletalMesh location
				FVector attachLocal = attachPointWorld - selfLocationWorld;
				FVector defaultLocal = defaultPointWorld - selfLocationWorld;

				sumDeltaTranslation += (attachPointWorld - defaultPointWorld);

				FQuat q = FQuat::FindBetweenNormals(defaultLocal.GetSafeNormal(), attachLocal.GetSafeNormal());

				// Don't add a point from the opposite hemisphere
				float dot = totalQuat.X * q.X + totalQuat.Y * q.Y + totalQuat.Z * q.Z + totalQuat.W * q.W;
				if (dot < 0)
				{
					q = q * -1.0f;
				}

				totalQuat.X += q.X;
				totalQuat.Y += q.Y;
				totalQuat.Z += q.Z;
				totalQuat.W += q.W;
			}

			FVector deltaTranslation = sumDeltaTranslation / AttachPoints.Num();
			totalQuat.Normalize();

			FTransform newTransform = FTransform(totalQuat * SkeletalMeshComponent->GetComponentQuat(), selfLocationWorld + deltaTranslation);
			SkeletalMeshComponent->SetWorldTransform(newTransform, false, nullptr, ETeleportType::TeleportPhysics);

			// Verify the positions of the points
			{

				TArray<float> distances = {};
				for (int i = 0; i < AttachPoints.Num(); i++)
				{
					FVector attachPointWorldPos = AttachPoints[i]->GetComponentLocation();
					FVector defaultPointWorldPos = SkeletalMeshComponent->GetComponentTransform().TransformPosition(defaultAttachPositions[i]);
					distances.Add(FVector::Dist(attachPointWorldPos, defaultPointWorldPos));
				}
				
				float sumDistance = 0;
				for (int i = 0; i < distances.Num(); i++)
				{
					sumDistance += distances[i];
				}

				if (sumDistance > 50)
				{
					reattach();
				}
			}
		}
	}
	lastDeltaTime = DeltaTime;
}


void ADragon::followPlayer(float DeltaTime)
{
	ACharacter* player = Cast<ACharacter>(UGameplayStatics::GetActorOfClass(this, ACharacter::StaticClass()));

	if (player)
	{
		FVector playerPos = player->GetActorLocation();
		FVector FloorLocation = playerPos;
		// Raycast floor
		{
			FVector Start = playerPos;
			FVector End = Start - FVector(0, 0, 10000); // cast 10,000 units downward

			FHitResult HitResult;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(player); // don't hit yourself

			bool bHit = GetWorld()->LineTraceSingleByChannel(
				HitResult,
				Start,
				End,
				ECC_Visibility,
				Params
			);

			if (bHit)
			{
				FloorLocation = HitResult.Location;
			}
		}

		FVector direction = (FloorLocation - GetActorLocation());
		if (direction.Length() > 150)
		{
			direction.Normalize();
			FTransform newTransform = SkeletalMeshComponent->GetRelativeTransform();
			newTransform.SetLocation(newTransform.GetLocation() + direction * DeltaTime * MovementSpeed);

			FQuat forwardRotation = direction.Rotation().Quaternion();
			FQuat Adjust(FRotator(0.f, -90.f, 0.f));
			forwardRotation = Adjust * forwardRotation;

			newTransform.SetRotation(forwardRotation);


			SetActorTransform(newTransform, false, nullptr, ETeleportType::TeleportPhysics);
			SkeletalMeshComponent->SetWorldTransform(newTransform);
		}
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 60, FColor::Red, TEXT("Cannot find player"));
	}
}

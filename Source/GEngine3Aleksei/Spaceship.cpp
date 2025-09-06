// Fill out your copyright notice in the Description page of Project Settings.


#include "Spaceship.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetSystemLibrary.h"

// Sets default values
ASpaceship::ASpaceship()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(Capsule);

	DebugMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugMesh"));
	DebugMesh->SetupAttachment(Capsule);

	UStaticMesh* staticMesh = FindObject<UStaticMesh>(
		this, TEXT("/Script/Engine.StaticMesh'/Engine/BasicShapes/Cylinder.Cylinder'"));
	DebugMesh->SetStaticMesh(staticMesh);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(DebugMesh);

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComp->SetupAttachment(SpringArm);

	FloatingPawnMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingPawnMovement"));

	HonkInputAction = LoadObject<UInputAction>(this, TEXT("/Script/EnhancedInput.InputAction'/Game/Input/IA_Honk.IA_Honk'"));
}

void ASpaceship::SetupPlayerInputComponent(UInputComponent* inputComponent)
{
	Super::SetupPlayerInputComponent(inputComponent);
	UEnhancedInputComponent* eic = Cast<UEnhancedInputComponent>(InputComponent);
	if (eic)
	{
		
		eic->BindAction(HonkInputAction, ETriggerEvent::Triggered, this, &ASpaceship::HonkActionCallback);
	}
}

// Called when the game starts or when spawned
void ASpaceship::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ASpaceship::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ASpaceship::HonkActionCallback(const FInputActionValue& Value)
{
	GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Green, TEXT("Working"));

	
	
}

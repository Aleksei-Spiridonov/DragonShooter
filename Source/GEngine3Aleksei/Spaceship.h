// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnhancedPlayerInput.h"

#include "Spaceship.generated.h"

class UFloatingPawnMovement;
class UCameraComponent;
class UCapsuleComponent;
class UStaticMeshComponent;
class USpringArmComponent;

struct FInputActionValue;

UCLASS()
class GENGINE3ALEKSEI_API ASpaceship : public APawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	UCapsuleComponent* Capsule;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* DebugMesh;

	UPROPERTY(EditAnywhere)
	UCameraComponent* CameraComp;

	UPROPERTY(EditAnywhere)
	USpringArmComponent* SpringArm;

	UPROPERTY(EditAnywhere)
	UFloatingPawnMovement* FloatingPawnMovement;

	UPROPERTY(EditAnywhere)
	UInputAction* HonkInputAction;
		
public:	
	// Sets default values for this actor's properties
	ASpaceship();

	virtual void SetupPlayerInputComponent(class UInputComponent* InputComponent) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void HonkActionCallback(const FInputActionValue& Value);

};

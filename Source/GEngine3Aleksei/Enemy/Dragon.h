// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Animation/AnimInstance.h"
#include "GEngine3Aleksei/Public/ToggleableDebugVisualization.h"
#include "Dragon.generated.h"


class UPhysicsControlComponent;
UCLASS()

class GENGINE3ALEKSEI_API UDragonAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> PhysicalBones;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FTransform> PhysicalTransforms;
};


class UCapsuleComponent;

UCLASS()
class GENGINE3ALEKSEI_API ADragon : public AActor, public IToggleableDebugVisualization
{
	GENERATED_BODY()
	
protected:

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USkeletalMeshComponent* SkeletalMeshComponent;
	
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UAnimSequence* Anim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UPhysicsControlComponent* PhysicsControl;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MovementSpeed;

	UPROPERTY(EditAnywhere)
	TArray<UStaticMeshComponent*> AttachPoints;
	
	TArray<FVector> defaultAttachPositions; // Positions of legs in local coordinate system.
	FVector defaultLegCenter;

	bool bDebugOn = false;
    virtual void ToggleDebugVisualization_Implementation() override;

	
	
	FVector currentVelocity;
	FVector force;
	float lastDeltaTime;
	
	bool bIsAttached = false;
	
public:	
	// Sets default values for this actor's properties
	ADragon();

	
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void AddTranslation(FVector DeltaPos);
	void SetTranslation(FVector Pos);
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void reattach();

	TArray<FName> legNames = {TEXT("Bone_015"), TEXT("Bone_041"), TEXT("Bone_024"), TEXT("Bone_036")};

	FQuat targetRotation = FQuat::Identity;
 	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	void followPlayer(float DeltaTime);


	
	static FQuat FindBestFitQuaternion(const TArray<FVector>& OriginalPoints, const TArray<FVector>& CurrentPoints, float& OutError)
{
    check(OriginalPoints.Num() == CurrentPoints.Num() && OriginalPoints.Num() >= 3);
    const int32 N = OriginalPoints.Num();

    // 1) compute centroids
    FVector CentroidOrig(0,0,0), CentroidCurr(0,0,0);
    for (int32 i = 0; i < N; ++i)
    {
        CentroidOrig += OriginalPoints[i];
        CentroidCurr += CurrentPoints[i];
    }
    CentroidOrig /= (float)N;
    CentroidCurr /= (float)N;

    // 2) compute 3x3 covariance matrix (double precision for stability)
    double Cov[3][3] = { {0.0,0.0,0.0}, {0.0,0.0,0.0}, {0.0,0.0,0.0} };
    for (int32 i = 0; i < N; ++i)
    {
        const FVector P = OriginalPoints[i] - CentroidOrig;
        const FVector Q = CurrentPoints[i] - CentroidCurr;
        Cov[0][0] += (double)P.X * (double)Q.X;
        Cov[0][1] += (double)P.X * (double)Q.Y;
        Cov[0][2] += (double)P.X * (double)Q.Z;

        Cov[1][0] += (double)P.Y * (double)Q.X;
        Cov[1][1] += (double)P.Y * (double)Q.Y;
        Cov[1][2] += (double)P.Y * (double)Q.Z;

        Cov[2][0] += (double)P.Z * (double)Q.X;
        Cov[2][1] += (double)P.Z * (double)Q.Y;
        Cov[2][2] += (double)P.Z * (double)Q.Z;
    }

    // 3) Build 4x4 symmetric N matrix from Horn (using doubles)
    // N = [ trace(Cov) , (Cov[1][2]-Cov[2][1]), (Cov[2][0]-Cov[0][2]), (Cov[0][1]-Cov[1][0]);
    //       ... symmetric ... ]
    double Nmat[4][4];
    double trace = Cov[0][0] + Cov[1][1] + Cov[2][2];

    Nmat[0][0] = trace;
    Nmat[0][1] = Cov[1][2] - Cov[2][1];
    Nmat[0][2] = Cov[2][0] - Cov[0][2];
    Nmat[0][3] = Cov[0][1] - Cov[1][0];

    Nmat[1][0] = Nmat[0][1];
    Nmat[1][1] = Cov[0][0] - Cov[1][1] - Cov[2][2];
    Nmat[1][2] = Cov[0][1] + Cov[1][0];
    Nmat[1][3] = Cov[0][2] + Cov[2][0];

    Nmat[2][0] = Nmat[0][2];
    Nmat[2][1] = Nmat[1][2];
    Nmat[2][2] = -Cov[0][0] + Cov[1][1] - Cov[2][2];
    Nmat[2][3] = Cov[1][2] + Cov[2][1];

    Nmat[3][0] = Nmat[0][3];
    Nmat[3][1] = Nmat[1][3];
    Nmat[3][2] = Nmat[2][3];
    Nmat[3][3] = -Cov[0][0] - Cov[1][1] + Cov[2][2];

    // 4) Power iteration to find largest eigenvector of 4x4 symmetric matrix.
    // Use double for numerical stability.
    double v[4] = {1.0, 0.0, 0.0, 0.0}; // initial guess (can be any non-zero)
    const int MaxIters = 100;
    const double Eps = 1e-12;
    for (int iter = 0; iter < MaxIters; ++iter)
    {
        double w[4] = {0.0, 0.0, 0.0, 0.0};
        // w = Nmat * v
        for (int r = 0; r < 4; ++r)
        {
            double sum = 0.0;
            for (int c = 0; c < 4; ++c)
            {
                sum += Nmat[r][c] * v[c];
            }
            w[r] = sum;
        }

        // normalize w -> next v
        double norm = 0.0;
        for (int i = 0; i < 4; ++i) norm += w[i] * w[i];
        if (norm <= 0.0) break;
        norm = FMath::Sqrt((float)norm);
        for (int i = 0; i < 4; ++i) w[i] /= norm;

        // check convergence: ||v - w||
        double diff = 0.0;
        for (int i = 0; i < 4; ++i)
        {
            double d = v[i] - w[i];
            diff += d * d;
            v[i] = w[i];
        }
        if (diff < Eps) break;
    }

    // Ensure quaternion ordering: Horn eigenvector is [q0, qx, qy, qz]
    double q0 = v[0];
    double qx = v[1];
    double qy = v[2];
    double qz = v[3];

    // If vector is near zero for some reason, fallback to identity
    double vnorm2 = q0*q0 + qx*qx + qy*qy + qz*qz;
    if (vnorm2 < 1e-16)
    {
        OutError = 0.0f;
        return FQuat::Identity;
    }

    // Normalize quaternion (convert to floats for FQuat)
    double invNorm = 1.0 / FMath::Sqrt((float)vnorm2);
    q0 *= invNorm;
    qx *= invNorm;
    qy *= invNorm;
    qz *= invNorm;

    // Unreal's FQuat constructor is FQuat(X, Y, Z, W) where W is scalar part.
    FQuat Rotation((float)qx, (float)qy, (float)qz, (float)q0);
    Rotation.Normalize();

    // 5) Compute RMSD (error)
    double sumSq = 0.0;
    for (int32 i = 0; i < N; ++i)
    {
        const FVector Pcenter = OriginalPoints[i] - CentroidOrig;
        const FVector Transformed = Rotation.RotateVector(Pcenter) + CentroidCurr;
        const double dx = (double)Transformed.X - (double)CurrentPoints[i].X;
        const double dy = (double)Transformed.Y - (double)CurrentPoints[i].Y;
        const double dz = (double)Transformed.Z - (double)CurrentPoints[i].Z;
        sumSq += dx*dx + dy*dy + dz*dz;
    }
    OutError = FMath::Sqrt((float)(sumSq / (double)N));

    return Rotation;
}




};

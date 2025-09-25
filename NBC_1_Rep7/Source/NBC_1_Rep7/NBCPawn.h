// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h" // FInputActionValue
#include "NBCPawn.generated.h"


class UCapsuleComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;


UCLASS()
class NBC_1_REP7_API ANBCPawn : public APawn
{
	GENERATED_BODY()


public:
	ANBCPawn();


	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;


protected:
	// Components
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCapsuleComponent* CapsuleComp;


	UPROPERTY(VisibleAnywhere, Category = "Components")
	USkeletalMeshComponent* MeshComp;


	UPROPERTY(VisibleAnywhere, Category = "Components")
	USpringArmComponent* SpringArm;


	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* CameraComp;


	// Movement parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MoveSpeed;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float RotationSpeed; // degrees/sec for yaw & pitch


	// Camera pitch clamp
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinPitch;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MaxPitch;


	// Invert Y
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bInvertY;


	// Enhanced Input assets (assign in BP or via code)
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Move;


	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Look;


	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* IMC_Default;


	// Runtime inputs (stored each frame by callbacks)
	FVector2D MoveInput; // X=Right, Y=Forward (depends on how you set mapping)
	FVector2D LookInput; // X=MouseX (yaw), Y=MouseY (pitch)


	float CameraPitch;


	// input handlers
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
};
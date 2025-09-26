#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
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

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
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

    // Movement speeds
    UPROPERTY(EditAnywhere, Category = "Movement")
    float MoveSpeedGround = 600.f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float MoveSpeedAir = 300.f;

    // runtime current move speed
    float CurrentMoveSpeed = 600.f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float VerticalSpeed = 400.f;

    // Look / rotation
    UPROPERTY(EditAnywhere, Category = "Movement")
    float RotationSpeed = 120.f; // degrees/sec for yaw & pitch

    UPROPERTY(EditAnywhere, Category = "Camera")
    float MinPitch = -80.f;

    UPROPERTY(EditAnywhere, Category = "Camera")
    float MaxPitch = 80.f;

    UPROPERTY(EditAnywhere, Category = "Camera")
    bool bInvertY = false;

    // Tilt (visual roll)
    UPROPERTY(EditAnywhere, Category = "Movement")
    float TiltSpeed = 90.f; // degrees/sec for target change

    UPROPERTY(EditAnywhere, Category = "Movement")
    float MaxTiltAngle = 25.f; // visual tilt limit

    // gravity
    UPROPERTY(EditAnywhere, Category = "Movement")
    float Gravity = -980.f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float GroundProbeDistance = 6.f;

    // Input assets (assign in BP)
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* IA_Move;

    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* IA_Look;

    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* IA_Tilt; // Axis1D Q/E

    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* IA_Ascend;

    UPROPERTY(EditAnywhere, Category = "Input")
    UInputMappingContext* IMC_Default;

    // Runtime inputs/state
    FVector2D MoveInput;
    FVector2D LookInput;
    float TiltInput = 0.f;      // -1..1 from Q/E
    float VerticalInput = 0.f;  // -1..1 from Shift/Space

    float CameraPitch = -10.f;
    float VerticalVelocity = 0.f;
    bool bIsGrounded = false;

    // visual roll interpolation
    float SpringArmRollTarget = 0.f;
    float SpringArmRollCurrent = 0.f;

    // Input handlers
    void OnMove(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);
    void OnTilt(const FInputActionValue& Value);
    void OnAscend(const FInputActionValue& Value);

    // helpers
    void UpdateGroundedState();
};
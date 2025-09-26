// Fill out your copyright notice in the Description page of Project Settings.


#include "NBCPawn.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

ANBCPawn::ANBCPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // Root collision
    CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComp"));
    RootComponent = CapsuleComp;
    CapsuleComp->SetSimulatePhysics(false);

    // Mesh
    MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetSimulatePhysics(false);

    // Spring arm + camera
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 300.f;
    SpringArm->bUsePawnControlRotation = false; // we'll control pitch manually

    CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
    CameraComp->SetupAttachment(SpringArm);

    // Defaults
    MoveSpeed = 600.f;
    RotationSpeed = 120.f; // degrees per second
    MinPitch = -80.f;
    MaxPitch = 80.f;
    bInvertY = false;
    CameraPitch = -10.f;

    TiltSpeed = 90.f; // degrees/sec
    MaxTiltAngle = 45.f; // clamp roll

    VerticalSpeed = 400.f;

    CurrentRoll = 0.f;
    TiltInput = 0.f;
    VerticalInput = 0.f;

    // For quick testing in editor: auto possess player 0 (optional)
    AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void ANBCPawn::BeginPlay()
{
    Super::BeginPlay();

    // Add Mapping Context to local player subsystem if available
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                if (IMC_Default)
                {
                    Subsys->AddMappingContext(IMC_Default, 0);
                }
            }
        }
    }

    // Initialize springarm pitch
    SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, 0.f));
}

void ANBCPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // Bind actions (these UInputAction* are assigned in editor or via code)
        if (IA_Move)
        {
            EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ANBCPawn::OnMove);
            EIC->BindAction(IA_Move, ETriggerEvent::Completed, this, &ANBCPawn::OnMove);
            EIC->BindAction(IA_Move, ETriggerEvent::Canceled, this, &ANBCPawn::OnMove);
        }

        if (IA_Look)
        {
            EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ANBCPawn::OnLook);
            EIC->BindAction(IA_Look, ETriggerEvent::Completed, this, &ANBCPawn::OnLook);
            EIC->BindAction(IA_Look, ETriggerEvent::Canceled, this, &ANBCPawn::OnLook);
        }

        if (IA_Tilt)
        {
            EIC->BindAction(IA_Tilt, ETriggerEvent::Triggered, this, &ANBCPawn::OnTilt);
            EIC->BindAction(IA_Tilt, ETriggerEvent::Completed, this, &ANBCPawn::OnTilt);
            EIC->BindAction(IA_Tilt, ETriggerEvent::Canceled, this, &ANBCPawn::OnTilt);
        }

        if (IA_Ascend)
        {
            EIC->BindAction(IA_Ascend, ETriggerEvent::Triggered, this, &ANBCPawn::OnAscend);
            EIC->BindAction(IA_Ascend, ETriggerEvent::Completed, this, &ANBCPawn::OnAscend);
            EIC->BindAction(IA_Ascend, ETriggerEvent::Canceled, this, &ANBCPawn::OnAscend);
        }
    }
}

void ANBCPawn::OnMove(const FInputActionValue& Value)
{
    MoveInput = Value.Get<FVector2D>();
}

void ANBCPawn::OnLook(const FInputActionValue& Value)
{
    LookInput = Value.Get<FVector2D>();
}

void ANBCPawn::OnTilt(const FInputActionValue& Value)
{
    TiltInput = Value.Get<float>();
}

void ANBCPawn::OnAscend(const FInputActionValue& Value)
{
    VerticalInput = Value.Get<float>();
}

void ANBCPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // --- Movement (로컬 좌표계 기준) ---
    if (!MoveInput.IsNearlyZero())
    {
        FVector LocalOffset = FVector(MoveInput.Y * MoveSpeed * DeltaSeconds, MoveInput.X * MoveSpeed * DeltaSeconds, 0.f);
        AddActorLocalOffset(LocalOffset, true);
    }

    // Vertical (상승/하강)
    if (FMath::Abs(VerticalInput) > KINDA_SMALL_NUMBER)
    {
        FVector VerticalOffset = FVector(0.f, 0.f, VerticalInput * VerticalSpeed * DeltaSeconds);
        AddActorLocalOffset(VerticalOffset, true);
    }

    // --- Rotation (Yaw on Pawn, Pitch on spring arm) ---
    if (FMath::Abs(LookInput.X) > KINDA_SMALL_NUMBER)
    {
        float YawDelta = LookInput.X * RotationSpeed * DeltaSeconds;
        AddActorLocalRotation(FRotator(0.f, YawDelta, 0.f)); // local yaw
    }

    if (FMath::Abs(LookInput.Y) > KINDA_SMALL_NUMBER)
    {
        float PitchDelta = (bInvertY ? 1.f : -1.f) * LookInput.Y * RotationSpeed * DeltaSeconds;
        CameraPitch = FMath::Clamp(CameraPitch + PitchDelta, MinPitch, MaxPitch);
        SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, 0.f));
    }

    // --- Tilt (Roll) applied to Pawn and Mesh ---
    if (FMath::Abs(TiltInput) > KINDA_SMALL_NUMBER)
    {
        float RollDelta = TiltInput * TiltSpeed * DeltaSeconds;
        CurrentRoll = FMath::Clamp(CurrentRoll + RollDelta, -MaxTiltAngle, MaxTiltAngle);
    }
    else
    {
        CurrentRoll = FMath::FInterpTo(CurrentRoll, 0.f, DeltaSeconds, 4.f);
    }

    // 4) Actor 회전을 안전하게 sweep로 적용 (충돌 대상과 겹치지 않도록)
    FRotator ActorRot = GetActorRotation();
    // 보존해야할 Yaw
    float NewYaw = ActorRot.Yaw;
    float NewPitch = 0.f; // 우리는 Actor 전체의 Pitch는 바꾸지 않을 수 있음 (원하면 여기 변경)
    // Set full desired rotation including roll
    FRotator DesiredRotation = FRotator(0.f, NewYaw, CurrentRoll);

    // Sweep rotation via root component to avoid penetration
    if (UCapsuleComponent* RootCol = CapsuleComp)
    {
        FHitResult Hit;
        RootCol->MoveComponent(FVector::ZeroVector, DesiredRotation.Quaternion(), true, &Hit);
        // MoveComponent로 sweep 하므로 충돌 시 적절히 막힘
    }

    // 5) Mesh 시각 보정: Mesh는 카메라의 Pitch와 Actor Roll을 함께 받음
    if (MeshComp)
    {
        // Mesh는 보통 Actor Roll의 반대값을 줘서 과도한 시각 왜곡을 줄여줌.
        // 여기서는 CameraPitch도 같이 적용하여 '앞뒤로 기울어짐' 구현
        FRotator MeshRelRot = FRotator(CameraPitch, 0.f, -CurrentRoll);
        MeshComp->SetRelativeRotation(MeshRelRot);
    }
}


//#include "NBCPawn.h"
//
//
//#include "Components/CapsuleComponent.h"
//#include "Components/SkeletalMeshComponent.h"
//#include "GameFramework/SpringArmComponent.h"
//#include "Camera/CameraComponent.h"
//#include "EnhancedInputComponent.h"
//#include "EnhancedInputSubsystems.h"
//#include "InputAction.h"
//#include "InputMappingContext.h"
//#include "GameFramework/PlayerController.h"
//#include "Engine/LocalPlayer.h"
//
//// Sets default values
//ANBCPawn::ANBCPawn()
//{
//	PrimaryActorTick.bCanEverTick = true;
//
//	CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComp"));
//	RootComponent = CapsuleComp;
//	CapsuleComp->SetSimulatePhysics(false);
//
//	// Mesh
//	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
//	MeshComp->SetupAttachment(RootComponent);
//	MeshComp->SetSimulatePhysics(false);
//
//	// Spring arm + camera
//	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
//	SpringArm->SetupAttachment(RootComponent);
//	SpringArm->TargetArmLength = 300.f;
//	SpringArm->bUsePawnControlRotation = false; // we'll control pitch manually
//
//	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
//	CameraComp->SetupAttachment(SpringArm);
//
//	// Defaults
//	MoveSpeed = 600.f;
//	RotationSpeed = 120.f; // degrees per second
//	MinPitch = -80.f;
//	MaxPitch = 80.f;
//	bInvertY = false;
//	CameraPitch = -10.f;
//
//	// 자동 player 인식기능 // GameMode로 대체
//	/*AutoPossessPlayer = EAutoReceiveInput::Player0;*/
//}
//
//// Called when the game starts or when spawned
//void ANBCPawn::BeginPlay()
//{
//	Super::BeginPlay();
//
//
//	// Add Mapping Context to local player subsystem if available
//	if (APlayerController* PC = Cast<APlayerController>(GetController()))
//	{
//		if (ULocalPlayer* LP = PC->GetLocalPlayer())
//		{
//			if (UEnhancedInputLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
//			{
//				if (IMC_Default)
//				{
//					Subsys->AddMappingContext(IMC_Default, 0);
//				}
//			}
//		}
//	}
//
//	SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, 0.f));
//}
//
//void ANBCPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
//{
//	Super::SetupPlayerInputComponent(PlayerInputComponent);
//
//	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
//	{
//		if (IA_Move)
//		{
//			EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ANBCPawn::OnMove);
//			EIC->BindAction(IA_Move, ETriggerEvent::Completed, this, &ANBCPawn::OnMove);
//			EIC->BindAction(IA_Move, ETriggerEvent::Canceled, this, &ANBCPawn::OnMove);
//		}
//
//		if (IA_Look)
//		{
//			EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ANBCPawn::OnLook);
//			EIC->BindAction(IA_Look, ETriggerEvent::Completed, this, &ANBCPawn::OnLook);
//			EIC->BindAction(IA_Look, ETriggerEvent::Canceled, this, &ANBCPawn::OnLook);
//		}
//	}
//}
//void ANBCPawn::OnMove(const FInputActionValue& Value)
//{
//	// Expecting Axis2D: X=Right(-1..1), Y=Forward(-1..1)
//	MoveInput = Value.Get<FVector2D>();
//}
//
//
//void ANBCPawn::OnLook(const FInputActionValue& Value)
//{
//	LookInput = Value.Get<FVector2D>();
//}
//
//
//void ANBCPawn::Tick(float DeltaSeconds)
//{
//	Super::Tick(DeltaSeconds);
//
//
//	// --- Movement (로컬 좌표계 기준) ---
//	if (!MoveInput.IsNearlyZero())
//	{
//		FVector LocalOffset = FVector(MoveInput.Y * MoveSpeed * DeltaSeconds, MoveInput.X * MoveSpeed * DeltaSeconds, 0.f);
//		AddActorLocalOffset(LocalOffset, true);
//	}
//
//
//	// --- Rotation  ---
//	if (FMath::Abs(LookInput.X) > KINDA_SMALL_NUMBER)
//	{
//		float YawDelta = LookInput.X * RotationSpeed * DeltaSeconds;
//		AddActorLocalRotation(FRotator(0.f, YawDelta, 0.f)); // local yaw
//	}
//
//
//	if (FMath::Abs(LookInput.Y) > KINDA_SMALL_NUMBER)
//	{
//		float PitchDelta = (bInvertY ? 1.f : -1.f) * LookInput.Y * RotationSpeed * DeltaSeconds;
//		CameraPitch = FMath::Clamp(CameraPitch + PitchDelta, MinPitch, MaxPitch);
//		SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, 0.f));
//	}
//}


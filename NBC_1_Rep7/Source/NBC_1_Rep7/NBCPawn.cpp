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

// Sets default values
ANBCPawn::ANBCPawn()
{
	PrimaryActorTick.bCanEverTick = true;

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

	// 자동 player 인식기능 // GameMode로 대체
	/*AutoPossessPlayer = EAutoReceiveInput::Player0;*/
}

// Called when the game starts or when spawned
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

	SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, 0.f));
}

void ANBCPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
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
	}
}
void ANBCPawn::OnMove(const FInputActionValue& Value)
{
	// Expecting Axis2D: X=Right(-1..1), Y=Forward(-1..1)
	MoveInput = Value.Get<FVector2D>();
}


void ANBCPawn::OnLook(const FInputActionValue& Value)
{
	LookInput = Value.Get<FVector2D>();
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


	// --- Rotation  ---
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
}


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
#include "DrawDebugHelpers.h"

ANBCPawn::ANBCPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // Capsule root
    CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComp"));
    CapsuleComp->InitCapsuleSize(34.f, 88.f);
    RootComponent = CapsuleComp;

    // Collision defaults (capsule handles collisions)
    CapsuleComp->SetSimulatePhysics(false);
    CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CapsuleComp->SetCollisionObjectType(ECC_Pawn);
    CapsuleComp->SetCollisionResponseToAllChannels(ECR_Ignore);
    CapsuleComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    CapsuleComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

    // Mesh
    MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetSimulatePhysics(false);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // SpringArm & Camera
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 300.f;

    SpringArm->bUsePawnControlRotation = true;
    SpringArm->bInheritPitch = true;
    SpringArm->bInheritYaw = true;
    SpringArm->bInheritRoll = false;

    CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
    CameraComp->SetupAttachment(SpringArm);

    // 기본설정값
    MoveSpeedGround = 600.f;
    MoveSpeedAir = 300.f;
    CurrentMoveSpeed = MoveSpeedGround;
    VerticalSpeed = 400.f;
    RotationSpeed = 120.f;
    MinPitch = -80.f;
    MaxPitch = 80.f;
    bInvertY = false;
    CameraPitch = -10.f;

    TiltSpeed = 90.f;
    MaxTiltAngle = 25.f;

    VerticalVelocity = 0.f;
    bIsGrounded = false;

    SpringArmRollTarget = 0.f;
    SpringArmRollCurrent = 0.f;

    AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void ANBCPawn::BeginPlay()
{
    Super::BeginPlay();

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

        FRotator CR = PC->GetControlRotation();
        CR.Pitch = CameraPitch;
        PC->SetControlRotation(CR);
    }

    SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, SpringArmRollCurrent));
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

void ANBCPawn::UpdateGroundedState()
{
    bIsGrounded = false;
    if (GetWorld() && CapsuleComp)
    {
        FVector ActorLoc = GetActorLocation();
        float CapsuleHalf = CapsuleComp->GetScaledCapsuleHalfHeight();
        FVector TraceStart = ActorLoc;
        FVector TraceEnd = ActorLoc - FVector(0.f, 0.f, CapsuleHalf + GroundProbeDistance + 1.f);

        FHitResult Hit;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(this);

        if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
        {
            bIsGrounded = true;
        }
    }
}

void ANBCPawn::OnMove(const FInputActionValue& Value)
{
    MoveInput = Value.Get<FVector2D>();

    float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;

    CurrentMoveSpeed = bIsGrounded ? MoveSpeedGround : MoveSpeedAir;

    if (!MoveInput.IsNearlyZero() && Delta > 0.f)
    {
        FVector LocalOffset = FVector(MoveInput.Y * CurrentMoveSpeed * Delta, MoveInput.X * CurrentMoveSpeed * Delta, 0.f);
        AddActorLocalOffset(LocalOffset, true); // sweep=true
    }
}

void ANBCPawn::OnLook(const FInputActionValue& Value)
{
    LookInput = Value.Get<FVector2D>();

    float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
    float YawDelta = LookInput.X * RotationSpeed * Delta;
    float PitchDelta = (bInvertY ? 1.f : -1.f) * LookInput.Y * RotationSpeed * Delta;

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        FRotator ControlRot = PC->GetControlRotation();
        ControlRot.Yaw += YawDelta;
        ControlRot.Pitch = FMath::Clamp(ControlRot.Pitch + PitchDelta, MinPitch, MaxPitch);
        PC->SetControlRotation(ControlRot);

        FRotator ActorRot = GetActorRotation();
        ActorRot.Yaw = ControlRot.Yaw;
        SetActorRotation(FRotator(ActorRot.Pitch, ActorRot.Yaw, ActorRot.Roll));

        CameraPitch = ControlRot.Pitch;
    }
    else
    {
        if (YawDelta != 0.f)
        {
            AddActorLocalRotation(FRotator(0.f, YawDelta, 0.f));
        }
        CameraPitch = FMath::Clamp(CameraPitch + PitchDelta, MinPitch, MaxPitch);
    }

    SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, SpringArmRollCurrent));

    if (MeshComp)
    {
        FRotator MeshRel = MeshComp->GetRelativeRotation();
        MeshRel.Pitch = CameraPitch;
        MeshRel.Roll = SpringArmRollCurrent; 
        MeshComp->SetRelativeRotation(MeshRel);
    }
}

void ANBCPawn::OnTilt(const FInputActionValue& Value)
{
    // Axis1D: Q=-1, E=+1
    TiltInput = Value.Get<float>();

    SpringArmRollTarget = FMath::Clamp(TiltInput * MaxTiltAngle, -MaxTiltAngle, MaxTiltAngle);

}

void ANBCPawn::OnAscend(const FInputActionValue& Value)
{
    VerticalInput = Value.Get<float>();

    float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
    if (FMath::Abs(VerticalInput) > KINDA_SMALL_NUMBER && Delta > 0.f)
    {
        FVector LocalUp = FVector(0.f, 0.f, VerticalInput * VerticalSpeed * Delta);
        AddActorLocalOffset(LocalUp, true);
        VerticalVelocity = 0.f; 
    }
}

void ANBCPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateGroundedState();

    if (FMath::Abs(VerticalInput) <= KINDA_SMALL_NUMBER)
    {
        if (!bIsGrounded)
        {
            VerticalVelocity += Gravity * DeltaSeconds;
        }
        else if (VerticalVelocity < 0.f)
        {
            VerticalVelocity = 0.f;
        }
    }

    if (!FMath::IsNearlyZero(VerticalVelocity))
    {
        FVector LocalVertical = FVector(0.f, 0.f, VerticalVelocity * DeltaSeconds);
        AddActorLocalOffset(LocalVertical, true);
    }

    SpringArmRollCurrent = FMath::FInterpTo(SpringArmRollCurrent, SpringArmRollTarget, DeltaSeconds, TiltSpeed * 0.1f);

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        FRotator CR = PC->GetControlRotation();
        SetActorRotation(FRotator(0.f, CR.Yaw, 0.f));
    }

    FRotator NewActorRot = GetActorRotation();
    NewActorRot.Yaw = GetControlRotation().Yaw;  
    NewActorRot.Roll = SpringArmRollCurrent; 
    SetActorRotation(NewActorRot);

    //SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, 0.f));

    SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, SpringArmRollCurrent));

    if (MeshComp)
    {
        FRotator MeshRel = MeshComp->GetRelativeRotation();
        MeshRel.Pitch = CameraPitch;
        MeshRel.Roll = SpringArmRollCurrent;
        MeshComp->SetRelativeRotation(MeshRel);
    }

#if WITH_EDITOR
    if (GetWorld() && CapsuleComp)
    {
        FVector ActorLoc = GetActorLocation();
        float CapsuleHalf = CapsuleComp->GetScaledCapsuleHalfHeight();
        FVector TraceStart = ActorLoc;
        FVector TraceEnd = ActorLoc - FVector(0.f, 0.f, CapsuleHalf + GroundProbeDistance + 1.f);
        DrawDebugLine(GetWorld(), TraceStart, TraceEnd, bIsGrounded ? FColor::Green : FColor::Red, false, 0.02f);
    }
#endif
}

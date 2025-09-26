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

    // We'll manually control springarm pitch & roll (so disable using pawn control rotation)
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bInheritPitch = false;
    SpringArm->bInheritYaw = false;
    SpringArm->bInheritRoll = false;

    CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
    CameraComp->SetupAttachment(SpringArm);

    // Defaults
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

    // Register IMC to EnhancedInput subsystem
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

    // Initialize springarm rotation with initial pitch & roll
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
    // Axis2D: X=Right, Y=Forward (set in IMC)
    MoveInput = Value.Get<FVector2D>();

    float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;

    // Choose speed depending on grounded state
    CurrentMoveSpeed = bIsGrounded ? MoveSpeedGround : MoveSpeedAir;

    if (!MoveInput.IsNearlyZero() && Delta > 0.f)
    {
        // Local: X=Forward, Y=Right (we map Val.Y->forward, Val.X->right)
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

    // Update Controller rotation so camera view follows mouse
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        FRotator ControlRot = PC->GetControlRotation();
        ControlRot.Yaw += YawDelta;
        ControlRot.Pitch = FMath::Clamp(ControlRot.Pitch + PitchDelta, MinPitch, MaxPitch);
        PC->SetControlRotation(ControlRot);

        // Keep Pawn yaw aligned with control yaw (so movement forward matches view)
        FRotator ActorRot = GetActorRotation();
        ActorRot.Yaw = ControlRot.Yaw;
        SetActorRotation(FRotator(ActorRot.Pitch, ActorRot.Yaw, ActorRot.Roll));

        // store camera pitch for springarm/mesh visual sync
        CameraPitch = ControlRot.Pitch;
    }
    else
    {
        // fallback
        if (YawDelta != 0.f)
        {
            AddActorLocalRotation(FRotator(0.f, YawDelta, 0.f));
        }
        CameraPitch = FMath::Clamp(CameraPitch + PitchDelta, MinPitch, MaxPitch);
    }

    // Apply camera pitch and current springarm roll to springarm
    SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, SpringArmRollCurrent));

    // Sync mesh pitch visually as well
    if (MeshComp)
    {
        FRotator MeshRel = MeshComp->GetRelativeRotation();
        MeshRel.Pitch = CameraPitch; // adjust scale if needed (e.g. *0.6f)
        MeshComp->SetRelativeRotation(MeshRel);
    }
}

void ANBCPawn::OnTilt(const FInputActionValue& Value)
{
    // Axis1D: Q=-1, E=+1
    TiltInput = Value.Get<float>();

    // Determine target roll for springarm (visual). For natural feel, we invert sign:
    // when TiltInput is positive (E), tilt right; negative (Q) tilt left.
    SpringArmRollTarget = FMath::Clamp(-TiltInput * MaxTiltAngle, -MaxTiltAngle, MaxTiltAngle);

    // We do NOT rotate the Actor itself — only visual tilt on springarm and mesh.
    // Mesh will be set in Tick() to inverse/adjust to springarm roll for nicer look.
}

void ANBCPawn::OnAscend(const FInputActionValue& Value)
{
    VerticalInput = Value.Get<float>();

    // If user is actively ascending/descending, immediately apply local vertical offset
    float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
    if (FMath::Abs(VerticalInput) > KINDA_SMALL_NUMBER && Delta > 0.f)
    {
        FVector LocalUp = FVector(0.f, 0.f, VerticalInput * VerticalSpeed * Delta);
        AddActorLocalOffset(LocalUp, true);
        VerticalVelocity = 0.f; // override gravity while input active
    }
}

void ANBCPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 1) Update grounded state first (used by movement speed)
    UpdateGroundedState();

    // 2) Gravity (only when not actively ascending/descending)
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

    // Apply vertical velocity via local offset (sweep)
    if (!FMath::IsNearlyZero(VerticalVelocity))
    {
        FVector LocalVertical = FVector(0.f, 0.f, VerticalVelocity * DeltaSeconds);
        AddActorLocalOffset(LocalVertical, true);
    }

    // 3) Visual tilt smoothing: interpolate current roll towards target
    SpringArmRollCurrent = FMath::FInterpTo(SpringArmRollCurrent, SpringArmRollTarget, DeltaSeconds, 6.f); // interp speed 6

    // Apply to springarm (pitch from CameraPitch, roll from SpringArmRollCurrent)
    SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, SpringArmRollCurrent));

    // Mesh: apply pitch and a counter or scaled roll for nicer visuals (here we give inverse roll)
    if (MeshComp)
    {
        FRotator MeshRel = MeshComp->GetRelativeRotation();
        MeshRel.Pitch = CameraPitch;
        MeshRel.Roll = -SpringArmRollCurrent * 1.0f; // negative so mesh leans into turn visually
        MeshComp->SetRelativeRotation(MeshRel);
    }

    // Debug draw ground probe (optional)
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

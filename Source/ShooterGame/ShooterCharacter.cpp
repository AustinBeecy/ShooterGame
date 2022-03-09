// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterCharacter.h"
#include "GameFramework\SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet\GameplayStatics.h"
#include "Sound\SoundCue.h"
#include "Engine/SkeletalMeshSocket.h"
#include "DrawDebugHelpers.h"
#include "Particles/ParticleSystemComponent.h"

// Sets default values
AShooterCharacter::AShooterCharacter() :
	BaseTurnRate(45.f),
	BaseLookUpRate(45.f)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//Create a camera boom (pulls in twords the character if there is a colision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom")); 
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.f; // camera follows at this distance behind character 
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller 
	CameraBoom->SocketOffset = FVector(0.f, 50.f, 50.f); // camera offset for aiming ** may want to tweek values

	//Create Follow Camera 
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // attach camera to end of boom
	FollowCamera->bUsePawnControlRotation = false; // Camera Does not rotate 

	//Dont rotate with controller
	bUseControllerRotationPitch = false; 
	bUseControllerRotationYaw = true; 
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false; // character moves in the direction of input 
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f); // at this rotation rate

	GetCharacterMovement()->JumpZVelocity = 600.f; 
	GetCharacterMovement()->AirControl = 0.2f; 
}

// Called when the game starts or when spawned
void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	

}

void AShooterCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f)) {
		// find out which way is forward
		const FRotator Rotation{Controller->GetControlRotation()};
		const FRotator YawRotation{ 0, Rotation.Yaw, 0 };

		const FVector Direction{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::X)}; 
		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::MoveRight(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f)) {
		// find out which way is forward
		const FRotator Rotation{ Controller->GetControlRotation() };
		const FRotator YawRotation{ 0, Rotation.Yaw, 0 };

		const FVector Direction{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::Y) };
		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::TurnAtRate(float Rate)
{
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds()); // deg/sec * sec/frame = deg/frame
}

void AShooterCharacter::LookUpAtRate(float Rate)
{
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds()); // deg/sec * sec/frame = deg/frame
}

void AShooterCharacter::FireWeapon()
{
	if (FireSound) {
		UGameplayStatics::PlaySound2D(this, FireSound); 
	}
	const USkeletalMeshSocket* BarrelSocket = GetMesh()->GetSocketByName("barrelSocket");
	if (BarrelSocket) {
		const FTransform SocketTransform = BarrelSocket->GetSocketTransform(GetMesh());

		if (MuzzleFlash) {
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);
		}

		//Get current size of the viewport 
		FVector2D ViewportSize;

		if (GEngine && GEngine->GameViewport) {
			GEngine->GameViewport->GetViewportSize(ViewportSize);
		}
		
		//get screen space location of crosshairs
		FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
		CrosshairLocation.Y -= 50.f;
		FVector CrosshairWorldPosition; 
		FVector CrosshairWorldDirection;

		//get world pos and direction of crosshair 
		bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(UGameplayStatics::GetPlayerController(this, 0),
			CrosshairLocation, CrosshairWorldPosition, CrosshairWorldDirection); 

		if (bScreenToWorld) // if depprojection was succefull 
		{
			FHitResult ScreenTraceHit; 
			const FVector Start{ CrosshairWorldPosition }; 
			const FVector End{ CrosshairWorldDirection * 50'000.f }; 

			//set beam end point to line trace point
			FVector BeamEndPoint{ End }; 

			// Trace outward from crosshairs world location 
			GetWorld()->LineTraceSingleByChannel(
				ScreenTraceHit, 
				Start, 
				End, 
				ECollisionChannel::ECC_Visibility); 

			if (ScreenTraceHit.bBlockingHit) // was thier a trace hit 
			{
				//beamEndPoint is now traceHit location 
				BeamEndPoint = ScreenTraceHit.Location; 
				
			}

			//perform secod trace this time from the gun barrel 
			FHitResult WeaponTraceHit; 
			const FVector WeaponTraceStart{ SocketTransform.GetLocation() }; 
			const FVector WeaponTraveEnd{ BeamEndPoint }; 
			GetWorld()->LineTraceSingleByChannel(
				WeaponTraceHit,
				WeaponTraceStart,
				WeaponTraveEnd,
				ECollisionChannel::ECC_Visibility); 
			if (WeaponTraceHit.bBlockingHit) {

				BeamEndPoint = WeaponTraceHit.Location; 
			}
			
			if (ImpactParticles) {
				UGameplayStatics::SpawnEmitterAtLocation(
					GetWorld(),
					ImpactParticles,
					BeamEndPoint);
			}

			if (BeamParticles) {
				UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BeamParticles, SocketTransform);
				if (Beam) {
					Beam->SetVectorParameter(FName("Target"), BeamEndPoint);
				}
			}
		}

	

		/*
		FHitResult FireHit; 
		const FVector Start{ SocketTransform.GetLocation() }; 
		const FQuat Rotation{ SocketTransform.GetRotation() };
		const FVector RotationAxis{ Rotation.GetAxisX() };
		const FVector End{ Start + RotationAxis * 50'000.f };

		FVector BeamEndPoint{ End };

		GetWorld()->LineTraceSingleByChannel(FireHit, Start, End, ECollisionChannel::ECC_Visibility);
		if (FireHit.bBlockingHit) {
			//DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 2.f);
			//DrawDebugPoint(GetWorld(), FireHit.Location, 5.f, FColor::Red, false, 2.f);

			BeamEndPoint = FireHit.Location;  

			if (ImpactParticles) {
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, FireHit.Location);
			}
		}
		if (BeamParticles) {
			UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BeamParticles, SocketTransform);
			if (Beam) {
				Beam->SetVectorParameter(FName("Target"), BeamEndPoint);
			}
		}
		*/


	}
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); 
	if (AnimInstance && HipFireMontage)
	{
		AnimInstance->Montage_Play(HipFireMontage);
		AnimInstance->Montage_JumpToSection(FName("StartFire"));
	}
}

// Called every frame
void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShooterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("TurnRate", this, &AShooterCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AShooterCharacter::LookUpAtRate);
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAction("FireButton", IE_Pressed, this, &AShooterCharacter::FireWeapon);
}


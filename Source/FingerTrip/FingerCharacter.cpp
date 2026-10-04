// Fill out your copyright notice in the Description page of Project Settings.


#include "FingerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h" // GetWorld()->GetTimeSeconds() 사용을 위해 포함
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

// Sets default values
AFingerCharacter::AFingerCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Score = 0;
	bIsLeftPressed = false;
	bIsRightPressed = false;
	LastLeftClickTime = -1.0f; // 초기값 -1 (유효하지 않은 시간)
	LastRightClickTime = -1.0f; // 초기값 -1 (유효하지 않은 시간)
	RhythmWindowTolerance = 0.2f; // 기본 타이밍 허용 오차 (C++ 디폴트 0.2s, 인스턴스에서 0.5s 사용)
	bIsWalkingRhythmically = false;
	bNextStepIsLeft = true;

	// --- 가속도 시스템 변수 초기화 ---
	DefaultMaxWalkSpeed = 100.0f;
	ConsecutiveRhythmHits = 0;
	MaxConsecutiveRhythmHits = 15; // 12번 성공 시 최대 속도
	MinSpeedMultiplier = 1.0f;     // 최소 속도 배율
	MaxSpeedMultiplier = 3.6f;     // 최대 속도 배율
	CurrentMovementSpeedMultiplier = MinSpeedMultiplier;

	// --- 클리어 조건 관련 변수 초기화 ---
	MaxScore = 8;
	bIsSpecialCoinCollected = false;

	SpecialCoinScore = 0; // 스페셜 코인 획득 개수 초기화
	TargetSpecialCoins = 3; // 3별을 위한 스페셜 코인 목표 개수 3개

	LevelMaxTime = 600.0f; // 3분 (180초)으로 설정
	TimeRemaining = LevelMaxTime;

	bStar1Achieved = false;
	bStar2Achieved = false;
	bStar3Achieved = false;

	// --- 점프력 관련 변수 초기화 ---
	DefaultJumpZVelocity = 800.0f; // BeginPlay에서 실제 값을 가져올 예정
	MaxJumpZVelocityMultiplier = 1.5f; // 최대 속도일 때 점프력이 1.5배 증가하도록 설정 (조절 가능)

	// --- 게이지 초기화 ---
	InitialGauge = 0.0f;
	MaxGauge = 100.0f;
	CurrentGauge = 0.0f;
	GaugeGainPerHit = 10.0f;
	GaugeDrainRate = 20.0f;

	// --- 아이템 시스템 초기화 ---
	ItemSpeedMultiplier = 1.0f;
	ItemJumpMultiplier = 1.0f;
	DefaultDashStrength = 2200.0f;
	bIsAirWalking = false;

	bIsLevelCleared = false;
	bHasGameEnded = false;
}

// Called when the game starts or when spawned
void AFingerCharacter::BeginPlay()
{
	Super::BeginPlay();

	CurrentGauge = FMath::Clamp(InitialGauge, 0.0f, MaxGauge);

	// 블루프린트에서 설정된 최종 MaxWalkSpeed를 저장
	DefaultMaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;

	// CharacterMovementComponent의 JumpZVelocity 기본값을 저장
	DefaultJumpZVelocity = GetCharacterMovement()->JumpZVelocity;

	// --- 게임 타이머 시작 ---
	// 1초마다 UpdateGameTimer 함수 호출
	GetWorldTimerManager().SetTimer(GameTimerHandle, this, &AFingerCharacter::UpdateGameTimer, 1.0f, true);

	// 초기 속도 배율 적용
	UpdateMovementSpeed();

	// --- 카메라 위아래 각도 제한 설정 ---
	if (APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		CameraManager->ViewPitchMin = -60.0f; // 아래로 최대 60도
		CameraManager->ViewPitchMax = 50.0f;  // 위로 최대 50도 (아래에서 위를 더 볼 수 있게 완화)
	}

	if (USpringArmComponent* SpringArmComp = FindComponentByClass<USpringArmComponent>())
	{
		TargetZoomLength = SpringArmComp->TargetArmLength;
	}
}

void AFingerCharacter::AddGauge(float Amount)
{
	CurrentGauge = FMath::Clamp(CurrentGauge + Amount, 0.0f, MaxGauge);
}

// Called every frame
void AFingerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsWalkingRhythmically)
	{
		AddMovementInput(GetActorForwardVector(), 1.0f);
	}

	if (GEngine)
	{
		// FString GaugeMsg = FString::Printf(TEXT("Gauge: %.1f / %.1f %s"),
		// 	CurrentGauge, MaxGauge, bIsWallWalking ? TEXT("[WALL WALKING]") : TEXT(""));
		// GEngine->AddOnScreenDebugMessage(2, 0.1f, bIsWallWalking ? FColor::Yellow : FColor::Green, GaugeMsg);
	}

	// --- 3D 자유 시점 / 백뷰 고정(조향) 전환 로직 ---
	USpringArmComponent* SpringArmComp = FindComponentByClass<USpringArmComponent>();
	if (SpringArmComp)
	{
		// 부드러운 줌 보간
		SpringArmComp->TargetArmLength = FMath::FInterpTo(SpringArmComp->TargetArmLength, TargetZoomLength, DeltaTime, ZoomInterpSpeed);

		float CurrentSpeed = GetVelocity().Size();
		bool bIsMoving = CurrentSpeed > 10.0f;

		// 마우스에 캐릭터가 확확 돌아가지 않도록 항상 false 유지 (대신 AddCameraYaw에서 직접 부드럽게 조향)
		bUseControllerRotationYaw = false; 
		
		// 카메라는 항상 언리얼의 부드러운 기본 시스템(ControlRotation)을 따름 (월드 좌표계 기준 유지)
		SpringArmComp->bUsePawnControlRotation = true;

		if (bIsMoving)
		{
			// 이동 상태: 카메라는 무조건 등 뒤(백뷰)를 향해 스무스하게 보간하되,
			// Pitch와 Roll은 고정하여 벽을 걸어도 카메라는 항상 정상적인 똑바로 서있는 시점을 유지함
			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				FRotator CurrentControlRot = PC->GetControlRotation();
				
				// 카메라의 타겟은 항상 캐릭터의 현재 방향(조향 방향)의 Yaw만 따름
				FRotator TargetRot = GetActorRotation();
				TargetRot.Pitch = CurrentControlRot.Pitch; // 수직 시점(위아래)은 유저가 조작한 현재 상태 유지
				TargetRot.Roll = DefaultBackViewRotation.Roll;

				// 자유시점에서 백뷰로 스르륵 따라오는 스무스 보간
				FRotator NewControlRot = FMath::RInterpTo(CurrentControlRot, TargetRot, DeltaTime, CameraReturnInterpSpeed);
				PC->SetControlRotation(NewControlRot);
			}
		}
	}
}

// Called to bind functionality to input
void AFingerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("LeftClick", IE_Pressed, this, &AFingerCharacter::OnLeftClick);
	PlayerInputComponent->BindAction("RightClick", IE_Pressed, this, &AFingerCharacter::OnRightClick);

	PlayerInputComponent->BindAction("LeftClick", IE_Released, this, &AFingerCharacter::OnLeftRelease);
	PlayerInputComponent->BindAction("RightClick", IE_Released, this, &AFingerCharacter::OnRightRelease);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AFingerCharacter::Jump);
	PlayerInputComponent->BindAction("UseItem", IE_Pressed, this, &AFingerCharacter::UseCurrentItem);

	PlayerInputComponent->BindAxis("MoveForward", this, &AFingerCharacter::MoveForward);
	
	// 원래 입력 방식 대신 커스텀 함수를 바인딩하여 마우스 입력 발생 시간을 기록합니다.
	PlayerInputComponent->BindAxis("LookRight", this, &AFingerCharacter::AddCameraYaw);
	PlayerInputComponent->BindAxis("LookUp", this, &AFingerCharacter::AddCameraPitch);
	
	PlayerInputComponent->BindAxis("Zoom", this, &AFingerCharacter::ZoomCamera);
}

// --- 카메라 및 캐릭터 조향 로직 ---
void AFingerCharacter::ZoomCamera(float AxisValue)
{
	if (AxisValue != 0.0f)
	{
		float CurrentSpeed = GetVelocity().Size();
		if (CurrentSpeed <= 10.0f) // 캐릭터가 멈춰있을 때만
		{
			TargetZoomLength = FMath::Clamp(TargetZoomLength + (AxisValue * -ZoomSpeed), MinZoom, MaxZoom);
		}
	}
}

void AFingerCharacter::AddCameraYaw(float AxisValue)
{
	if (FMath::Abs(AxisValue) > 0.05f)
	{
		float CurrentSpeed = GetVelocity().Size();
		if (CurrentSpeed > 10.0f)
		{
			// 1. 이동 중: 캐릭터를 좌우로 조향합니다. (SteeringSensitivity로 감도 조절 가능)
			// 너무 확확 꺾이는 것을 방지하기 위해 감도 계수를 곱해줍니다.
			AddActorLocalRotation(FRotator(0.0f, AxisValue * SteeringSensitivity, 0.0f));
		}
		else
		{
			// 2. 정지 상태: 마우스로 완전 자유 시점(카메라만 회전)
			// 언리얼 카메라의 기본 배율(보통 2.5) 때문에 카메라가 2.5배 더 빨리 돕니다.
			// 이를 상쇄해서 캐릭터 조향(SteeringSensitivity)과 완벽하게 똑같은 감도를 맞춥니다.
			AddControllerYawInput((AxisValue * SteeringSensitivity) / 2.5f);
		}
	}
}

void AFingerCharacter::AddCameraPitch(float AxisValue)
{
	if (FMath::Abs(AxisValue) > 0.05f)
	{
		// 정지, 이동 상관없이 항상 카메라 위아래 회전을 허용합니다.
		AddControllerPitchInput((AxisValue * SteeringSensitivity) / 2.5f);
	}
}


void AFingerCharacter::OnLeftClick()
{
	bIsLeftPressed = true;
	LastLeftClickTime = UGameplayStatics::GetTimeSeconds(GetWorld());

	if (!bIsWalkingRhythmically)
	{
		// 캐릭터가 거의 완전히 멈췄을 때만 새 콤보 시작 가능 (광클 꼼수 방지)
		if (GetCharacterMovement()->Velocity.Size2D() < 10.0f)
		{
			bIsWalkingRhythmically = true;
			bStartedWithRightClick = false;
			bNextStepIsLeft = false; // 첫 발은 자동 성공 처리되므로 다음은 오른발
			ConsecutiveRhythmHits = 1;
			UpdateMovementSpeed();
			AddGauge(GaugeGainPerHit);

			// 출발할 때 카메라가 바라보는 방향으로 캐릭터를 즉시 회전시킵니다.
			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				FRotator CamRot = PC->GetControlRotation();
				FRotator CharRot = GetActorRotation();
				CharRot.Yaw = CamRot.Yaw;
				SetActorRotation(CharRot);
			}
		}
	}
	else
	{
		// 걷고 있는데 눌러야 할 발이 오른발인데 좌클릭을 한 경우 즉시 콤보 끊기
		if (!bNextStepIsLeft)
		{
			if (GetCharacterMovement()->IsFalling()) return; // 공중(점프 중)일 때는 실패 무시
			bIsWalkingRhythmically = false;
			ConsecutiveRhythmHits = 0;
			UpdateMovementSpeed();
		}
	}
}

void AFingerCharacter::OnRightClick()
{
	bIsRightPressed = true;
	LastRightClickTime = UGameplayStatics::GetTimeSeconds(GetWorld());

	if (bIsWalkingRhythmically)
	{
		// 눌러야 할 발이 왼발인데 우클릭을 한 경우 즉시 콤보 끊기
		if (bNextStepIsLeft)
		{
			if (GetCharacterMovement()->IsFalling()) return; // 공중(점프 중)일 때는 실패 무시
			bIsWalkingRhythmically = false;
			ConsecutiveRhythmHits = 0;
			UpdateMovementSpeed();
		}
	}
	else
	{
		// 가만히 있을 때 우클릭으로 출발하는 경우
		if (GetCharacterMovement()->Velocity.Size2D() < 10.0f)
		{
			bIsWalkingRhythmically = true;
			bStartedWithRightClick = true;
			bNextStepIsLeft = true; // 우클릭(오른발) 출발 성공, 다음은 왼발
			ConsecutiveRhythmHits = 1;
			UpdateMovementSpeed();
			AddGauge(GaugeGainPerHit);

			// 출발할 때 카메라가 바라보는 방향으로 캐릭터를 즉시 회전시킵니다.
			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				FRotator CamRot = PC->GetControlRotation();
				FRotator CharRot = GetActorRotation();
				CharRot.Yaw = CamRot.Yaw;
				SetActorRotation(CharRot);
			}
		}
	}
}

void AFingerCharacter::OnLeftRelease()
{
	bIsLeftPressed = false;
}

void AFingerCharacter::OnRightRelease()
{
	bIsRightPressed = false;
}

void AFingerCharacter::OnLeftFootDown()
{
	float CurrentTime = UGameplayStatics::GetTimeSeconds(GetWorld());

	// 속도 배율에 따른 단순 반비례 동적 판정 (정확히 1걸음/1주기 이내의 선입력만 허용)
	// 예: 1.0배속->500ms(1주기), 2.0배속->250ms(1주기), 3.6배속(MAX)->139ms(1주기)
	float EffectiveTolerance = RhythmWindowTolerance / FMath::Max(CurrentMovementSpeedMultiplier, 1.0f);

	// 첫 이동 시작에 의한 왼발 딛기면 타이밍 체크 패스 (좌클릭 출발 시)
	if (bIsWalkingRhythmically && ConsecutiveRhythmHits == 1 && !bStartedWithRightClick) 
	{
		ConsecutiveRhythmHits = 2;
		UpdateMovementSpeed();
		AddGauge(GaugeGainPerHit);
		
		// 다음 발(오른발)을 위한 이펙트 스폰
		SpawnRhythmEffect(true);
		return;
	}

	if (bIsWalkingRhythmically && (CurrentTime - LastLeftClickTime <= EffectiveTolerance))
	{
		LastLeftClickTime = -1.0f;
		ConsecutiveRhythmHits = FMath::Min(ConsecutiveRhythmHits + 1, MaxConsecutiveRhythmHits);
		UpdateMovementSpeed();
		AddGauge(GaugeGainPerHit);
		bNextStepIsLeft = false; 

		SpawnRhythmEffect(true);
	}
	else if (bIsWalkingRhythmically)
	{
		if (GetCharacterMovement()->IsFalling()) return; // 공중(점프 중)일 때는 애니메이션 노티파이에 의한 실패 무시

		bIsWalkingRhythmically = false;
		ConsecutiveRhythmHits = 0; 
		UpdateMovementSpeed(); 
	}
}

void AFingerCharacter::OnRightFootDown()
{
	float CurrentTime = UGameplayStatics::GetTimeSeconds(GetWorld());

	// 속도 배율에 따른 단순 반비례 동적 판정 (정확히 1걸음/1주기 이내의 선입력만 허용)
	float EffectiveTolerance = RhythmWindowTolerance / FMath::Max(CurrentMovementSpeedMultiplier, 1.0f);
	
	// 우클릭 출발 시 첫 발(오른발) 타이밍 체크 패스
	if (bIsWalkingRhythmically && ConsecutiveRhythmHits == 1 && bStartedWithRightClick) 
	{
		ConsecutiveRhythmHits = 2;
		UpdateMovementSpeed(); 
		AddGauge(GaugeGainPerHit);
		
		// 다음 발(왼발)을 위한 이펙트 스폰
		SpawnRhythmEffect(false);
		return;
	}

	if (bIsWalkingRhythmically && (CurrentTime - LastRightClickTime <= EffectiveTolerance))
	{
		LastRightClickTime = -1.0f;
		ConsecutiveRhythmHits = FMath::Min(ConsecutiveRhythmHits + 1, MaxConsecutiveRhythmHits);
		UpdateMovementSpeed(); 
		AddGauge(GaugeGainPerHit);
		bNextStepIsLeft = true;

		SpawnRhythmEffect(false);
	}
	else if (bIsWalkingRhythmically)
	{
		if (GetCharacterMovement()->IsFalling()) return; // 공중(점프 중)일 때는 애니메이션 노티파이에 의한 실패 무시

		bIsWalkingRhythmically = false;
		ConsecutiveRhythmHits = 0; 
		UpdateMovementSpeed(); 
	}
}


// --- 새로운 가속도 계산 및 적용 함수 ---
void AFingerCharacter::UpdateMovementSpeed()
{
	// 멤버 변수로 선언된 SpeedMultipliers 배열을 직접 사용
	int32 Index = FMath::Clamp(ConsecutiveRhythmHits, 0, SpeedMultipliers.Num() - 1);

	CurrentMovementSpeedMultiplier = SpeedMultipliers[Index];

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = DefaultMaxWalkSpeed * CurrentMovementSpeedMultiplier * ItemSpeedMultiplier;

		// 2. 점프력(JumpZVelocity) 업데이트 (속도 배율에 비례 및 아이템 버프 반영)
	   // CurrentMovementSpeedMultiplier가 1.0 (최소)일 때 DefaultJumpZVelocity 유지
	   // CurrentMovementSpeedMultiplier가 MaxSpeedMultiplier (3.6)일 때 JumpZVelocity가 DefaultJumpZVelocity * MaxJumpZVelocityMultiplier (1.5)가 되도록 선형 보간

	   // 속도 배율이 MinSpeedMultiplier(1.0) ~ MaxSpeedMultiplier(3.6) 범위 내에서
	   // 점프 배율이 1.0 ~ MaxJumpZVelocityMultiplier(1.5) 범위 내로 보간

		float JumpMultiplier = FMath::Lerp(1.0f, MaxJumpZVelocityMultiplier,
			(CurrentMovementSpeedMultiplier - MinSpeedMultiplier) / (MaxSpeedMultiplier - MinSpeedMultiplier));

		// MinSpeedMultiplier와 MaxSpeedMultiplier가 동일한 경우 (예: 둘 다 1.0인 경우) 나누기 0 방지
		if (FMath::IsNearlyEqual(MaxSpeedMultiplier, MinSpeedMultiplier))
		{
			JumpMultiplier = 1.0f; // 변화 없음
		}
		else
		{
			JumpMultiplier = FMath::Lerp(1.0f, MaxJumpZVelocityMultiplier,
				(CurrentMovementSpeedMultiplier - MinSpeedMultiplier) / (MaxSpeedMultiplier - MinSpeedMultiplier));
		}

		GetCharacterMovement()->JumpZVelocity = DefaultJumpZVelocity * JumpMultiplier * ItemJumpMultiplier;

		/*UE_LOG(LogTemp, Warning, TEXT("Updated MaxWalkSpeed: %.1f (x%.1f), JumpZ: %.1f (x%.1f), Hits: %d"),
			GetCharacterMovement()->MaxWalkSpeed, CurrentMovementSpeedMultiplier,
			GetCharacterMovement()->JumpZVelocity, JumpMultiplier, ConsecutiveRhythmHits);*/

		if (GEngine)
		{
			// FString DebugMsg = FString::Printf(TEXT("Speed Multiplier: x%.1f"), CurrentMovementSpeedMultiplier);
			// 첫 번째 인자로 키 값(1)을 주어 같은 메시지가 화면을 도배하지 않고 갱신되도록 합니다.
			// GEngine->AddOnScreenDebugMessage(1, 3.0f, FColor::Cyan, DebugMsg);
		}
	}
}

void AFingerCharacter::MoveForward(float AxisValue)
{
	if (AxisValue != 0.0f)
	{
		// 개발자 테스트용 치트: WASD 이동 시 최고 속도(최대 콤보 상태)로 설정
		ConsecutiveRhythmHits = SpeedMultipliers.Num() - 1;
		UpdateMovementSpeed();

		AddMovementInput(GetActorForwardVector(), AxisValue);
	}
}

void AFingerCharacter::Jump()
{
	// 공중 걷기 중에는 점프 불가
	if (bIsAirWalking)
	{
		return;
	}

	Super::Jump(); // ACharacter의 기본 Jump 기능을 호출합니다.
	bJumpInputPressed = true; // 점프 입력이 눌렸음을 표시
}

// --- 아이템 시스템 구현 ---
void AFingerCharacter::AcquireItem(const FFingerItemData& NewItemData)
{
	// 이전 아이템이 있어도 새 아이템으로 덮어씀
	CurrentHeldItem = NewItemData;

	if (OnItemChanged.IsBound())
	{
		OnItemChanged.Broadcast(CurrentHeldItem);
	}

	if (GEngine)
	{
		FString Msg = FString::Printf(TEXT("아이템 획득: %s (Shift 키로 사용)"), *CurrentHeldItem.ItemName.ToString());
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, Msg);
	}
}

void AFingerCharacter::UseCurrentItem()
{
	if (CurrentHeldItem.ItemType == EFingerItemType::None)
	{
		return;
	}

	switch (CurrentHeldItem.ItemType)
	{
	case EFingerItemType::SpeedBoost:
		ApplySpeedBoost(CurrentHeldItem.EffectValue > 0.0f ? CurrentHeldItem.EffectValue : 1.6f,
		                CurrentHeldItem.Duration > 0.0f ? CurrentHeldItem.Duration : 5.0f);
		break;

	case EFingerItemType::JumpBoost:
		ApplyJumpBoost(CurrentHeldItem.EffectValue > 0.0f ? CurrentHeldItem.EffectValue : 1.6f,
		               CurrentHeldItem.Duration > 0.0f ? CurrentHeldItem.Duration : 5.0f);
		break;

	case EFingerItemType::Dash:
		ExecuteDash(CurrentHeldItem.EffectValue > 0.0f ? CurrentHeldItem.EffectValue : DefaultDashStrength);
		break;

	case EFingerItemType::AirWalk:
		StartAirWalk(CurrentHeldItem.Duration > 0.0f ? CurrentHeldItem.Duration : 3.0f);
		break;

	default:
		break;
	}

	// 사용 후 소모 (None으로 초기화)
	CurrentHeldItem = FFingerItemData();
	if (OnItemChanged.IsBound())
	{
		OnItemChanged.Broadcast(CurrentHeldItem);
	}
}

// 1. 이동속도 버프
void AFingerCharacter::ApplySpeedBoost(float Multiplier, float Duration)
{
	ItemSpeedMultiplier = Multiplier;
	UpdateMovementSpeed();

	GetWorldTimerManager().ClearTimer(SpeedBuffTimerHandle);
	GetWorldTimerManager().SetTimer(SpeedBuffTimerHandle, this, &AFingerCharacter::EndSpeedBoost, Duration, false);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("이동속도 증가 버프 적용!"));
	}
}

void AFingerCharacter::EndSpeedBoost()
{
	ItemSpeedMultiplier = 1.0f;
	UpdateMovementSpeed();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("이동속도 증가 버프 종료"));
	}
}

// 2. 점프력 버프
void AFingerCharacter::ApplyJumpBoost(float Multiplier, float Duration)
{
	ItemJumpMultiplier = Multiplier;
	UpdateMovementSpeed();

	GetWorldTimerManager().ClearTimer(JumpBuffTimerHandle);
	GetWorldTimerManager().SetTimer(JumpBuffTimerHandle, this, &AFingerCharacter::EndJumpBoost, Duration, false);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("점프력 증가 버프 적용!"));
	}
}

void AFingerCharacter::EndJumpBoost()
{
	ItemJumpMultiplier = 1.0f;
	UpdateMovementSpeed();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("점프력 증가 버프 종료"));
	}
}

// 3. 대쉬
void AFingerCharacter::ExecuteDash(float Strength)
{
	FVector DashVelocity = GetActorForwardVector() * Strength;

	// 공중(점프 중)일 때 낙하 관성을 끊고 앞쪽으로 쭉 뻗어나가며 살짝 띄워줌
	if (GetCharacterMovement()->IsFalling())
	{
		DashVelocity.Z = 250.0f;
	}

	LaunchCharacter(DashVelocity, true, true);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan, TEXT("대쉬 사용!"));
	}
}

// 4. 공중 걷기 (3초)
void AFingerCharacter::StartAirWalk(float Duration)
{
	bIsAirWalking = true;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		FVector Vel = MoveComp->Velocity;
		Vel.Z = 0.0f;
		MoveComp->Velocity = Vel;
		MoveComp->SetMovementMode(MOVE_Flying);
	}

	GetWorldTimerManager().ClearTimer(AirWalkTimerHandle);
	GetWorldTimerManager().SetTimer(AirWalkTimerHandle, this, &AFingerCharacter::EndAirWalk, Duration, false);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, FColor(200, 100, 255), TEXT("공중 걷기 시작! (점프 불가)"));
	}
}

void AFingerCharacter::EndAirWalk()
{
	if (!bIsAirWalking) return;

	bIsAirWalking = false;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Falling);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor(200, 100, 255), TEXT("공중 걷기 종료"));
	}
}

void AFingerCharacter::StopJumping()
{
	Super::StopJumping();
}

void AFingerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit); // ACharacter의 기본 Landed 기능을 호출합니다.
	bJumpInputPressed = false; // 착지했으므로 점프 입력 상태를 리셋 
	
	// 착지 시 바니합(연속 점프) 꼼수 방지를 위해 리듬 콤보 초기화
	bIsWalkingRhythmically = false;
	ConsecutiveRhythmHits = 0;
	UpdateMovementSpeed();
}

// 점수 시스템
void AFingerCharacter::AddScore(int32 ScoreToAdd)
{
	Score++;
	UE_LOG(LogTemp, Warning, TEXT("Current Score: %d"), Score); // 디버깅을 위해 로그 출력

	// 모든 코인을 획득했을 때 클리어 조건 확인
	if (Score >= MaxScore)
	{
		CheckClearConditions();
	}
}

// --- 스페셜 코인 점수 추가 함수 (개수 증가) ---
void AFingerCharacter::AddSpecialCoinScore(int32 SpecialScoreToAdd)
{
	SpecialCoinScore++; // 스페셜 코인 개수 증가
	// UE_LOG(LogTemp, Warning, TEXT("Special Coins: %d / %d"), SpecialCoinScore, TargetSpecialCoins); // 디버그 로그는 UI로 대체

	// 스페셜 코인 개수가 목표에 도달했을 때 클리어 조건 확인
	if (SpecialCoinScore >= TargetSpecialCoins && !bHasGameEnded)
	{
		bStar3Achieved = true; // 이 플래그는 CheckClearConditions에서만 설정하도록 변경
		//CheckClearConditions(); // 스페셜 코인 목표 달성 시 클리어 조건 체크
	}
}

//// --- 스페셜 코인 획득 상태를 설정하는 함수 구현 ---
//void AFingerCharacter::SetSpecialCoinCollected(bool bCollected)
//{
//	bIsSpecialCoinCollected = bCollected;
//	if (bIsSpecialCoinCollected)
//	{
//		bStar3Achieved = true; // 스페셜 코인 획득 시 별 3 달성
//		UE_LOG(LogTemp, Warning, TEXT("Special Coin Collected! Star 3 Achieved!"));
//		// 스페셜 코인 획득 시 바로 클리어 조건 확인 (선택 사항)
//		// CheckClearConditions(); 
//	}
//}
// --- 타이머 업데이트 함수 구현 ---
void AFingerCharacter::UpdateGameTimer()
{
	// 블루프린트로 플레이 시간을 관리(hh:mm:ss 누적)하기 위해 C++의 타이머 감소 및 게임 오버 로직을 비활성화합니다.
}


// --- 클리어 조건 확인 함수 구현 ---
void AFingerCharacter::CheckClearConditions()
{
	if (bHasGameEnded)
	{
		return;
	}

	// 별 1: 코인 모두 획득
	if (Score >= MaxScore)
	{
		bIsLevelCleared = true;
		bStar1Achieved = true;
		//UE_LOG(LogTemp, Warning, TEXT("Star 1 Achieved: All Coins Collected!"));
	}

	// 별 2: 코인 모두 획득 시 남은 시간이 1분(60초) 이상
	if (bStar1Achieved && TimeRemaining >= 300.0f)
	{
		bStar2Achieved = true;
		//UE_LOG(LogTemp, Warning, TEXT("Star 2 Achieved: Time Remaining >= 1 minute!"));
	}

	// 별 3: 스페셜 코인 획득 (AddScore에서 이미 처리될 수 있음)
	// bIsSpecialCoinCollected는 스페셜 코인 획득 시 바로 true로 설정되므로,
	// 여기서는 최종적으로 확인만 합니다.
	// 3별 조건: 스페셜 코인 목표 개수 획득
	if (SpecialCoinScore >= TargetSpecialCoins)
	{
		bStar3Achieved = true;
		//UE_LOG(LogTemp, Warning, TEXT("Star 3 Achieved: Target Special Coins Collected!"));
	}

	// 모든 별 획득 여부 출력
	UE_LOG(LogTemp, Warning, TEXT("--- Clear Results ---"));
	UE_LOG(LogTemp, Warning, TEXT("Star 1: %s"), (bStar1Achieved ? TEXT("YES") : TEXT("NO")));
	UE_LOG(LogTemp, Warning, TEXT("Star 2: %s"), (bStar2Achieved ? TEXT("YES") : TEXT("NO")));
	UE_LOG(LogTemp, Warning, TEXT("Star 3: %s"), (bStar3Achieved ? TEXT("YES") : TEXT("NO")));
	UE_LOG(LogTemp, Warning, TEXT("---------------------"));

	// 모든 코인을 획득했다면 타이머 중지
	GetWorldTimerManager().ClearTimer(GameTimerHandle);

	// TODO: 게임 종료 후 추가 처리 (예: 캐릭터 이동 정지, 애니메이션 변경 등)
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
}

void AFingerCharacter::SpawnRhythmEffect(bool bIsRightFoot)
{
	if (!RhythmTimingEffect) return;

	FVector BaseLocation = GetActorLocation() + GetActorForwardVector() * 35.0f;
	if (bIsRightFoot)
	{
		BaseLocation += GetActorRightVector() * 15.0f;
	}
	else
	{
		BaseLocation -= GetActorRightVector() * 15.0f;
	}

	// 바닥이나 벽 표면을 감지하기 위해 캐릭터 위쪽(UpVector)에서 아래쪽(-UpVector)으로 라인트레이스를 쏩니다.
	FVector TraceStart = BaseLocation + GetActorUpVector() * 100.0f;
	FVector TraceEnd = BaseLocation - GetActorUpVector() * 200.0f;
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	FVector SpawnLocation = BaseLocation - GetActorUpVector() * 64.0f; // 레이저 실패 시: 캐릭터 발 아래쪽으로 띄움
	FRotator SpawnRotation = FRotationMatrix::MakeFromZ(GetActorUpVector()).Rotator();

	// 바닥이나 벽에 부딪혔다면, 그 표면의 법선(Normal)을 구해 회전값으로 만듭니다.
	if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		SpawnLocation = HitResult.ImpactPoint + HitResult.ImpactNormal * 1.0f; // 깜빡임(Z-fighting) 방지용으로 딱 1.0만 띄움
		SpawnRotation = FRotationMatrix::MakeFromZ(HitResult.ImpactNormal).Rotator();
	}

	UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), RhythmTimingEffect, SpawnLocation, SpawnRotation);
	if (NiagaraComp)
	{
		NiagaraComp->SetVariableFloat(FName("SpeedMultiplier"), CurrentMovementSpeedMultiplier);
	}
}

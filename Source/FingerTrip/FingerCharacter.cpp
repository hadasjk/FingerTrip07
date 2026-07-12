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
	RhythmWindowTolerance = 0.2f; // 기본 타이밍 허용 오차
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

	bIsLevelCleared = false;
	bHasGameEnded = false;
}

// Called when the game starts or when spawned
void AFingerCharacter::BeginPlay()
{
	Super::BeginPlay();

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
		CameraManager->ViewPitchMax = 20.0f;  // 위로 최대 20도
	}
}

// Called every frame
void AFingerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsWalkingRhythmically)
	{
		AddMovementInput(GetActorForwardVector(), 1.0f);
	}

	// --- 3D 자유 시점 / 백뷰 고정(조향) 전환 로직 ---
	USpringArmComponent* SpringArmComp = FindComponentByClass<USpringArmComponent>();
	if (SpringArmComp)
	{
		float CurrentSpeed = GetVelocity().Size();
		bool bIsMoving = CurrentSpeed > 10.0f;

		// 마우스에 캐릭터가 확확 돌아가지 않도록 항상 false 유지 (대신 AddCameraYaw에서 직접 부드럽게 조향)
		bUseControllerRotationYaw = false; 
		
		// 카메라는 항상 언리얼의 부드러운 기본 시스템(ControlRotation)을 따름
		SpringArmComp->bUsePawnControlRotation = true;

		if (bIsMoving)
		{
			// 이동 상태: 카메라는 무조건 등 뒤(백뷰)를 향해 스무스하게 보간
			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				FRotator CurrentControlRot = PC->GetControlRotation();
				
				// 카메라의 타겟은 항상 캐릭터의 현재 방향(조향 방향)
				FRotator TargetRot = GetActorRotation();
				TargetRot.Pitch = DefaultBackViewRotation.Pitch;
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

	PlayerInputComponent->BindAxis("MoveForward", this, &AFingerCharacter::MoveForward);
	
	// 원래 입력 방식 대신 커스텀 함수를 바인딩하여 마우스 입력 발생 시간을 기록합니다.
	PlayerInputComponent->BindAxis("LookRight", this, &AFingerCharacter::AddCameraYaw);
	PlayerInputComponent->BindAxis("LookUp", this, &AFingerCharacter::AddCameraPitch);
}

// --- 카메라 및 캐릭터 조향 로직 ---
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
		float CurrentSpeed = GetVelocity().Size();
		if (CurrentSpeed <= 10.0f)
		{
			// 정지 중에만 카메라 위아래 회전을 허용하며, 감도를 동일하게 맞춥니다.
			AddControllerPitchInput((AxisValue * SteeringSensitivity) / 2.5f);
		}
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
		}
	}
	else
	{
		// 걷고 있는데 눌러야 할 발이 오른발인데 좌클릭을 한 경우 즉시 콤보 끊기
		if (!bNextStepIsLeft)
		{
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

	// 첫 이동 시작에 의한 왼발 딛기면 타이밍 체크 패스 (좌클릭 출발 시)
	if (bIsWalkingRhythmically && ConsecutiveRhythmHits == 1 && !bStartedWithRightClick) 
	{
		ConsecutiveRhythmHits = 2;
		UpdateMovementSpeed();
		
		// 다음 발(오른발)을 위한 이펙트 스폰
		SpawnRhythmEffect(true);
		return;
	}

	if (bIsWalkingRhythmically && (CurrentTime - LastLeftClickTime <= RhythmWindowTolerance))
	{
		LastLeftClickTime = -1.0f;
		ConsecutiveRhythmHits = FMath::Min(ConsecutiveRhythmHits + 1, MaxConsecutiveRhythmHits);
		UpdateMovementSpeed();
		bNextStepIsLeft = false; 

		SpawnRhythmEffect(true);
	}
	else if (bIsWalkingRhythmically)
	{
		bIsWalkingRhythmically = false;
		ConsecutiveRhythmHits = 0; 
		UpdateMovementSpeed(); 
	}
}

void AFingerCharacter::OnRightFootDown()
{
	float CurrentTime = UGameplayStatics::GetTimeSeconds(GetWorld());
	
	// 우클릭 출발 시 첫 발(오른발) 타이밍 체크 패스
	if (bIsWalkingRhythmically && ConsecutiveRhythmHits == 1 && bStartedWithRightClick) 
	{
		ConsecutiveRhythmHits = 2;
		UpdateMovementSpeed(); 
		
		// 다음 발(왼발)을 위한 이펙트 스폰
		SpawnRhythmEffect(false);
		return;
	}

	if (bIsWalkingRhythmically && (CurrentTime - LastRightClickTime <= RhythmWindowTolerance))
	{
		LastRightClickTime = -1.0f;
		ConsecutiveRhythmHits = FMath::Min(ConsecutiveRhythmHits + 1, MaxConsecutiveRhythmHits);
		UpdateMovementSpeed(); 
		bNextStepIsLeft = true;

		SpawnRhythmEffect(false);
	}
	else if (bIsWalkingRhythmically)
	{
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
		GetCharacterMovement()->MaxWalkSpeed = DefaultMaxWalkSpeed * CurrentMovementSpeedMultiplier;

		// 2. 점프력(JumpZVelocity) 업데이트 (속도 배율에 비례)
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

		GetCharacterMovement()->JumpZVelocity = DefaultJumpZVelocity * JumpMultiplier;

		/*UE_LOG(LogTemp, Warning, TEXT("Updated MaxWalkSpeed: %.1f (x%.1f), JumpZ: %.1f (x%.1f), Hits: %d"),
			GetCharacterMovement()->MaxWalkSpeed, CurrentMovementSpeedMultiplier,
			GetCharacterMovement()->JumpZVelocity, JumpMultiplier, ConsecutiveRhythmHits);*/
	}
}

void AFingerCharacter::MoveForward(float AxisValue)
{
	// Axis 기반 이동은 사용하지 않습니다. (자동 이동으로 대체)
}

void AFingerCharacter::Jump()
{
	Super::Jump(); // ACharacter의 기본 Jump 기능을 호출합니다.
	bJumpInputPressed = true; // 점프 입력이 눌렸음을 표시
}

void AFingerCharacter::StopJumping()
{
	Super::StopJumping();
}

void AFingerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit); // ACharacter의 기본 Landed 기능을 호출합니다.
	bJumpInputPressed = false; // 착지했으므로 점프 입력 상태를 리셋 
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
	if (TimeRemaining > 0)
	{
		TimeRemaining -= 1.0f; // 1초 감소
		if (TimeRemaining < 0)
		{
			TimeRemaining = 0;
		}
		UE_LOG(LogTemp, Warning, TEXT("Time Remaining: %.1f"), TimeRemaining);

		// 시간이 0이 되면 게임 종료 또는 특정 이벤트 발생
		if (TimeRemaining <= 0 && Score < MaxScore)
		{
			// 시간 초과로 인한 게임 실패 로직
			bIsLevelCleared = false;
			bHasGameEnded = true;

			UE_LOG(LogTemp, Warning, TEXT("Time's Up! Game Over."));
			GetWorldTimerManager().ClearTimer(GameTimerHandle); // 타이머 중지

			GetCharacterMovement()->StopMovementImmediately();
			GetCharacterMovement()->DisableMovement();
		}
	}
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

	// 바닥의 기울기를 감지하기 위해 위에서 아래로 레이저(LineTrace)를 쏩니다.
	FVector TraceStart = BaseLocation + FVector(0, 0, 100.0f);
	FVector TraceEnd = BaseLocation - FVector(0, 0, 200.0f);
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	FVector SpawnLocation = BaseLocation;
	SpawnLocation.Z -= 64.0f; // 레이저 실패 시: 바닥(-65)에서 1.0 위로 띄움
	FRotator SpawnRotation = FRotator::ZeroRotator;

	// 바닥에 부딪혔다면, 그 바닥의 경사도(Normal)를 구해 회전값으로 만듭니다.
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

// Fill out your copyright notice in the Description page of Project Settings.


#include "FingerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h" // GetWorld()->GetTimeSeconds() 사용을 위해 포함

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
	bCanMove = false; // 초기에는 이동 비활성화

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
}

// Called every frame
void AFingerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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
	PlayerInputComponent->BindAxis("LookRight", this, &APawn::AddControllerYawInput);
}


void AFingerCharacter::OnLeftClick()
{
	bIsLeftPressed = true;
	LastLeftClickTime = UGameplayStatics::GetTimeSeconds(GetWorld()); // 클릭 시간 기록
}

void AFingerCharacter::OnRightClick()
{
	bIsRightPressed = true;
	LastRightClickTime = UGameplayStatics::GetTimeSeconds(GetWorld()); // 클릭 시간 기록
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
	if (bIsLeftPressed && (CurrentTime - LastLeftClickTime <= RhythmWindowTolerance))
	{
		bCanMove = true;
		LastLeftClickTime = -1.0f;

		// --- 리듬 성공 시 가속도 로직 ---
		ConsecutiveRhythmHits = FMath::Min(ConsecutiveRhythmHits + 1, MaxConsecutiveRhythmHits);
		UpdateMovementSpeed(); // 속도 업데이트
		//UE_LOG(LogTemp, Warning, TEXT("Left Foot Down + Left Click SUCCESS! Hits: %d"), ConsecutiveRhythmHits);
	}
	else
	{
		bCanMove = false;
		// --- 리듬 실패 시 가속도 로직 ---
		ConsecutiveRhythmHits = 0; // 연속 성공 횟수 초기화
		UpdateMovementSpeed(); // 속도 초기화
		//UE_LOG(LogTemp, Warning, TEXT("Left Foot Down but Left Click FAILED. Stopping. Hits Reset."));
	}
}

void AFingerCharacter::OnRightFootDown()
{
	float CurrentTime = UGameplayStatics::GetTimeSeconds(GetWorld());
	if (bIsRightPressed && (CurrentTime - LastRightClickTime <= RhythmWindowTolerance))
	{
		bCanMove = true;
		LastRightClickTime = -1.0f;

		// --- 리듬 성공 시 가속도 로직 ---
		ConsecutiveRhythmHits = FMath::Min(ConsecutiveRhythmHits + 1, MaxConsecutiveRhythmHits);
		UpdateMovementSpeed(); // 속도 업데이트
		//UE_LOG(LogTemp, Warning, TEXT("Right Foot Down + Right Click SUCCESS! Hits: %d"), ConsecutiveRhythmHits);
	}
	else
	{
		bCanMove = false;
		// --- 리듬 실패 시 가속도 로직 ---
		ConsecutiveRhythmHits = 0; // 연속 성공 횟수 초기화
		UpdateMovementSpeed(); // 속도 초기화
		//UE_LOG(LogTemp, Warning, TEXT("Right Foot Down but Right Click FAILED. Stopping. Hits Reset."));
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
	// bCanMove가 true이고, 이동 입력이 있을 때만 실제 이동을 허용
	if ((Controller != nullptr) && (AxisValue != 0.0f) && bCanMove)
	{
		AddMovementInput(GetActorForwardVector() * AxisValue);
	}

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


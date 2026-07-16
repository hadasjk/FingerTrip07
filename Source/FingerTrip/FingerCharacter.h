// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FingerCharacter.generated.h"

UCLASS()
class FINGERTRIP_API AFingerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    // Sets default values for this character's properties
    AFingerCharacter();

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;

    // Called to bind functionality to input
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    void MoveForward(float AxisValue);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    bool bJumpInputPressed = false; // 점프 입력이 눌렸을 때 true


    UFUNCTION()
    void OnLeftClick();

    UFUNCTION()
    void OnRightClick();

    UFUNCTION()
    void OnLeftRelease();

    UFUNCTION()
    void OnRightRelease();

    UPROPERTY(VisibleAnywhere)
    bool bIsLeftPressed;

    UPROPERTY(VisibleAnywhere)
    bool bIsRightPressed;

    // 좌클릭이 마지막으로 눌린 게임 시간 (초)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input")
    float LastLeftClickTime;

    // 우클릭이 마지막으로 눌린 게임 시간 (초)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input")
    float LastRightClickTime;

    // 리듬 게임 타이밍 허용 오차 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Rhythm")
    float RhythmWindowTolerance;

    // 리듬에 맞춰 걷고 있는지 여부
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Rhythm")
    bool bIsWalkingRhythmically;

    // 다음 밟아야 할 발이 왼발인지 여부
    	UPROPERTY(BlueprintReadOnly, Category = "Movement|Rhythm")
	bool bNextStepIsLeft = false;

	// 우클릭으로 출발했는지 여부 (애니메이션 분기용)
	UPROPERTY(BlueprintReadOnly, Category = "Movement|Rhythm")
	bool bStartedWithRightClick = false;

	void SpawnRhythmEffect(bool bIsRightFoot);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Rhythm")
    class UNiagaraSystem* RhythmTimingEffect;

    // 애니메이션 Notify에서 호출될 함수
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void OnLeftFootDown();

    UFUNCTION(BlueprintCallable, Category = "Animation")
    void OnRightFootDown();



    // ACharacter의 점프 관련 함수 오버라이드
    virtual void Jump() override;
    virtual void StopJumping() override;
    virtual void Landed(const FHitResult& Hit) override;

    // --- 가속도 시스템을 위한 새로운 변수 및 함수 ---
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Rhythm")
    int32 ConsecutiveRhythmHits; // 연속으로 리듬을 성공한 횟수

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Rhythm")
    int32 MaxConsecutiveRhythmHits; // 속도가 최대로 증가하는 연속 성공 횟수 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Rhythm")
    float MinSpeedMultiplier; // 최소 속도 배

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Rhythm")
    float MaxSpeedMultiplier; // 최대 속도 배율 

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Rhythm")
    float CurrentMovementSpeedMultiplier; // 현재 적용되는 이동 속도 배율

    // 속도를 업데이트하는 함수
    void UpdateMovementSpeed();


private:
    float DefaultMaxWalkSpeed; // 캐릭터의 원래 최대 걷기 속도 저장용
    // **새로운 멤버 변수: 속도 배율을 저장할 상수 배열**
    const TArray<float> SpeedMultipliers = {
        1.0f, 1.0f, 1.2f, 1.4f, 1.6f, 1.8f,
        2.0f, 2.2f, 2.4f, 2.6f, 2.8f, 3.0f,
        3.2f, 3.4f, 3.6f
    };

    // --- 게임 클리어 및 별점 시스템을 위한 변수 및 함수 ---
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats")
    int32 Score;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Clear")
    int32 MaxScore; // 레벨에 있는 총 코인의 수 (별 1, 2 조건)

    // 점수를 증가시키는 함수 (코인을 먹었을 때 호출)
    UFUNCTION(BlueprintCallable, Category = "Player Stats")
    void AddScore(int32 ScoreToAdd);

    //UFUNCTION(BlueprintCallable, Category = "Game Clear")
    //void SetSpecialCoinCollected(bool bCollected); // 블루프린트에서 호출 가능하도록 설정


    // --- 스페셜 코인 점수 시스템 (새로 추가/수정) ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Stats")
    int32 SpecialCoinScore; // 획득한 스페셜 코인 개수 (3별 조건)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Clear")
    int32 TargetSpecialCoins; // 3별 획득을 위한 스페셜 코인 목표 개수

    UFUNCTION(BlueprintCallable, Category = "Player Stats")
    void AddSpecialCoinScore(int32 SpecialScoreToAdd); // 스페셜 코인 점수 추가


    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Clear")
    float TimeRemaining; // 남은 시간 (초 단위)

    // --- 별 획득 여부 (UI 표시 등을 위해) ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Clear")
    bool bStar1Achieved;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Clear")
    bool bStar2Achieved;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Clear")
    bool bStar3Achieved;

    // --- 클리어 조건 확인 함수 ---
    UFUNCTION(BlueprintCallable, Category = "Game Clear")
    void CheckClearConditions();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Clear")
    bool bIsLevelCleared; // 레벨 클리어 조건이 충족되었는지 여부

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Clear")
    bool bHasGameEnded;

    // --- 카메라 전환 제어 변수 ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float CameraReturnInterpSpeed = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    FRotator DefaultBackViewRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float GyroSensitivity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float AutoAlignDelay = 1.5f; // 마우스 조작 후 백뷰 복귀까지 대기하는 시간 (초)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float SteeringSensitivity = 0.5f; // 이동 중 마우스 조향 민감도

    float LastMouseInputTime = -1.0f; // 마지막으로 마우스를 조작한 시간

    void AddCameraYaw(float AxisValue);
    void AddCameraPitch(float AxisValue);

    // 카메라 줌인/줌아웃
    void ZoomCamera(float AxisValue);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
    float ZoomSpeed = 100.0f; // 줌 속도

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
    float MinZoom = 200.0f; // 최소 거리

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
    float MaxZoom = 1500.0f; // 최대 거리

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
    float ZoomInterpSpeed = 5.0f; // 줌 보간 속도

    float TargetZoomLength; // 부드러운 줌을 위한 목표 거리

    // --- 점프력 가속을 위한 변수 ---
    UPROPERTY(EditAnywhere, Category = "Movement|Jump")
    float DefaultJumpZVelocity; // 캐릭터의 원래 JumpZVelocity를 저장할 변수

    UPROPERTY(EditAnywhere, Category = "Movement|Jump")
    float MaxJumpZVelocityMultiplier; // 최대 속도일 때 JumpZVelocity의 최대 배율

private:


    UPROPERTY(VisibleAnywhere, Category = "Game Clear")
    bool bIsSpecialCoinCollected; // 스페셜 코인 획득 여부 (별 3 조건)

    UPROPERTY(EditAnywhere, Category = "Game Clear")
    float LevelMaxTime; // 레벨의 최대 시간 (초 단위) (별 2 조건)



    // --- 타이머 관련 함수 ---
    void UpdateGameTimer(); // 매 초마다 호출될 타이머 함수
    FTimerHandle GameTimerHandle; // 타이머 핸들



};

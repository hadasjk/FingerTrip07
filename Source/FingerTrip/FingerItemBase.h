// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FingerItemTypes.h"
#include "FingerItemBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class URotatingMovementComponent;
class UTextRenderComponent;

// PrioritizeCategories와 AutoExpandCategories로 "Item Settings"를 디테일 패널 최상단으로 강제 배치 및 자동 펼침
UCLASS(PrioritizeCategories = ("Item Settings"), AutoExpandCategories = ("Item Settings"))
class FINGERTRIP_API AFingerItemBase : public AActor
{
	GENERATED_BODY()
	
public:	
	AFingerItemBase();

	// =========================================================================
	// [1] 디테일 창 최상단 노출 핵심 설정 (인스턴스별 드롭다운 선택)
	// =========================================================================

	// 디테일 패널에서 배치된 각 인스턴스마다 선택 가능한 아이템 종류 (최우선 노출)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings", meta = (DisplayPriority = 0))
	EFingerItemType ItemType = EFingerItemType::SpeedBoost;

	// 아이템 획득 후 다시 나타날 때까지 걸리는 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings", meta = (DisplayPriority = 1))
	float RespawnTime = 5.0f;

	// 에디터/월드에서 머리 위 라벨 텍스트 표시 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings", meta = (DisplayPriority = 2))
	bool bShowLabelInWorld = true;

	// 세부 커스텀 데이터 (지속시간, 수치 등 - 고급 설정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings|Advanced Custom", meta = (DisplayPriority = 3))
	FFingerItemData ItemData;

	// 상하 부유 애니메이션 진폭 (위아래 흔들리는 높이)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings|Animation", AdvancedDisplay)
	float FloatAmplitude = 12.0f;

	// 상하 부유 애니메이션 주기 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings|Animation", AdvancedDisplay)
	float FloatFrequency = 2.0f;

	// 아이템 데이터 게터
	UFUNCTION(BlueprintPure, Category = "Item")
	const FFingerItemData& GetItemData() const { return ItemData; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void OnConstruction(const FTransform& Transform) override;

	// =========================================================================
	// [2] 컴포넌트 목록
	// =========================================================================

	// 오버랩 감지용 구체 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionSphere;

	// 아이템 메시 컴포넌트 (공중 회전 및 렌더링)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// 공중 회전 컴포넌트 (초당 Yaw 회전)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<URotatingMovementComponent> RotatingMovement;

	// 레벨 디자인 확인용 텍스트 렌더러 (에디터 및 인게임 선택 표시)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> LabelTextComponent;

	FVector InitialMeshLocalLocation;
	FTimerHandle RespawnTimerHandle;

	// 플레이어 닿음 오버랩 이벤트
	UFUNCTION()
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                            bool bFromSweep, const FHitResult& SweepResult);

	// 아이템 비활성화 (획득 시)
	virtual void DeactivateItem();

	// 아이템 리스폰 (일정 시간 후)
	virtual void RespawnItem();

	// ItemType에 맞춰 기본 데이터 및 색상 동기화
	void SyncItemTypeDefaults();
};

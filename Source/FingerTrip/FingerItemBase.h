// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FingerItemTypes.h"
#include "FingerItemBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;

// PrioritizeCategories와 AutoExpandCategories로 "Item Settings"를 디테일 패널 최상단으로 강제 배치 및 자동 펼침
UCLASS(PrioritizeCategories = ("Item Settings"), AutoExpandCategories = ("Item Settings"))
class FINGERTRIP_API AFingerItemBase : public AActor
{
	GENERATED_BODY()
	
public:	
	AFingerItemBase();

	// =========================================================================
	// [1] 디테일 창 최상단 노출 핵심 설정 (인스턴스별 설정)
	// =========================================================================

	// 아이템 대분류 (소모 아이템 vs 영구 아이템)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings", meta = (DisplayPriority = 0))
	EFingerItemCategory ItemCategory = EFingerItemCategory::Consumable;

	// --- [소모 아이템 설정] ---
	// true이면 획득 시 소모 아이템 풀에서 무작위 선택 (현재 4종, 향후 추가 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings", meta = (DisplayPriority = 1, EditCondition = "ItemCategory == EFingerItemCategory::Consumable"))
	bool bRandomConsumable = true;

	// bRandomConsumable이 false일 때 고정으로 지급할 소모 아이템 종류
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings", meta = (DisplayPriority = 2, EditCondition = "ItemCategory == EFingerItemCategory::Consumable && !bRandomConsumable"))
	EFingerItemType ItemType = EFingerItemType::SpeedBoost;

	// 소모 아이템 리스폰 시간 (초) - 영구 아이템은 리스폰되지 않음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings", meta = (DisplayPriority = 3, EditCondition = "ItemCategory == EFingerItemCategory::Consumable"))
	float RespawnTime = 5.0f;

	// --- [영구 아이템 설정] ---
	// 영구 아이템 종류 (현재 이동속도 영구 증가, 향후 추가 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings", meta = (DisplayPriority = 1, EditCondition = "ItemCategory == EFingerItemCategory::Permanent"))
	EFingerPermanentItemType PermanentItemType = EFingerPermanentItemType::PermanentSpeedBoost;

	// 영구 능력치 증가량 (이동속도의 경우 0.04x 씩 빨라짐)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings", meta = (DisplayPriority = 2, EditCondition = "ItemCategory == EFingerItemCategory::Permanent"))
	float PermanentValue = 0.04f;

	// --- [비주얼 설정] ---
	// 빙글빙글 회전 속도 (초당 도 회전, 예: 90 = 1초에 90도 회전)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings|Visual", meta = (DisplayPriority = 4))
	float RotationSpeed = 90.0f;

	// 상하 부유 애니메이션 진폭 (위아래 흔들리는 높이)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings|Visual", AdvancedDisplay)
	float FloatAmplitude = 12.0f;

	// 상하 부유 애니메이션 주기 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings|Visual", AdvancedDisplay)
	float FloatFrequency = 2.0f;

	// 세부 커스텀 데이터 (고정 소모 아이템 전용 세부 설정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings|Advanced Custom", AdvancedDisplay)
	FFingerItemData ItemData;

	// 커스텀 소모 아이템 풀 (기본 4종 외에 추가하거나 특정 액터에서 풀을 한정하고 싶을 때 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings|Advanced Custom", AdvancedDisplay)
	TArray<EFingerItemType> ConsumablePool;

	// 소모 아이템 풀에서 랜덤으로 1개를 추첨하는 함수
	UFUNCTION(BlueprintCallable, Category = "Item")
	EFingerItemType PickRandomConsumableType() const;

	// 특정 아이템 타입의 기본 FFingerItemData 생성 헬퍼
	static FFingerItemData CreateDefaultItemData(EFingerItemType InType);

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

	FVector InitialMeshLocalLocation;
	FTimerHandle RespawnTimerHandle;

	// 플레이어 닿음 오버랩 이벤트
	UFUNCTION()
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                            bool bFromSweep, const FHitResult& SweepResult);

	// 소모 아이템 비활성화 및 리스폰 예약
	virtual void DeactivateItem();

	// 소모 아이템 리스폰 (일정 시간 후)
	virtual void RespawnItem();

	// 영구 아이템 획득 처리 (리스폰되지 않고 파괴/영구 비활성화)
	virtual void ConsumePermanentItem(class AFingerCharacter* Player);

	// 설정값에 맞춰 데이터 동기화
	void SyncItemTypeDefaults();
};

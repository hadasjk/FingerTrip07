// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FingerItemTypes.generated.h"

class UTexture2D;

/**
 * 아이템 대분류 (소모 아이템 vs 영구 아이템)
 */
UENUM(BlueprintType)
enum class EFingerItemCategory : uint8
{
	Consumable      UMETA(DisplayName = "소모 아이템 (Shift 사용, 리스폰 O)"),
	Permanent       UMETA(DisplayName = "영구 아이템 (영구 스탯, 리스폰 X)")
};

/**
 * 소모 아이템 능력 종류 (추후 새로운 소모 아이템 추가 시 여기에 추가)
 */
UENUM(BlueprintType)
enum class EFingerItemType : uint8
{
	None            UMETA(DisplayName = "None"),
	SpeedBoost      UMETA(DisplayName = "Speed Boost (이동속도 증가)"),
	JumpBoost       UMETA(DisplayName = "Jump Boost (점프력 증가)"),
	Dash            UMETA(DisplayName = "Dash (대쉬)"),
	AirWalk         UMETA(DisplayName = "Air Walk (공중 걷기)")
	// 추후 추가될 소모 아이템 예시: Invincible, Magnet 등
};

/**
 * 영구 아이템 종류 (추후 새로운 영구 아이템 추가 시 여기에 추가)
 */
UENUM(BlueprintType)
enum class EFingerPermanentItemType : uint8
{
	None                    UMETA(DisplayName = "None"),
	PermanentSpeedBoost     UMETA(DisplayName = "Permanent Speed (+0.04x 이동속도)")
	// 추후 추가될 영구 아이템 예시: PermanentJumpBoost, MaxGaugeBoost 등
};

/**
 * 소모 아이템 데이터 구조체
 */
USTRUCT(BlueprintType)
struct FFingerItemData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	EFingerItemType ItemType = EFingerItemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FText ItemName;

	// UI 표시용 아이콘
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|UI")
	TObjectPtr<UTexture2D> ItemIcon = nullptr;

	// 버프 지속 시간 (초 단위, 대쉬는 0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Stats")
	float Duration = 5.0f;

	// 효과 수치 (속도 배율, 점프 배율, 대쉬 힘 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Stats")
	float EffectValue = 1.5f;

	FFingerItemData()
		: ItemType(EFingerItemType::None)
		, ItemName(FText::GetEmpty())
		, ItemIcon(nullptr)
		, Duration(0.0f)
		, EffectValue(0.0f)
	{
	}

	FFingerItemData(EFingerItemType InType, const FText& InName, float InDuration, float InValue)
		: ItemType(InType)
		, ItemName(InName)
		, ItemIcon(nullptr)
		, Duration(InDuration)
		, EffectValue(InValue)
	{
	}
};

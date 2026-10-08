// Fill out your copyright notice in the Description page of Project Settings.

#include "FingerItemBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FingerCharacter.h"

AFingerItemBase::AFingerItemBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. 충돌 구체 (루트 컴포넌트)
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->SetSphereRadius(50.0f);
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 2. 스태틱 메시 (아이템 외형 - 회전 및 부유 대상)
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 기본 설정값 초기화
	ItemCategory = EFingerItemCategory::Consumable;
	bRandomConsumable = true;
	ItemType = EFingerItemType::SpeedBoost;
	RespawnTime = 5.0f;

	PermanentItemType = EFingerPermanentItemType::PermanentSpeedBoost;
	PermanentValue = 0.04f;

	RotationSpeed = 90.0f; // 1초에 90도 회전
	FloatAmplitude = 12.0f;
	FloatFrequency = 2.0f;

	// 기본 소모 아이템 풀 (4종)
	ConsumablePool = {
		EFingerItemType::SpeedBoost,
		EFingerItemType::JumpBoost,
		EFingerItemType::Dash,
		EFingerItemType::AirWalk
	};

	SyncItemTypeDefaults();
}

void AFingerItemBase::BeginPlay()
{
	Super::BeginPlay();

	InitialMeshLocalLocation = MeshComponent ? MeshComponent->GetRelativeLocation() : FVector::ZeroVector;

	if (CollisionSphere)
	{
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AFingerItemBase::OnOverlapBegin);
	}

	SyncItemTypeDefaults();
}

void AFingerItemBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsHidden() && MeshComponent)
	{
		// 1. 위아래로 둥실둥실 부유하는 사인파 애니메이션
		float Time = GetWorld()->GetTimeSeconds();
		float DeltaZ = FMath::Sin(Time * FloatFrequency) * FloatAmplitude;
		MeshComponent->SetRelativeLocation(InitialMeshLocalLocation + FVector(0.0f, 0.0f, DeltaZ));

		// 2. 빙글빙글 360도 회전 애니메이션
		if (RotationSpeed != 0.0f)
		{
			MeshComponent->AddRelativeRotation(FRotator(0.0f, RotationSpeed * DeltaTime, 0.0f));
		}
	}
}

void AFingerItemBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	SyncItemTypeDefaults();
}

#if WITH_EDITOR
void AFingerItemBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SyncItemTypeDefaults();
}
#endif

EFingerItemType AFingerItemBase::PickRandomConsumableType() const
{
	const TArray<EFingerItemType>& Pool = (ConsumablePool.Num() > 0) ? ConsumablePool : TArray<EFingerItemType>{
		EFingerItemType::SpeedBoost,
		EFingerItemType::JumpBoost,
		EFingerItemType::Dash,
		EFingerItemType::AirWalk
	};

	int32 RandIndex = FMath::RandRange(0, Pool.Num() - 1);
	return Pool[RandIndex];
}

FFingerItemData AFingerItemBase::CreateDefaultItemData(EFingerItemType InType)
{
	FFingerItemData Data;
	Data.ItemType = InType;

	switch (InType)
	{
	case EFingerItemType::SpeedBoost:
		Data.ItemName = NSLOCTEXT("FingerItem", "SpeedBoost", "Speed Boost");
		Data.Duration = 5.0f;
		Data.EffectValue = 1.6f;
		break;

	case EFingerItemType::JumpBoost:
		Data.ItemName = NSLOCTEXT("FingerItem", "JumpBoost", "Jump Boost");
		Data.Duration = 5.0f;
		Data.EffectValue = 1.6f;
		break;

	case EFingerItemType::Dash:
		Data.ItemName = NSLOCTEXT("FingerItem", "Dash", "Dash");
		Data.Duration = 0.0f;
		Data.EffectValue = 2200.0f;
		break;

	case EFingerItemType::AirWalk:
		Data.ItemName = NSLOCTEXT("FingerItem", "AirWalk", "Air Walk");
		Data.Duration = 3.0f;
		Data.EffectValue = 0.0f;
		break;

	default:
		Data.ItemName = FText::FromString(TEXT("None"));
		break;
	}

	return Data;
}

void AFingerItemBase::SyncItemTypeDefaults()
{
	if (ItemCategory == EFingerItemCategory::Consumable && !bRandomConsumable)
	{
		ItemData = CreateDefaultItemData(ItemType);
	}
}

void AFingerItemBase::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                    bool bFromSweep, const FHitResult& SweepResult)
{
	AFingerCharacter* Player = Cast<AFingerCharacter>(OtherActor);
	if (!Player)
	{
		return;
	}

	if (ItemCategory == EFingerItemCategory::Permanent)
	{
		// [영구 아이템]: 획득 시 영구 효과 적용 후 리스폰 없이 영구 제거
		ConsumePermanentItem(Player);
	}
	else
	{
		// [소모 아이템]: 랜덤 선택 또는 고정 아이템 지급 후 리스폰 타이머 작동
		EFingerItemType FinalType = bRandomConsumable ? PickRandomConsumableType() : ItemType;
		FFingerItemData FinalData = (bRandomConsumable || ItemData.ItemType != FinalType) ? CreateDefaultItemData(FinalType) : ItemData;

		Player->AcquireItem(FinalData);

		// 월드에서 숨기고 리스폰 타이머 가동
		DeactivateItem();
	}
}

void AFingerItemBase::DeactivateItem()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	// 리스폰 타이머 등록
	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AFingerItemBase::RespawnItem, RespawnTime, false);
}

void AFingerItemBase::RespawnItem()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}

void AFingerItemBase::ConsumePermanentItem(AFingerCharacter* Player)
{
	if (Player)
	{
		Player->ApplyPermanentItem(PermanentItemType, PermanentValue);
	}

	// 영구 아이템은 리스폰되지 않으므로 액터 파괴
	Destroy();
}

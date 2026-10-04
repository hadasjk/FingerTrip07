// Fill out your copyright notice in the Description page of Project Settings.

#include "FingerItemBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
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

	// 2. 스태틱 메시 (아이템 외형)
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 3. 공중 천천히 회전 컴포넌트 (초당 60도)
	RotatingMovement = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingMovement"));
	RotatingMovement->RotationRate = FRotator(0.0f, 60.0f, 0.0f);

	// 4. 레벨 디자인용 라벨 텍스트
	LabelTextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LabelTextComponent"));
	LabelTextComponent->SetupAttachment(RootComponent);
	LabelTextComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
	LabelTextComponent->SetHorizontalAlignment(EHTA_Center);
	LabelTextComponent->SetVerticalAlignment(EVRTA_TextCenter);
	LabelTextComponent->SetWorldSize(20.0f);

	// 기본값 초기화
	ItemType = EFingerItemType::SpeedBoost;
	RespawnTime = 5.0f;
	FloatAmplitude = 10.0f;
	FloatFrequency = 2.0f;
	bShowLabelInWorld = true;

	SyncItemTypeDefaults();
}

void AFingerItemBase::BeginPlay()
{
	Super::BeginPlay();

	InitialMeshLocalLocation = MeshComponent->GetRelativeLocation();

	if (CollisionSphere)
	{
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AFingerItemBase::OnOverlapBegin);
	}

	if (LabelTextComponent)
	{
		LabelTextComponent->SetVisibility(bShowLabelInWorld);
	}
}

void AFingerItemBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 공중에서 둥실둥실 위아래로 부유하는 애니메이션
	if (!IsHidden())
	{
		float Time = GetWorld()->GetTimeSeconds();
		float DeltaZ = FMath::Sin(Time * FloatFrequency) * FloatAmplitude;
		MeshComponent->SetRelativeLocation(InitialMeshLocalLocation + FVector(0.0f, 0.0f, DeltaZ));

		// 카메라 방향으로 라벨 텍스트 빌보드 회전
		if (bShowLabelInWorld && LabelTextComponent && LabelTextComponent->IsVisible())
		{
			if (APlayerCameraManager* CameraManager = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->PlayerCameraManager : nullptr)
			{
				FVector CamLoc = CameraManager->GetCameraLocation();
				FRotator LookRot = (CamLoc - LabelTextComponent->GetComponentLocation()).Rotation();
				LabelTextComponent->SetWorldRotation(LookRot);
			}
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

	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AFingerItemBase, ItemType))
	{
		// 디테일 패널에서 ItemType 드롭다운을 변경하면 해당 버프에 맞는 기본값 자동 동기화
		SyncItemTypeDefaults();
	}
}
#endif

void AFingerItemBase::SyncItemTypeDefaults()
{
	ItemData.ItemType = ItemType;

	FColor LabelColor = FColor::White;
	FString TypeNameString;

	switch (ItemType)
	{
	case EFingerItemType::SpeedBoost:
		ItemData.ItemName = NSLOCTEXT("FingerItem", "SpeedBoost", "이동속도 증가");
		if (ItemData.Duration <= 0.0f) ItemData.Duration = 5.0f;
		if (ItemData.EffectValue <= 0.0f) ItemData.EffectValue = 1.6f; // 기본 1.6배 가속
		TypeNameString = TEXT("속도 증가");
		LabelColor = FColor::Yellow;
		break;

	case EFingerItemType::JumpBoost:
		ItemData.ItemName = NSLOCTEXT("FingerItem", "JumpBoost", "점프력 증가");
		if (ItemData.Duration <= 0.0f) ItemData.Duration = 5.0f;
		if (ItemData.EffectValue <= 0.0f) ItemData.EffectValue = 1.6f; // 기본 1.6배 점프력
		TypeNameString = TEXT("점프력 증가");
		LabelColor = FColor::Green;
		break;

	case EFingerItemType::Dash:
		ItemData.ItemName = NSLOCTEXT("FingerItem", "Dash", "대쉬");
		ItemData.Duration = 0.0f; // 즉발형
		if (ItemData.EffectValue <= 0.0f) ItemData.EffectValue = 2200.0f; // 기본 대쉬 세기
		TypeNameString = TEXT("대쉬");
		LabelColor = FColor::Cyan;
		break;

	case EFingerItemType::AirWalk:
		ItemData.ItemName = NSLOCTEXT("FingerItem", "AirWalk", "공중 걷기");
		ItemData.Duration = 3.0f; // 3초간 공중 걷기
		ItemData.EffectValue = 0.0f;
		TypeNameString = TEXT("공중 걷기");
		LabelColor = FColor(200, 100, 255);
		break;

	default:
		TypeNameString = TEXT("None");
		break;
	}

	if (LabelTextComponent)
	{
		LabelTextComponent->SetText(FText::FromString(TypeNameString));
		LabelTextComponent->SetTextRenderColor(LabelColor);
		LabelTextComponent->SetVisibility(bShowLabelInWorld && ItemType != EFingerItemType::None);
	}
}

void AFingerItemBase::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                    bool bFromSweep, const FHitResult& SweepResult)
{
	if (AFingerCharacter* Player = Cast<AFingerCharacter>(OtherActor))
	{
		// 플레이어에게 아이템 부여 (기존 아이템이 있더라도 새 아이템으로 덮어씌움)
		Player->AcquireItem(ItemData);

		// 먹은 후 월드에서 일시 숨김 및 리스폰 타이머 가동
		DeactivateItem();
	}
}

void AFingerItemBase::DeactivateItem()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	if (LabelTextComponent)
	{
		LabelTextComponent->SetVisibility(false);
	}

	// 리스폰 타이머 등록 (인스턴스의 ItemType과 ItemData는 그대로 유지되므로 같은 버프로 다시 나타남)
	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AFingerItemBase::RespawnItem, RespawnTime, false);
}

void AFingerItemBase::RespawnItem()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	if (LabelTextComponent && bShowLabelInWorld)
	{
		LabelTextComponent->SetVisibility(true);
	}
}

#include "pch.h"
#include "ActorComponent.h"

UActorComponent::UActorComponent()
    : Owner(nullptr), bIsActive(true), bCanEverTick(false)
{
}

UActorComponent::~UActorComponent()
{
}

void UActorComponent::InitializeComponent()
{
    // 액터에 부착될 때 초기화
    // 필요하다면 Override
}

void UActorComponent::BeginPlay()
{
    // 게임 시작 시
    // 필요하다면 Override
}

void UActorComponent::TickComponent(float DeltaTime)
{
    if (!bIsActive || !bCanEverTick)
        return;

    // 매 프레임 처리
    // 자식 클래스에서 Override
}

void UActorComponent::EndPlay()
{
    // 파괴 시
    // 필요하다면 Override
}

#include "pch.h"
#include "ActorComponent.h"
#include "Actor.h"
#include "World.h"
#include "SelectionManager.h"

IMPLEMENT_CLASS(UActorComponent)

BEGIN_PROPERTIES(UActorComponent)
    ADD_PROPERTY(bool, bIsActive, "컴포넌트", true, "컴포넌트를 활성화합니다")
    ADD_PROPERTY(bool, bTickEnabled, "컴포넌트", true, "틱을 확성화합니다. 기본적으로 틱이 가능한 컴포넌트만 영향이 있습니다.")
END_PROPERTIES()
    
UActorComponent::UActorComponent()
{
}

UActorComponent::~UActorComponent()
{
    // 안전장치: 등록 상태면 해제
    if (bRegistered) UnregisterComponent();
}

UWorld* UActorComponent::GetWorld() const
{
    return Owner ? Owner->GetWorld() : nullptr;
}

// ─────────────── Registration

void UActorComponent::RegisterComponent(UWorld* InWorld)
{
    if (bRegistered)
    {
        //UE_LOG("ActorComponent::RegisterComponent - Already registered, skipping: %s", GetClass()->Name);
        return;
    }
    //UE_LOG("ActorComponent::RegisterComponent - Registering: %s", GetClass()->Name);
    bRegistered = true;
    OnRegister(InWorld);


    // 여기서는 게임 수명 훅을 직접 부르지 않음.
    // BeginPlay/InitializeComponent는 보통 Actor/World 타이밍에서 호출.
    // 다만 에디터 유틸 컴포넌트라면 필요에 따라 여기서 InitializeComponent를 호출해도 됨.
}

void UActorComponent::UnregisterComponent()
{
    if (!bRegistered) return;

    OnUnregister();
    bRegistered = false;
}

void UActorComponent::OnRegister(UWorld* InWorld)
{
    // 리소스/핸들 생성, 메시/버퍼 생성 등(프레임 비의존)
}

void UActorComponent::OnUnregister()
{
    // 리소스/핸들 반납
}

// 외부에서 호출되는 컴포넌트 삭제 요청 (= 너 월드에서 지워짐 ㅅㄱ)
void UActorComponent::DestroyComponent()
{
    // NOTE: 액터는 bPendingDestroy 적용되었는데 컴포넌트는 아직 적용되지 않고 바로 삭제됨
    if (bPendingDestroy) return;
    bPendingDestroy = true;

    // 등록 중이면 우선 해제(EndPlay 포함)
    if (bRegistered) UnregisterComponent();

    // UObject 메모리 해제
    DeleteObject(this);
    // Owner 참조 끊기
    //Owner = nullptr;
}

// ─────────────── Lifecycle (게임 수명)

void UActorComponent::InitializeComponent()
{
    // BeginPlay 이전 초기화(게임 수명 관련 초기화)
}

void UActorComponent::BeginPlay()
{
    // PIE 시작 후 월드 등록 시 1회
    // 필요하다면 Override
}

void UActorComponent::TickComponent(float DeltaTime)
{
    // 매 프레임 처리
}

void UActorComponent::EndPlay()
{
    // PIE 중 파괴 시
    // 필요하다면 Override
}

void UActorComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    Owner = nullptr; // Actor에서 이거 설정해 줌
}

void UActorComponent::PostDuplicate()
{
    bRegistered = false;
}

void UActorComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    // UActorComponent는 기본적으로 직렬화할 추가 데이터가 없음
    // 파생 클래스에서 필요한 데이터를 직렬화하도록 오버라이드
}
#include "pch.h"
#include "TestAnimGraphInstance.h"
#include "AnimSequence.h"
#include "AnimationTypes.h"
#include "AnimationStateMachine.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "GlobalConsole.h"
#include "ResourceManager.h"

// ========================================
// UTestAnimGraphInstance 구현
// ========================================

void UTestAnimGraphInstance::NativeInitializeAnimation()
{
	// 부모 클래스 초기화 호출
	Super::NativeInitializeAnimation();

	UE_LOG("TestAnimGraphInstance::NativeInitializeAnimation - Start");

	// 1. 애니메이션 로드
	LoadAnimations();

	// 2. 하체 StateMachine 생성
	auto* LocomotionSM = CreateLowerBodyStateMachine();

	if (!LocomotionSM)
	{
		UE_LOG("TestAnimGraphInstance::NativeInitializeAnimation - Failed to create LowerBodyStateMachine");
		return;
	}

	// 3. StateMachine을 노드로 래핑
	LowerBodyStateMachine = CreateNode<UAnimNode_StateMachine>();
	LowerBodyStateMachine->StateMachine = LocomotionSM;

	// ========================================
	// 4. 노드 그래프 구성
	// ========================================

	// 현재: 단순 구조 (LowerBodyStateMachine만 사용)
	RootNode = LowerBodyStateMachine;

	// 확장 예시 (주석 처리):
	// 상하체 분리 블렌딩
	// auto* UpperBody = CreateNode<UAnimNode_StateMachine>();
	// UpperBody->StateMachine = CreateUpperBodyStateMachine();
	//
	// auto* Blender = CreateNode<UAnimNode_BlendTwoWay>();
	// Blender->InputA = LowerBodyStateMachine;
	// Blender->InputB = UpperBody;
	// Blender->BlendAlpha = 0.5f;
	//
	// RootNode = Blender;

	UE_LOG("TestAnimGraphInstance::NativeInitializeAnimation - Complete, RootNode set to LowerBodyStateMachine");
}

void UTestAnimGraphInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// 부모 클래스 업데이트 (RootNode->Update() 자동 호출됨)
	Super::NativeUpdateAnimation(DeltaSeconds);

	// ========================================
	// 게임 로직 (개발자가 자유롭게 작성)
	// ========================================

	// 1. 이동 변수 업데이트
	UpdateMovementVariables();

	// 2. StateMachine 전환 조건 체크
	UpdateLowerBodyStateMachine();

	// 확장 예시 (주석 처리):
	// UpdateUpperBodyStateMachine();
	// UpdateFacialStateMachine();
}

// ========================================
// Private 헬퍼 함수들 (개발자가 자유롭게 작성)
// ========================================

void UTestAnimGraphInstance::LoadAnimations()
{
	auto& ResourceManager = UResourceManager::GetInstance();

	// ========================================
	// 디버그: ResourceManager에 등록된 모든 AnimSequence 확인
	// ========================================
	auto AllAnimSequences = ResourceManager.GetAll<UAnimSequence>();
	UE_LOG("=== AnimSequence Cache Debug (TestAnimGraphInstance) ===");
	UE_LOG("Total AnimSequences in ResourceManager: %d", AllAnimSequences.size());

	for (size_t i = 0; i < AllAnimSequences.size(); ++i)
	{
		UAnimSequence* Anim = AllAnimSequences[i];
		if (Anim)
		{
			UE_LOG("  [%d] Path: %s, Length: %.2fs",
				i, Anim->GetFilePath().c_str(), Anim->SequenceLength);
		}
	}
	UE_LOG("========================================================");

	// ========================================
	// 애니메이션 로드
	// ========================================
	if (!IdleAnimation)
	{
		IdleAnimation = ResourceManager.Get<UAnimSequence>(GDataDir + "/Fbx/Idle.fbx");
		if (IdleAnimation)
		{
			UE_LOG("TestAnimGraphInstance: Loaded IdleAnimation via Get()");
		}
		else
		{
			UE_LOG("TestAnimGraphInstance: Failed to Get IdleAnimation - not in cache!");
		}
	}

	if (!WalkAnimation)
	{
		WalkAnimation = ResourceManager.Get<UAnimSequence>(GDataDir + "/Fbx/Walking.fbx");
		if (WalkAnimation)
		{
			UE_LOG("TestAnimGraphInstance: Loaded WalkAnimation via Get()");
		}
		else
		{
			UE_LOG("TestAnimGraphInstance: Failed to Get WalkAnimation - not in cache!");
		}
	}

	if (!RunAnimation)
	{
		RunAnimation = ResourceManager.Get<UAnimSequence>(GDataDir + "/Fbx/Running.fbx");
		if (RunAnimation)
		{
			UE_LOG("TestAnimGraphInstance: Loaded RunAnimation via Get()");
		}
		else
		{
			UE_LOG("TestAnimGraphInstance: Failed to Get RunAnimation - not in cache!");
		}
	}
}

UAnimStateMachine* UTestAnimGraphInstance::CreateLowerBodyStateMachine()
{
	// StateMachine 생성
	auto* SM = NewObject<UAnimStateMachine>(this);

	if (!SM)
	{
		UE_LOG("TestAnimGraphInstance::CreateLowerBodyStateMachine - Failed to create StateMachine");
		return nullptr;
	}

	// State 추가 (애니메이션이 설정되어 있을 때만)
	if (IdleAnimation)
	{
		SM->AddState("Idle", IdleAnimation, true, 1.0f);
	}

	if (WalkAnimation)
	{
		SM->AddState("Walk", WalkAnimation, true, 1.0f);
	}

	if (RunAnimation)
	{
		SM->AddState("Run", RunAnimation, true, 1.0f);  // Run은 1.5배 빠르게
	}

	// Transition 설정 (모든 상태 간 전환 가능)
	SM->AddTransition("Idle", "Walk", 3.0f);  // 3.0초 블렌딩
	SM->AddTransition("Walk", "Run", 3.0f);
	SM->AddTransition("Run", "Idle", 3.0f);
	SM->AddTransition("Walk", "Idle", 3.0f);
	SM->AddTransition("Run", "Walk", 3.0f);

	// 초기 상태 설정
	SM->SetInitialState("Idle");

	UE_LOG("TestAnimGraphInstance::CreateLowerBodyStateMachine - StateMachine created with Idle/Walk/Run states");

	return SM;
}

void UTestAnimGraphInstance::UpdateMovementVariables()
{
	if (!OwnerComponent)
		return;

	// Actor에서 Velocity 가져오기
	AActor* Owner = OwnerComponent->GetOwner();

	// NOTE: CharacterAnimInstance와 동일한 로직 (테스트용 사인파)
	static float x = 0.0f;
	if (Owner)
	{
		FVector Velocity = FVector(1.0f, 1.0f, 1.0f);
		Speed = 200.0f * Velocity.Size() * sinf(x);
		x += 0.0003f;
	}
	else
	{
		Speed = 0.0f;
	}

	// TODO: 실제 게임에서는 Owner->GetVelocity() 사용
	// FVector Velocity = Owner->GetVelocity();
	// Speed = Velocity.Size();

	// TODO: 점프/낙하 감지
	bIsInAir = false;
}

void UTestAnimGraphInstance::UpdateLowerBodyStateMachine()
{
	// StateMachine 유효성 검증
	if (!LowerBodyStateMachine || !LowerBodyStateMachine->StateMachine)
		return;

	// NOTE: CharacterAnimInstance와 동일한 전환 로직

	// Speed 기반 상태 전환
	if (Speed >= 100.0f)
	{
		LowerBodyStateMachine->StateMachine->TransitionTo("Run");
	}
	else if (Speed >= 0.1f)
	{
		LowerBodyStateMachine->StateMachine->TransitionTo("Walk");
	}
	else
	{
		LowerBodyStateMachine->StateMachine->TransitionTo("Idle");
	}
}

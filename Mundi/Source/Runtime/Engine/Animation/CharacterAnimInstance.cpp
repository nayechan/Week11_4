#include "pch.h"
#include "CharacterAnimInstance.h"
#include "AnimSequence.h"
#include "AnimationTypes.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "GlobalConsole.h"
#include "ResourceManager.h"
#include "Character.h"

UCharacterAnimInstance::~UCharacterAnimInstance()
{
	ObjectFactory::DeleteObject(StateMachine);
}

void UCharacterAnimInstance::NativeInitializeAnimation()
{
	// 부모 클래스 초기화 호출
	Super::NativeInitializeAnimation();

	auto& ResourceManager = UResourceManager::GetInstance();

	// ========================================
	// 디버그: ResourceManager에 등록된 모든 AnimSequence 확인
	// ========================================
	auto AllAnimSequences = ResourceManager.GetAll<UAnimSequence>();
	UE_LOG("=== AnimSequence Cache Debug ===");
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
	UE_LOG("================================");

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(OwnerComponent->GetOwner()))
	{
		Character = OwnerCharacter;
	}

	// ========================================
	// 테스트용 애니메이션 하드코딩
	// TODO: Lua/Blueprint에서 설정 가능하도록 변경
	// ========================================
	if (!IdleAnimation)
	{
		IdleAnimation = ResourceManager.Get<UAnimSequence>(GDataDir + "/Fbx/Standing Idle.fbx");
		if (IdleAnimation)
		{
			UE_LOG("CharacterAnimInstance: Loaded IdleAnimation via Get()");
		}
		else
		{
			UE_LOG("CharacterAnimInstance: Failed to Get IdleAnimation - not in cache!");
		}
	}

	if (!WalkAnimation)
	{
		WalkAnimation = ResourceManager.Get<UAnimSequence>(GDataDir + "/Fbx/Walking.fbx");
		if (WalkAnimation)
		{
			UE_LOG("CharacterAnimInstance: Loaded WalkAnimation via Get()");
		}
		else
		{
			UE_LOG("CharacterAnimInstance: Failed to Get WalkAnimation - not in cache!");
		}
	}

	if (!RunAnimation)
	{
		RunAnimation = ResourceManager.Get<UAnimSequence>(GDataDir + "/Fbx/Running.fbx");
		if (RunAnimation)
		{
			UE_LOG("CharacterAnimInstance: Loaded RunAnimation via Get()");
		}
		else
		{
			UE_LOG("CharacterAnimInstance: Failed to Get RunAnimation - not in cache!");
		}
	}

	// ========================================
	// StateMachine 생성
	// ========================================
	StateMachine = NewObject<UAnimStateMachine>();

	if (!StateMachine)
	{
		UE_LOG("CharacterAnimInstance::NativeInitializeAnimation - Failed to create StateMachine");
		return;
	}

	// State 추가 (애니메이션이 설정되어 있을 때만)
	if (IdleAnimation)
	{
		StateMachine->AddState("Idle", IdleAnimation, true, 1.0f);
	}

	if (WalkAnimation)
	{
		StateMachine->AddState("Walk", WalkAnimation, true, 1.0f);
	}

	if (RunAnimation)
	{
		StateMachine->AddState("Run", RunAnimation, true, 1.5f);  // Run은 1.5배 빠르게
	}

	// Transition 설정
	StateMachine->AddTransition("Idle", "Walk", 0.5f);  // 0.5초 블렌딩
	StateMachine->AddTransition("Walk", "Run", 0.3f);   // 0.3초 블렌딩
	StateMachine->AddTransition("Run", "Idle", 0.7f);   // 0.7초 블렌딩
	StateMachine->AddTransition("Walk", "Idle", 0.5f);  // Walk -> Idle 추가
	StateMachine->AddTransition("Run", "Walk", 0.3f);   // Run -> Walk 추가

	// 초기 상태 설정
	StateMachine->SetInitialState("Idle");

	UE_LOG("CharacterAnimInstance::NativeInitializeAnimation - StateMachine initialized with Idle/Walk/Run states");
}

void UCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// ========================================
	// 올바른 실행 순서:
	// 1. Super 호출 - 시간 업데이트
	// 2. 게임 로직 (Movement, StateMachine 조건 체크)
	// 3. StateMachine 업데이트 (Transition 처리)
	//
	// Notify는 UpdateAnimation()에서 자동으로 트리거됨!
	// ========================================

	// 1. 시간 업데이트 (부모 클래스)
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 2. 게임 로직
	UpdateMovementVariables();

	UpdateStateMachine();

	// 3. StateMachine 업데이트 (Transition만 처리)
	if (StateMachine)
	{
		StateMachine->Update(DeltaSeconds);
	}

	// Notify는 UpdateAnimation()에서 자동 트리거됨 (직접 호출 불필요!)
}

void UCharacterAnimInstance::UpdateMovementVariables()
{
	if (!OwnerComponent)
		return;

	// Actor에서 Velocity 가져오기
	//AActor* Owner = OwnerComponent->GetOwner();
	if (Character)
	{
		Speed = Character->GetSpeed();
		//static float x = 0.0f;
		//if (Owner)
		//{
		//	FVector Velocity = FVector(1.0f, 1.0f, 1.0f);
		//	Speed = 5.0f * Velocity.Size() * sinf(x);
		//	x += 0.001f;
		//}
		//else
		//{
		//	Speed = 0.0f;
		//}

		// TODO: 점프/낙하 감지
		bIsInAir = false;
	}
}

void UCharacterAnimInstance::UpdateStateMachine()
{
	if (!StateMachine)
		return;

	FName CurrentState = StateMachine->GetCurrentState();

	if (Speed >= 1.0f)
	{
		StateMachine->TransitionTo("Run");
	}
	else if (Speed >= 0.3f)
	{
		StateMachine->TransitionTo("Walk");
	}
	else
	{
		StateMachine->TransitionTo("Idle");
	}
}

void UCharacterAnimInstance::GetAnimationPose(FPoseContext& OutPose)
{
	if (StateMachine)
	{
		// Unreal 방식: DeltaTime만 전파, 각 노드가 자신의 시간 관리
		// StateMachine이 내부적으로 StateLocalTime을 사용하여:
		// 1. 포즈 추출
		// 2. Notify 수집 -> OutPose.AnimNotifies에 추가
		StateMachine->GetBlendedPose(OutPose);
	}
	else
	{
		OutPose.BoneTransforms.Empty();
	}
}

// ⭐ GetActiveAnimations() 제거
// Unreal 방식에서는 AnimInstance가 직접 Notify를 찾지 않음
// 대신 GetAnimationPose() 호출 시 StateMachine이 Notify를 수집하여
// OutPose.AnimNotifies에 추가함 (트리 누적 패턴)

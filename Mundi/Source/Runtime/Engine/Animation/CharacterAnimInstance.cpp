#include "pch.h"
#include "CharacterAnimInstance.h"
#include "AnimSequence.h"
#include "AnimationTypes.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "SkeletalMesh.h"
#include "VertexData.h"
#include "GlobalConsole.h"
#include "ResourceManager.h"
#include "Character.h"

// 소멸자 제거: StateMachineNode가 UPROPERTY로 GC 관리됨

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
	// .anim 파일에서 애니메이션 로드
	// .anim 파일은 Notify 등 메타데이터를 포함
	// ========================================
	if (!IdleAnimation)
	{
		IdleAnimation = ResourceManager.Get<UAnimSequence>(GDataDir + "/Animations/Standing Idle.anim");
		if (IdleAnimation)
		{
			UE_LOG("CharacterAnimInstance: Loaded IdleAnimation from .anim");
		}
		else
		{
			UE_LOG("CharacterAnimInstance: Failed to load IdleAnimation from .anim!");
		}
	}

	if (!WalkAnimation)
	{
		WalkAnimation = ResourceManager.Get<UAnimSequence>(GDataDir + "/Animations/Walking.anim");
		if (WalkAnimation)
		{
			UE_LOG("CharacterAnimInstance: Loaded WalkAnimation from .anim");
		}
		else
		{
			UE_LOG("CharacterAnimInstance: Failed to load WalkAnimation from .anim!");
		}
	}

	if (!RunAnimation)
	{
		RunAnimation = ResourceManager.Get<UAnimSequence>(GDataDir + "/Animations/Running.anim");
		if (RunAnimation)
		{
			UE_LOG("CharacterAnimInstance: Loaded RunAnimation from .anim");
		}
		else
		{
			UE_LOG("CharacterAnimInstance: Failed to load RunAnimation from .anim!");
		}
	}

	// ========================================
	// BlendSpace1D 생성 (Locomotion)
	// ========================================
	LocomotionBlendSpace = CreateNode<UAnimNode_BlendSpace1D>();

	if (!LocomotionBlendSpace)
	{
		UE_LOG("CharacterAnimInstance::NativeInitializeAnimation - Failed to create BlendSpace1D");
		return;
	}

	// 샘플 추가 (Speed 기반 블렌딩)
	// Sample Value: 0.0 = Idle, 0.2 = Walk, 1.0 = Run
	if (IdleAnimation)
	{
		LocomotionBlendSpace->AddSample(0.0f, IdleAnimation);
		UE_LOG("CharacterAnimInstance: Added Idle sample at 0.0");
	}

	if (WalkAnimation)
	{
		LocomotionBlendSpace->AddSample(0.2f, WalkAnimation);
		UE_LOG("CharacterAnimInstance: Added Walk sample at 0.5");
	}

	if (RunAnimation)
	{
		LocomotionBlendSpace->AddSample(1.0f, RunAnimation);
		UE_LOG("CharacterAnimInstance: Added Run sample at 1.0");
	}

	// BlendSpace 설정
	LocomotionBlendSpace->bLooping = true;
	LocomotionBlendSpace->PlayRate = 1.0f;
	LocomotionBlendSpace->bUsePhaseSync = true;  // 발 위상 동기화

	// RootNode 설정 (부모 클래스가 자동으로 Update/Evaluate 호출)
	RootNode = LocomotionBlendSpace;

	// ========================================
	// 노드 수동 초기화 (Super 호출 후 생성되었으므로)
	// ========================================
	if (OwnerComponent && OwnerComponent->GetSkeletalMesh())
	{
		const USkeletalMesh* SkelMesh = OwnerComponent->GetSkeletalMesh();
		if (SkelMesh->GetSkeletalMeshData())
		{
			const FSkeleton* Skel = SkelMesh->GetSkeletalMeshData()->Skeleton;
			if (Skel)
			{
				LocomotionBlendSpace->Initialize(Skel, this);
				UE_LOG("CharacterAnimInstance: Manually initialized BlendSpace1D node");
			}
		}
	}

	UE_LOG("CharacterAnimInstance::NativeInitializeAnimation - BlendSpace1D created with %d samples, RootNode set",
		LocomotionBlendSpace->Samples.Num());
}

void UCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// ========================================
	// BlendSpace 기반 실행 순서:
	// 1. Super 호출 - RootNode->Update() 자동 호출됨
	// 2. 게임 로직 (Speed 업데이트)
	// 3. 부드러운 속도 보간
	// 4. BlendSpace CurrentParameter 업데이트
	// ========================================

	// 1. 부모 클래스 업데이트 (RootNode->Update() 자동 호출)
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 2. 실제 속도 계산
	UpdateMovementVariables();

	// 3. 부드러운 가속/감속 적용
	UpdateSmoothedSpeed(DeltaSeconds);

	// 4. BlendSpace 파라미터 업데이트
	if (LocomotionBlendSpace)
	{
		LocomotionBlendSpace->CurrentParameter = SmoothedSpeed;
	}
}

void UCharacterAnimInstance::UpdateMovementVariables()
{
	if (!OwnerComponent)
		return;

	if (Character)
	{
		// Character에서 실제 속도 가져오기
		// TODO : Speed 0~1 사이로 정규화하기
		float RawSpeed = Character->GetSpeed();

		// Speed를 0.0 ~ 1.0 범위로 정규화
		// 0.0 = Idle, 0.2 = Walk, 1.0 = Run
		// 가정: RawSpeed 0~1 범위가 이미 정규화된 값
		Speed = FMath::Clamp(RawSpeed/4.0f, 0.0f, 1.0f);

		// TODO: 점프/낙하 감지
		bIsInAir = false;
	}
}

void UCharacterAnimInstance::UpdateSmoothedSpeed(float DeltaSeconds)
{
	// ========================================
	// TODO(human): 부드러운 가속/감속 보간 로직 구현
	// ========================================
	//
	// 목표: Speed(목표값)에서 SmoothedSpeed(현재값)으로 부드럽게 전환
	//
	// 고려사항:
	// - 가속 속도와 감속 속도를 다르게 할 수 있음 (감속이 더 빠르면 반응성 향상)
	// - 선형 보간(FInterpTo) vs 지수 감쇠(exponential smoothing)
	// - DeltaSeconds를 사용하여 프레임 독립적으로 구현
	//
	// 사용 가능한 함수:
	// - FMath::FInterpTo(Current, Target, DeltaTime, InterpSpeed)
	// - FMath::FInterpConstantTo(Current, Target, DeltaTime, InterpSpeed)
	// - 또는 직접 구현: Current += (Target - Current) * Rate * DeltaTime
	//
	// 아래에 SmoothedSpeed 업데이트 로직을 구현해주세요:

	SmoothedSpeed = FMath::Lerp(SmoothedSpeed, Speed, SmoothRate * DeltaSeconds);
}

// ========================================
// GetAnimationPose() 오버라이드 제거
// ========================================
// 노드 기반 시스템에서는 부모 클래스 UAnimGraphInstance가
// RootNode->Evaluate()를 통해 자동으로 처리합니다.
//
// 이점:
// 1. 코드 중복 제거
// 2. 일관된 평가 파이프라인
// 3. 노드 합성 가능 (BlendSpace, 상하체 분리 등)

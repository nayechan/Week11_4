#pragma once
#include "AnimInstance.h"
#include "AnimNode.h"
#include "UAnimGraphInstance.generated.h"

/**
 * @brief 노드 그래프 기반 AnimInstance
 *
 * 기존 UAnimInstance 인터페이스를 유지하며,
 * 내부에 FAnimNode 트리를 사용하는 Adapter
 *
 * 설계 원칙:
 * - Adapter Pattern: 기존 AnimInstance 인터페이스 유지
 * - Composite Pattern: RootNode를 통한 트리 구조
 * - Strategy Pattern: 노드 조합으로 다양한 동작 구현
 *
 * 장점:
 * 1. SkeletalMeshComponent 수정 최소화 (AnimationMode만 추가)
 * 2. 기존 AnimInstance와 호환 (같은 인터페이스)
 * 3. C++ 또는 Lua에서 노드 그래프 구성 가능
 * 4. 런타임에 유연하게 노드 조합 변경
 *
 * 사용 예시 (C++):
 *   class UMyCharacterAnimGraph : public UAnimGraphInstance
 *   {
 *       virtual void NativeInitializeAnimation() override
 *       {
 *           Super::NativeInitializeAnimation();
 *
 *           // Idle 플레이어 생성
 *           auto* IdlePlayer = CreateNode<UAnimNode_SequencePlayer>();
 *           IdlePlayer->Sequence = LoadObject<UAnimSequence>("Data/Fbx/Idle.fbx");
 *           IdlePlayer->bLooping = true;
 *           IdlePlayer->Play();
 *
 *           // Walk 플레이어 생성
 *           auto* WalkPlayer = CreateNode<UAnimNode_SequencePlayer>();
 *           WalkPlayer->Sequence = LoadObject<UAnimSequence>("Data/Fbx/Walking.fbx");
 *           WalkPlayer->bLooping = true;
 *           WalkPlayer->Play();
 *
 *           // 블렌더 생성
 *           auto* Blender = CreateNode<UAnimNode_BlendTwoWay>();
 *           Blender->InputA = IdlePlayer;
 *           Blender->InputB = WalkPlayer;
 *           Blender->BlendAlpha = 0.0f;  // 나중에 Speed로 업데이트
 *
 *           RootNode = Blender;
 *       }
 *
 *       virtual void NativeUpdateAnimation(float DeltaSeconds) override
 *       {
 *           Super::NativeUpdateAnimation(DeltaSeconds);
 *
 *           // 블렌드 알파 업데이트 (속도 기반)
 *           AActor* Owner = GetOwnerComponent()->GetOwner();
 *           float Speed = Owner->GetVelocity().Size();
 *           auto* Blender = Cast<UAnimNode_BlendTwoWay>(RootNode);
 *           Blender->BlendAlpha = FMath::Clamp(Speed / 600.0f, 0.0f, 1.0f);
 *       }
 *   };
 *
 * 사용 예시 (Lua):
 *   function MyAnimGraph:LuaInitializeAnimation()
 *       local idlePlayer = self:CreateNodeByName("UAnimNode_SequencePlayer")
 *       idlePlayer.Sequence = self:LoadAnimSequence("Data/Fbx/Idle.fbx")
 *       idlePlayer.bLooping = true
 *       idlePlayer:Play()
 *
 *       local walkPlayer = self:CreateNodeByName("UAnimNode_SequencePlayer")
 *       walkPlayer.Sequence = self:LoadAnimSequence("Data/Fbx/Walking.fbx")
 *       walkPlayer.bLooping = true
 *       walkPlayer:Play()
 *
 *       local blender = self:CreateNodeByName("UAnimNode_BlendTwoWay")
 *       blender.InputA = idlePlayer
 *       blender.InputB = walkPlayer
 *       blender.BlendAlpha = 0.0
 *
 *       self.RootNode = blender
 *       self.blender = blender  -- 나중에 접근용
 *   end
 *
 *   function MyAnimGraph:LuaUpdateAnimation(DeltaTime)
 *       local owner = self.OwnerComponent:GetOwner()
 *       local speed = owner:GetVelocity():Size()
 *       self.blender.BlendAlpha = math.min(speed / 600.0, 1.0)
 *   end
 */
UCLASS(DisplayName="애니메이션 그래프 인스턴스", Description="노드 기반 애니메이션 시스템")
class UAnimGraphInstance : public UAnimInstance
{
public:
	GENERATED_REFLECTION_BODY()

	UAnimGraphInstance() = default;
	virtual ~UAnimGraphInstance() = default;

	// ========================================
	// 노드 그래프
	// ========================================

	/**
	 * @brief 루트 노드 (포즈 출력의 시작점)
	 *
	 * 트리 구조:
	 *   RootNode (예: BlendTwoWay)
	 *     ├─> InputA (예: SequencePlayer - Idle)
	 *     └─> InputB (예: SequencePlayer - Walk)
	 *
	 * 실행 순서:
	 *   1. Update: RootNode->Update(DeltaTime)
	 *   2. Evaluate: RootNode->Evaluate(OutPose)
	 *
	 * 주의:
	 * - nullptr이면 RefPose 반환
	 * - 순환 참조 방지 (노드가 자기 자신을 참조하면 안 됨)
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[AnimGraph]", Tooltip="루트 노드")
	UAnimNode* RootNode = nullptr;

	/**
	 * @brief 노드 풀 (생명주기 관리)
	 *
	 * 역할:
	 * - CreateNode<T>()로 생성된 모든 노드 저장
	 * - UPROPERTY로 GC 관리 (자동 메모리 해제)
	 * - 소멸 시 모든 노드 자동 정리
	 *
	 * 주의:
	 * - 이 배열에 없는 노드는 GC에 의해 삭제될 수 있음
	 * - 수동으로 NewObject<>()한 노드는 반드시 Nodes.Add() 필요
	 */
	UPROPERTY()
	TArray<UAnimNode*> Nodes;

	// ========================================
	// 노드 생성 헬퍼
	// ========================================

	/**
	 * @brief 새 노드 생성 및 등록 (C++ 템플릿)
	 * @tparam T 노드 타입 (UAnimNode 상속)
	 * @return 생성된 노드 포인터
	 *
	 * 사용 예시:
	 *   auto* Player = CreateNode<UAnimNode_SequencePlayer>();
	 *   Player->Sequence = IdleAnimation;
	 *   Player->Play();
	 *
	 * 동작:
	 * 1. NewObject<T>(this) - 리플렉션 기반 생성
	 * 2. Nodes.Add(Node) - GC 관리 등록
	 * 3. return Node
	 *
	 * 주의:
	 * - T는 반드시 UAnimNode 상속 클래스여야 함 (컴파일 타임 체크)
	 * - Initialize()는 NativeInitializeAnimation()에서 일괄 호출됨
	 */
	template<typename T>
	T* CreateNode()
	{
		static_assert(std::is_base_of<UAnimNode, T>::value,
			"T must inherit from UAnimNode");

		T* Node = NewObject<T>(this);
		Nodes.Add(Node);
		return Node;
	}

	/**
	 * @brief 새 노드 생성 (Lua용, 타입 이름으로)
	 * @param NodeTypeName 노드 타입 이름 (예: "UAnimNode_SequencePlayer")
	 * @return 생성된 노드 포인터 (실패 시 nullptr)
	 *
	 * 사용 예시 (Lua):
	 *   local player = self:CreateNodeByName("UAnimNode_SequencePlayer")
	 *   if player then
	 *       player.Sequence = myAnim
	 *       player:Play()
	 *   end
	 *
	 * 동작:
	 * 1. 리플렉션 시스템으로 타입 이름 조회
	 * 2. UClass 찾기 (FindObject<UClass>)
	 * 3. NewObject로 인스턴스 생성
	 * 4. Nodes에 등록
	 *
	 * 주의:
	 * - 잘못된 타입 이름 → nullptr 반환 + 경고 로그
	 * - UAnimNode 상속이 아닌 타입 → nullptr 반환
	 */
	UFUNCTION(LuaBind, DisplayName="CreateNodeByName")
	UAnimNode* CreateNodeByName(const FString& NodeTypeName);

	// ========================================
	// UAnimInstance 인터페이스 구현
	// ========================================

	/**
	 * @brief 초기화: 모든 노드 초기화
	 *
	 * 호출 시점: SkeletalMeshComponent::BeginPlay → AnimInstance->InitializeAnimation()
	 *
	 * 동작:
	 * 1. Super::NativeInitializeAnimation() - 부모 로직 실행
	 * 2. Skeleton 정보 가져오기
	 * 3. 모든 노드의 Initialize(Skeleton, this) 호출
	 *
	 * 주의:
	 * - OwnerComponent나 SkeletalMesh가 없으면 스킵
	 * - 하위 클래스에서 오버라이드 시 Super::NativeInitializeAnimation() 필수
	 */
	virtual void NativeInitializeAnimation() override;

	/**
	 * @brief 업데이트: 루트 노드부터 트리 순회
	 *
	 * 호출 시점: 매 프레임 TickAnimation → UpdateAnimation → NativeUpdateAnimation
	 *
	 * 동작:
	 * 1. Super::NativeUpdateAnimation(DeltaSeconds) - 부모 로직
	 * 2. RootNode->Update(DeltaSeconds) - 트리 전체 업데이트
	 *
	 * 트리 순회:
	 * - RootNode->Update()가 자식 노드들의 Update() 호출
	 * - Composite Pattern으로 재귀적 업데이트
	 *
	 * 주의:
	 * - RootNode가 nullptr이면 아무것도 안 함
	 * - 하위 클래스에서 블렌드 알파 등 업데이트 가능
	 */
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/**
	 * @brief 포즈 추출: 루트 노드에 위임
	 *
	 * 호출 시점: TickAnimation → GetAnimationPose
	 *
	 * 동작:
	 * 1. RootNode가 있으면 RootNode->Evaluate(OutPose) 호출
	 * 2. RootNode가 없으면 RefPose 반환
	 *
	 * 트리 순회:
	 * - RootNode->Evaluate()가 자식 노드들의 Evaluate() 호출
	 * - 각 노드가 OutPose.AnimNotifies에 Notify 추가 (트리 누적)
	 * - 최종적으로 Root로 돌아올 때 모든 Notify 수집 완료
	 *
	 * 기존 AnimInstance와의 호환성:
	 * - 같은 인터페이스 (GetAnimationPose)
	 * - 같은 출력 (FPoseContext)
	 * - SkeletalMeshComponent는 차이를 모름!
	 */
	virtual void GetAnimationPose(FPoseContext& OutPose) override;
};

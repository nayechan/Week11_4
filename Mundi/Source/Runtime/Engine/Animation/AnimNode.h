#pragma once
#include "Object.h"
#include "AnimationTypes.h"
#include "UAnimNode.generated.h"

// 전방 선언 (순환 참조 방지)
class UAnimInstance;
struct FSkeleton;

/**
 * @brief 애니메이션 노드 베이스 클래스
 *
 * 설계 원칙:
 * - Single Responsibility: 각 노드는 하나의 역할 (재생, 블렌딩, 변환 등)
 * - Open/Closed: 새 노드 추가 시 기존 코드 수정 불필요
 * - Liskov Substitution: 모든 노드는 UAnimNode 인터페이스로 교체 가능
 *
 * Unreal Engine UAnimNode 참고 (차이점 주의):
 * - Initialize: 스켈레톤 정보 캐싱, 본 인덱스 조회 테이블 생성 등
 * - Update: 시간 진행 및 상태 업데이트 (Node-Centric Time Management)
 * - Evaluate: 포즈 생성 (트리 순회의 핵심)
 *
 * 주요 차이점:
 * - Unreal의 UAnimNode는 일반 구조체, 우리는 UAnimNode (UObject 상속으로 리플렉션/GC 활용)
 * - Unreal은 FAnimationBaseContext 사용, 우리는 FPoseContext 사용
 * - 복잡한 컴파일 타임 최적화 제거 (CRTP 등)
 */
UCLASS(Abstract, DisplayName="애니메이션 노드", Description="애니메이션 그래프의 기본 단위")
class UAnimNode : public UObject
{
public:
	GENERATED_REFLECTION_BODY()

	UAnimNode() = default;
	virtual ~UAnimNode() = default;

	// ========================================
	// 핵심 인터페이스 (노드 생명주기)
	// ========================================

	/**
	 * @brief 노드 초기화
	 * @param InSkeleton 타겟 스켈레톤 (본 구조 정보)
	 * @param InOwner 소유 AnimInstance
	 *
	 * 호출 시점: AnimInstance::InitializeAnimation()
	 *
	 * 용도:
	 * - 스켈레톤 정보 캐싱 (본 인덱스 조회 등)
	 * - 본 마스크 미리 계산 (문자열 비교 회피)
	 * - 자식 노드 초기화
	 *
	 * 주의사항:
	 * - Skeleton이 nullptr일 수 있음 (방어 코드 필요)
	 * - 여러 번 호출될 수 있음 (리셋 로직 필요)
	 *
	 * 예시 (하위 클래스):
	 *   virtual void Initialize(const FSkeleton* InSkeleton, UAnimInstance* InOwner) override
	 *   {
	 *       UAnimNode::Initialize(InSkeleton, InOwner);
	 *
	 *       // 본 인덱스 캐싱
	 *       if (Skeleton)
	 *       {
	 *           SpineBoneIndex = Skeleton->BoneNameToIndex.FindRef("Spine");
	 *       }
	 *   }
	 */
	virtual void Initialize(const FSkeleton* InSkeleton, UAnimInstance* InOwner)
	{
		Skeleton = InSkeleton;
		Owner = InOwner;
	}

	/**
	 * @brief 시간 진행 및 상태 업데이트
	 * @param DeltaTime 프레임 델타 시간 (초)
	 *
	 * 호출 시점: AnimInstance::UpdateAnimation() → RootNode->Update()
	 *
	 * 중요: Node-Centric Time Management
	 * - 각 노드는 자신의 InternalTime을 관리
	 * - AnimInstance는 전역 시간을 관리하지 않음
	 * - 트리 구조에서 부모가 자식 Update() 호출
	 *
	 * 예시 (SequencePlayer):
	 *   virtual void Update(float DeltaTime) override
	 *   {
	 *       if (bIsPlaying)
	 *       {
	 *           InternalTime += DeltaTime * PlayRate;
	 *           if (bLooping && InternalTime >= SequenceLength)
	 *               InternalTime = fmodf(InternalTime, SequenceLength);
	 *       }
	 *   }
	 *
	 * 예시 (Blend):
	 *   virtual void Update(float DeltaTime) override
	 *   {
	 *       if (InputA) InputA->Update(DeltaTime);
	 *       if (InputB) InputB->Update(DeltaTime);
	 *   }
	 */
	virtual void Update(float DeltaTime) {}

	/**
	 * @brief 포즈 생성 (트리 순회의 핵심) - 순수 가상 함수
	 * @param OutPose 출력 포즈 컨텍스트
	 *
	 * 호출 시점: AnimInstance::GetAnimationPose() → RootNode->Evaluate()
	 *
	 * 트리 순회 패턴 (Composite Pattern):
	 * 1. 자식 노드가 있으면 Evaluate() 호출하여 자식 포즈 수집
	 * 2. 자신의 로직 적용 (블렌딩, 시퀀스 샘플링, 본 변환 등)
	 * 3. OutPose에 결과 저장
	 * 4. OutPose.AnimNotifies에 Notify 추가 (트리 누적)
	 *
	 * 예시 (SequencePlayer):
	 *   virtual void Evaluate(FPoseContext& OutPose) override
	 *   {
	 *       if (!Sequence || !Skeleton)
	 *       {
	 *           OutPose.SetNumBones(Skeleton ? Skeleton->Bones.Num() : 0);
	 *           return;
	 *       }
	 *
	 *       // 1. 포즈 추출
	 *       FAnimExtractContext ExtractContext(InternalTime, bLooping);
	 *       Sequence->GetAnimationPose(OutPose, ExtractContext);
	 *
	 *       // 2. Notify 수집 (트리 누적)
	 *       TArray<FAnimNotifyEvent> Notifies = Sequence->GetAnimNotifiesInRange(
	 *           PreviousTime, InternalTime, bLooping
	 *       );
	 *       OutPose.AnimNotifies.Append(Notifies);
	 *   }
	 *
	 * 예시 (Blend):
	 *   virtual void Evaluate(FPoseContext& OutPose) override
	 *   {
	 *       // 1. 자식 노드 Evaluate
	 *       FPoseContext PoseA, PoseB;
	 *       InputA->Evaluate(PoseA);
	 *       InputB->Evaluate(PoseB);
	 *
	 *       // 2. 블렌딩 로직 적용
	 *       FAnimationRuntime::BlendTwoPosesTogether(PoseA, PoseB, Alpha, OutPose);
	 *
	 *       // 3. Notify 합치기 (트리 누적)
	 *       OutPose.AnimNotifies.Append(PoseA.AnimNotifies);
	 *       OutPose.AnimNotifies.Append(PoseB.AnimNotifies);
	 *   }
	 */
	virtual void Evaluate(FPoseContext& OutPose) = 0;

	// ========================================
	// 헬퍼 함수 (디버깅/UI용)
	// ========================================

	/**
	 * @brief 현재 재생 시간 반환
	 * @return 노드의 InternalTime (초)
	 *
	 * 용도:
	 * - 디버깅 (현재 애니메이션 진행 상황 확인)
	 * - UI (타임라인 표시)
	 * - 동기화 (여러 노드 간 시간 정렬)
	 */
	virtual float GetCurrentTime() const { return 0.0f; }

	/**
	 * @brief 노드 타입 이름 반환
	 * @return 노드 타입 문자열 (예: "SequencePlayer", "BlendTwoWay")
	 *
	 * 용도:
	 * - 디버깅 로그
	 * - 비주얼 노드 그래프 에디터
	 * - 프로파일링
	 */
	virtual FString GetNodeName() const { return "AnimNode"; }

protected:
	// ========================================
	// 내부 상태 (하위 클래스에서 접근 가능)
	// ========================================

	/**
	 * @brief 스켈레톤 정보 (본 인덱스 조회용)
	 *
	 * 사용 예시:
	 *   int32 SpineIndex = Skeleton->BoneNameToIndex.FindRef("Spine");
	 *   const FBone& SpineBone = Skeleton->Bones[SpineIndex];
	 *
	 * 주의:
	 * - const로 보호 (읽기 전용)
	 * - nullptr 체크 필수
	 * - Initialize()에서 설정됨
	 */
	const FSkeleton* Skeleton = nullptr;

	/**
	 * @brief 소유 AnimInstance (World 접근, Notify 트리거 등)
	 *
	 * 사용 예시:
	 *   UWorld* World = Owner->GetWorld();
	 *   USkeletalMeshComponent* Component = Owner->GetOwnerComponent();
	 *
	 * 주의:
	 * - nullptr 체크 필수
	 * - Initialize()에서 설정됨
	 */
	UAnimInstance* Owner = nullptr;
};

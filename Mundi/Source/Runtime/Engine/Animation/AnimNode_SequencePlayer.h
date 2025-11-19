#pragma once
#include "AnimNode.h"
#include "UAnimNode_SequencePlayer.generated.h"

// 전방 선언
class UAnimSequence;

/**
 * @brief AnimSequence 재생 노드
 *
 * 기존 UAnimSingleNodeInstance의 로직을 노드화
 *
 * 주요 기능:
 * - AnimSequence를 InternalTime 기준으로 샘플링
 * - 루핑/비루핑 모드 지원
 * - PlayRate로 재생 속도 조절
 * - Notify 수집 및 트리 누적
 *
 * 사용 예시:
 *   auto* Player = Instance->CreateNode<UAnimNode_SequencePlayer>();
 *   Player->Sequence = IdleAnimation;
 *   Player->bLooping = true;
 *   Player->PlayRate = 1.0f;
 *   Player->Play();
 *   Instance->RootNode = Player;
 *
 * Node-Centric Time Management:
 * - InternalTime/PreviousTime를 자체 관리
 * - AnimInstance는 시간을 관리하지 않음
 * - Update()에서 DeltaTime만큼 InternalTime 증가
 */
UCLASS(DisplayName="시퀀스 플레이어", Description="AnimSequence를 재생하는 노드")
class UAnimNode_SequencePlayer : public UAnimNode
{
public:
	GENERATED_REFLECTION_BODY()

	UAnimNode_SequencePlayer() = default;
	virtual ~UAnimNode_SequencePlayer() = default;

	// ========================================
	// 프로퍼티 (Lua/UI에서 설정 가능)
	// ========================================

	/**
	 * @brief 재생할 애니메이션 시퀀스
	 *
	 * 주의:
	 * - nullptr일 경우 RefPose 반환
	 * - Skeleton과 호환되는지 확인 필요 (SkeletonID)
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[Animation]", Tooltip="재생할 애니메이션 시퀀스")
	UAnimSequence* Sequence = nullptr;

	/**
	 * @brief 재생 속도 배율
	 *
	 * - 1.0 = 정상 속도
	 * - 2.0 = 2배속
	 * - 0.5 = 0.5배속
	 * - 최소값: 0.1 (너무 느린 재생 방지)
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[Animation]", Tooltip="재생 속도 배율", Range="0.1, 10.0")
	float PlayRate = 1.0f;

	/**
	 * @brief 루핑 모드
	 *
	 * - true: 끝까지 재생 후 처음으로 돌아감
	 * - false: 끝까지 재생 후 정지
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[Animation]", Tooltip="루핑 모드")
	bool bLooping = false;

	// ========================================
	// 재생 제어 API (Lua 바인딩)
	// ========================================

	/**
	 * @brief 재생 시작
	 *
	 * InternalTime은 유지됨 (일시정지 후 재개 가능)
	 */
	UFUNCTION(LuaBind, DisplayName="Play")
	void Play()
	{
		bIsPlaying = true;
	}

	/**
	 * @brief 재생 정지 및 시간 리셋
	 *
	 * InternalTime을 0으로 초기화
	 */
	UFUNCTION(LuaBind, DisplayName="Stop")
	void Stop()
	{
		bIsPlaying = false;
		InternalTime = 0.0f;
		PreviousTime = 0.0f;
	}

	/**
	 * @brief 일시정지 (시간 유지)
	 *
	 * InternalTime은 유지됨
	 */
	UFUNCTION(LuaBind, DisplayName="Pause")
	void Pause()
	{
		bIsPlaying = false;
	}

	/**
	 * @brief 재생 속도 설정
	 * @param NewRate 새 재생 속도 (최소 0.1)
	 */
	UFUNCTION(LuaBind, DisplayName="SetPlayRate")
	void SetPlayRate(float NewRate)
	{
		PlayRate = FMath::Max(0.1f, NewRate);
	}

	/**
	 * @brief 현재 시간 설정 (특정 시간으로 점프)
	 * @param NewTime 새 재생 시간 (초)
	 */
	UFUNCTION(LuaBind, DisplayName="SetCurrentTime")
	void SetCurrentTime(float NewTime)
	{
		PreviousTime = InternalTime;
		InternalTime = NewTime;
	}

	// ========================================
	// 상태 조회 API
	// ========================================

	UFUNCTION(LuaBind, DisplayName="IsPlaying")
	bool IsPlaying() const { return bIsPlaying; }

	UFUNCTION(LuaBind, DisplayName="IsLooping")
	bool IsLooping() const { return bLooping; }

	UFUNCTION(LuaBind, DisplayName="GetPlayRate")
	float GetPlayRate() const { return PlayRate; }

	UFUNCTION(LuaBind, DisplayName="GetSequence")
	UAnimSequence* GetSequence() const { return Sequence; }

	// ========================================
	// UAnimNode 인터페이스 구현
	// ========================================

	/**
	 * @brief 시간 진행 (Node-Centric)
	 *
	 * 로직 (AnimSingleNodeInstance::NativeUpdateAnimation 참고):
	 * 1. bIsPlaying이 false면 아무것도 안 함
	 * 2. PreviousTime = InternalTime (Notify 범위 체크용)
	 * 3. InternalTime += DeltaTime * PlayRate
	 * 4. 루핑 처리:
	 *    - bLooping true: fmod로 시간 래핑
	 *    - bLooping false: 끝에 도달하면 정지
	 */
	virtual void Update(float DeltaTime) override
	{
		if (!bIsPlaying || !Sequence)
			return;

		// Notify 범위 체크용
		PreviousTime = InternalTime;
		InternalTime += DeltaTime * PlayRate;

		// 애니메이션 길이
		const float AnimLength = Sequence->GetPlayLength();

		if (InternalTime >= AnimLength)
		{
			if (bLooping)
			{
				// 루핑: 시작으로 돌아가기
				if (AnimLength > 0.0f)
					InternalTime = fmodf(InternalTime, AnimLength);
			}
			else
			{
				// 비루핑: 끝에서 정지
				InternalTime = AnimLength;
				bIsPlaying = false;
			}
		}
	}

	/**
	 * @brief 포즈 추출 + Notify 수집
	 *
	 * 로직 (AnimSingleNodeInstance::GetAnimationPose 참고):
	 * 1. Sequence가 없으면 RefPose 반환
	 * 2. InternalTime 기준으로 Sequence->GetAnimationPose() 호출
	 * 3. PreviousTime ~ InternalTime 범위의 Notify 수집
	 * 4. OutPose.AnimNotifies에 Append (트리 누적 패턴)
	 */
	virtual void Evaluate(FPoseContext& OutPose) override
	{
		if (!Sequence || !Skeleton)
		{
			// RefPose 반환
			OutPose.SetNumBones(Skeleton ? Skeleton->Bones.Num() : 0);
			return;
		}

		// 1. 포즈 추출
		FAnimExtractContext ExtractContext(InternalTime, bLooping);
		Sequence->GetAnimationPose(OutPose, ExtractContext);

		// 2. Notify 수집 (PreviousTime ~ InternalTime 범위)
		TArray<FAnimNotifyEvent> Notifies;
		Sequence->GetAnimNotifiesInRange(PreviousTime, InternalTime, Notifies);

		// 3. 트리 누적 패턴: OutPose.AnimNotifies에 추가
		OutPose.AnimNotifies.Append(Notifies);
	}

	/**
	 * @brief 현재 재생 시간 반환
	 */
	virtual float GetCurrentTime() const override
	{
		return InternalTime;
	}

	/**
	 * @brief 노드 이름 반환 (디버깅용)
	 */
	virtual FString GetNodeName() const override
	{
		if (Sequence)
			return "SequencePlayer(" + Sequence->GetName() + ")";
		else
			return "SequencePlayer(None)";
	}

private:
	// ========================================
	// 내부 상태 (Node-Centric Time Management)
	// ========================================

	/**
	 * @brief 현재 재생 시간 (초)
	 *
	 * - Update()에서 증가
	 * - Evaluate()에서 샘플링에 사용
	 */
	float InternalTime = 0.0f;

	/**
	 * @brief 이전 프레임의 재생 시간 (초)
	 *
	 * - Notify 범위 체크용 (PreviousTime ~ InternalTime)
	 * - Loop wrap-around 감지
	 */
	float PreviousTime = 0.0f;

	/**
	 * @brief 재생 중 여부
	 *
	 * - Play(): true
	 * - Stop()/Pause(): false
	 * - 비루핑 애니메이션 끝: false
	 */
	bool bIsPlaying = false;
};

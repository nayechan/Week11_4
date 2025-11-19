#pragma once
#include "AnimNode.h"
#include "AnimationTypes.h"
#include "AnimationRuntime.h"
#include "UAnimNode_BlendSpace1D.generated.h"

// 전방 선언
class UAnimSequence;

/**
 * @brief 1D BlendSpace 노드
 *
 * 하나의 파라미터(예: 속도)에 따라 여러 애니메이션을 선형 보간
 *
 * 핵심 개념:
 * - Samples: 파라미터 값 → 애니메이션 매핑
 * - CurrentParameter: Lua/C++에서 설정 (예: 캐릭터 속도)
 * - FindBlendSamples(): 현재 파라미터에 맞는 두 샘플 찾기
 * - Evaluate(): 두 샘플 블렌딩
 *
 * 사용 예시 (Lua):
 *   local blendSpace = self:CreateNodeByName("UAnimNode_BlendSpace1D")
 *   blendSpace:AddSample(0.0, IdleAnim)
 *   blendSpace:AddSample(5.0, WalkAnim)
 *   blendSpace:AddSample(10.0, RunAnim)
 *   blendSpace.bLooping = true
 *   blendSpace.PlayRate = 1.0
 *   self.RootNode = blendSpace
 *
 *   -- Update에서
 *   blendSpace.CurrentParameter = speed  -- 자동 블렌딩!
 *
 * 장점:
 * - 부드러운 속도 전환 (State Machine의 급격한 전환과 대비)
 * - 파라미터 기반 제어 (조건 분기 불필요)
 * - AAA급 Locomotion 시스템
 *
 * 제한사항:
 * - 1D만 지원 (2D는 UAnimNode_BlendSpace2D로 별도 구현)
 * - Samples는 SampleValue 오름차순 정렬 필요
 */
UCLASS(DisplayName="BlendSpace 1D", Description="1D 파라미터에 따라 애니메이션 블렌딩")
class UAnimNode_BlendSpace1D : public UAnimNode
{
public:
	GENERATED_REFLECTION_BODY()

	UAnimNode_BlendSpace1D() = default;
	virtual ~UAnimNode_BlendSpace1D() = default;

	// ========================================
	// 프로퍼티 (Lua/UI에서 설정 가능)
	// ========================================

	/**
	 * @brief BlendSpace 샘플 목록
	 *
	 * 주의:
	 * - SampleValue 오름차순으로 정렬 필요
	 * - Animation이 nullptr인 샘플은 무시됨
	 *
	 * 예시:
	 * - Samples[0] = {0.0, Idle}
	 * - Samples[1] = {5.0, Walk}
	 * - Samples[2] = {10.0, Run}
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[BlendSpace]", Tooltip="BlendSpace 샘플 목록 (SampleValue 오름차순)")
	TArray<FBlendSample1D> Samples;

	/**
	 * @brief 현재 파라미터 값 (Lua/C++에서 설정)
	 *
	 * 예시:
	 * - Speed = 0.0 → Idle (100%)
	 * - Speed = 3.0 → Idle (40%) + Walk (60%)
	 * - Speed = 7.5 → Walk (50%) + Run (50%)
	 * - Speed = 10.0 → Run (100%)
	 *
	 * Lua 사용:
	 *   blendSpace.CurrentParameter = self.Variables["Speed"]
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[BlendSpace]", Tooltip="현재 파라미터 값 (예: 속도)")
	float CurrentParameter = 0.0f;

	/**
	 * @brief 재생 속도 배율
	 *
	 * - 1.0 = 정상 속도
	 * - 2.0 = 2배속
	 * - 모든 샘플에 동일하게 적용
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[BlendSpace]", Tooltip="재생 속도 배율", Range="0.1, 10.0")
	float PlayRate = 1.0f;

	/**
	 * @brief 루핑 모드
	 *
	 * - true: 모든 샘플 루핑
	 * - false: 끝까지 재생 후 정지
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[BlendSpace]", Tooltip="루핑 모드")
	bool bLooping = true;

	/**
	 * @brief Phase Synchronization 활성화 (Unreal Engine 스타일)
	 *
	 * Phase Sync: 가중치 높은 샘플(Leader)의 정규화된 시간을 기준으로 동기화
	 *
	 * 사용 케이스:
	 * - true: Locomotion (Idle/Walk/Run) - 발 위상 동기화 필수
	 * - false: One-shot (Attack/Jump) - 독립 재생
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[BlendSpace]", Tooltip="Phase Synchronization")
	bool bUsePhaseSync = true;

	// ========================================
	// Lua 편의 함수 (샘플 추가)
	// ========================================

	/**
	 * @brief 샘플 추가 (Lua에서 편리하게 사용)
	 * @param SampleValue 파라미터 값
	 * @param Animation 애니메이션
	 *
	 * Lua 예시:
	 *   blendSpace:AddSample(0.0, IdleAnim)
	 *   blendSpace:AddSample(5.0, WalkAnim)
	 *   blendSpace:AddSample(10.0, RunAnim)
	 */
	UFUNCTION(LuaBind, DisplayName="AddSample")
	void AddSample(float SampleValue, UAnimSequence* Animation)
	{
		if (!Animation)
			return;

		FBlendSample1D NewSample;
		NewSample.SampleValue = SampleValue;
		NewSample.Animation = Animation;
		NewSample.InternalTime = 0.0f;
		NewSample.PreviousTime = 0.0f;

		Samples.Add(NewSample);

		// SampleValue 오름차순 정렬
		Samples.Sort([](const FBlendSample1D& A, const FBlendSample1D& B)
		{
			return A.SampleValue < B.SampleValue;
		});
	}

	/**
	 * @brief 모든 샘플 제거
	 */
	UFUNCTION(LuaBind, DisplayName="ClearSamples")
	void ClearSamples()
	{
		Samples.Empty();
	}

	// ========================================
	// UAnimNode 인터페이스 구현
	// ========================================

	/**
	 * @brief 시간 진행 (Phase Synchronization 지원)
	 *
	 * 두 가지 모드:
	 *
	 * 1. bUsePhaseSync = false (독립 재생)
	 *    - 각 샘플이 독립적으로 시간 진행
	 *    - One-shot 애니메이션에 적합 (Attack, Jump 등)
	 *
	 * 2. bUsePhaseSync = true (Unreal Engine 스타일)
	 *    - Weight-based Leader Selection
	 *    - Leader의 정규화된 시간을 모든 샘플에 동기화
	 *    - Locomotion에 적합 (Idle/Walk/Run)
	 *
	 * Phase Sync 알고리즘:
	 * 1. FindBlendSamples(): 현재 블렌딩되는 두 샘플 찾기
	 * 2. Leader 결정: BlendAlpha < 0.5 ? Lower : Upper
	 * 3. Leader 시간 진행 (normalized time 기준)
	 * 4. 모든 샘플에 Leader의 NormalizedTime 적용
	 *
	 * 예시 (Speed = 3.0, Alpha = 0.6):
	 * - Leader = Walk (가중치 60%)
	 * - Walk의 NormalizedTime = 0.25 (25% 지점, 왼발 앞)
	 * - Idle도 NormalizedTime = 0.25로 동기화
	 * - 결과: Idle 25% + Walk 25% = 같은 발 위상!
	 */
	virtual void Update(float DeltaTime) override
	{
		if (DeltaTime <= 0.0f || Samples.Num() == 0)
			return;

		// ========================================
		// 모드 1: 독립 재생 (Phase Sync 비활성화)
		// ========================================
		if (!bUsePhaseSync)
		{
			// 각 샘플이 독립적으로 시간 진행
			for (FBlendSample1D& Sample : Samples)
			{
				if (!Sample.Animation)
					continue;

				Sample.PreviousTime = Sample.InternalTime;
				Sample.InternalTime += DeltaTime * PlayRate;

				const float AnimLength = Sample.Animation->GetPlayLength();

				if (bLooping)
				{
					if (AnimLength > 0.0f)
						Sample.InternalTime = fmodf(Sample.InternalTime, AnimLength);
				}
				else
				{
					Sample.InternalTime = FMath::Min(Sample.InternalTime, AnimLength);
				}
			}
			return;
		}

		// ========================================
		// 모드 2: Phase Synchronization (Unreal 스타일)
		// ========================================

		// Step 1: 현재 블렌딩되는 샘플 찾기
		int32 LowerIdx, UpperIdx;
		float BlendAlpha;
		FindBlendSamples(CurrentParameter, LowerIdx, UpperIdx, BlendAlpha);

		// Step 2: Leader 결정 (가중치 높은 샘플)
		// - BlendAlpha < 0.5 → Lower 가중치 높음 (1-Alpha > 0.5)
		// - BlendAlpha >= 0.5 → Upper 가중치 높음 (Alpha >= 0.5)
		int32 LeaderIdx = (BlendAlpha < 0.5f) ? LowerIdx : UpperIdx;
		FBlendSample1D& LeaderSample = Samples[LeaderIdx];

		if (!LeaderSample.Animation)
		{
			// Leader가 유효하지 않으면 독립 재생으로 폴백
			for (FBlendSample1D& Sample : Samples)
			{
				if (!Sample.Animation)
					continue;

				Sample.PreviousTime = Sample.InternalTime;
				Sample.InternalTime += DeltaTime * PlayRate;

				const float AnimLength = Sample.Animation->GetPlayLength();
				if (bLooping && AnimLength > 0.0f)
					Sample.InternalTime = fmodf(Sample.InternalTime, AnimLength);
				else
					Sample.InternalTime = FMath::Min(Sample.InternalTime, AnimLength);
			}
			return;
		}

		const float LeaderLength = LeaderSample.Animation->GetPlayLength();
		if (LeaderLength <= 0.0f)
			return;

		// Step 3: Leader의 정규화된 시간 계산 및 진행
		float LeaderNormalizedTime = LeaderSample.InternalTime / LeaderLength;

		LeaderSample.PreviousTime = LeaderSample.InternalTime;

		// Leader 시간 진행 (normalized time 기준)
		LeaderNormalizedTime += (DeltaTime * PlayRate) / LeaderLength;

		// 루핑/클램핑 (normalized time 기준)
		if (bLooping)
		{
			LeaderNormalizedTime = fmodf(LeaderNormalizedTime, 1.0f);
		}
		else
		{
			LeaderNormalizedTime = FMath::Min(LeaderNormalizedTime, 1.0f);
		}

		// Leader의 절대 시간 업데이트
		LeaderSample.InternalTime = LeaderNormalizedTime * LeaderLength;

		// Step 4: 모든 샘플을 Leader의 NormalizedTime에 동기화
		for (int32 i = 0; i < Samples.Num(); ++i)
		{
			if (i == LeaderIdx)
				continue; // Leader는 이미 업데이트됨

			FBlendSample1D& Sample = Samples[i];
			if (!Sample.Animation)
				continue;

			const float SampleLength = Sample.Animation->GetPlayLength();
			if (SampleLength <= 0.0f)
				continue;

			Sample.PreviousTime = Sample.InternalTime;

			// Leader의 정규화된 시간을 이 샘플의 절대 시간으로 변환
			Sample.InternalTime = LeaderNormalizedTime * SampleLength;
		}
	}

	/**
	 * @brief 포즈 생성 (블렌딩)
	 *
	 * 로직:
	 * 1. FindBlendSamples(): CurrentParameter에 맞는 샘플 찾기
	 * 2. ExtractPose(): 각 샘플의 포즈 추출
	 * 3. BlendTwoPosesTogether(): 두 포즈 블렌딩
	 * 4. AnimNotifies 누적
	 */
	virtual void Evaluate(FPoseContext& OutPose) override
	{
		if (Samples.Num() == 0 || !Skeleton)
		{
			// RefPose 반환
			OutPose.SetNumBones(Skeleton ? Skeleton->Bones.Num() : 0);
			return;
		}

		// 1. 블렌딩할 샘플 찾기
		int32 LowerIdx, UpperIdx;
		float BlendAlpha;
		FindBlendSamples(CurrentParameter, LowerIdx, UpperIdx, BlendAlpha);

		// 2. 샘플 유효성 체크
		if (!Samples[LowerIdx].Animation)
		{
			OutPose.SetNumBones(Skeleton->Bones.Num());
			return;
		}

		if (LowerIdx == UpperIdx)
		{
			// 정확히 샘플 위치 (블렌딩 불필요)
			ExtractPose(Samples[LowerIdx], OutPose);
		}
		else
		{
			// 두 샘플 블렌딩
			if (!Samples[UpperIdx].Animation)
			{
				// Upper 샘플이 없으면 Lower만 사용
				ExtractPose(Samples[LowerIdx], OutPose);
			}
			else
			{
				// 정상 블렌딩
				FPoseContext PoseA, PoseB;
				ExtractPose(Samples[LowerIdx], PoseA);
				ExtractPose(Samples[UpperIdx], PoseB);

				FAnimationRuntime::BlendTwoPosesTogether(PoseA, PoseB, BlendAlpha, OutPose);

				// Notify 누적
				OutPose.AnimNotifies.Append(PoseA.AnimNotifies);
				OutPose.AnimNotifies.Append(PoseB.AnimNotifies);
			}
		}
	}

	/**
	 * @brief 현재 시간 반환 (디버깅용)
	 *
	 * 참고: BlendSpace는 여러 샘플이 있으므로, 첫 번째 샘플 시간 반환
	 */
	virtual float GetCurrentTime() const override
	{
		if (Samples.Num() > 0)
			return Samples[0].InternalTime;
		return 0.0f;
	}

	/**
	 * @brief 노드 이름 반환 (디버깅용)
	 */
	virtual FString GetNodeName() const override
	{
		return "BlendSpace1D";
	}

private:
	// ========================================
	// 헬퍼 함수
	// ========================================

	/**
	 * @brief 현재 파라미터에 맞는 블렌딩 샘플 찾기
	 * @param Param 현재 파라미터 값 (예: Speed = 3.0)
	 * @param OutLowerIdx 하위 샘플 인덱스 (예: 0 = Idle)
	 * @param OutUpperIdx 상위 샘플 인덱스 (예: 1 = Walk)
	 * @param OutAlpha 블렌딩 비율 (0.0 = Lower 100%, 1.0 = Upper 100%)
	 *
	 * 예시:
	 * - Samples: [{0, Idle}, {5, Walk}, {10, Run}]
	 * - Param = 3.0
	 * - 결과: LowerIdx=0 (Idle), UpperIdx=1 (Walk), Alpha=0.6
	 * - 의미: Idle(40%) + Walk(60%)
	 *
	 * 알고리즘:
	 * - 이분 탐색으로 Param이 속한 구간 찾기
	 * - 선형 보간으로 Alpha 계산
	 * - 범위 밖이면 가장 가까운 샘플 사용
	 *
	 * 주의:
	 * - Samples가 SampleValue 오름차순 정렬되어 있다고 가정
	 * - Samples.Num() >= 1 보장됨 (호출자가 체크)
	 */
	void FindBlendSamples(float Param, int32& OutLowerIdx, int32& OutUpperIdx, float& OutAlpha)
	{
		// 예시:
		// Samples = [{0, Idle}, {5, Walk}, {10, Run}]
		// Param = 3.0 → Lower=0, Upper=1, Alpha=0.6 (3.0은 0과 5 사이, 60%만큼 진행)
		// Param = 7.5 → Lower=1, Upper=2, Alpha=0.5 (7.5는 5와 10 사이, 50%만큼 진행)

		// 케이스 1: 최소값 이하
		if (Param <= Samples[0].SampleValue)
		{
			OutLowerIdx = 0;
			OutUpperIdx = 0;
			OutAlpha = 0.0f;
			return;
		}

		// 케이스 2: 최대값 이상
		if (Param >= Samples.Last().SampleValue)
		{
			OutLowerIdx = Samples.Num() - 1;
			OutUpperIdx = OutLowerIdx;
			OutAlpha = 0.0f;
			return;
		}

		// 케이스 3: 중간 구간 (이분 탐색)
		// TArray는 std::vector 상속 → std::lower_bound 사용 가능
		auto Found = std::lower_bound(Samples.begin(), Samples.end(), Param,
			[](const FBlendSample1D& Sample, float Value)
			{
				return Sample.SampleValue < Value;
			});

		// lower_bound는 Param 이상의 첫 원소를 찾음
		// 예: Param=3.0 → Found는 Walk(5.0)를 가리킴
		OutUpperIdx = static_cast<int32>(Found - Samples.begin());
		OutLowerIdx = OutUpperIdx - 1;

		// 선형 보간 Alpha 계산
		const float Range = Samples[OutUpperIdx].SampleValue - Samples[OutLowerIdx].SampleValue;
		if (Range > 0.0f)
		{
			OutAlpha = (Param - Samples[OutLowerIdx].SampleValue) / Range;
		}
		else
		{
			// Range가 0이면 (같은 값) Lower만 사용
			OutAlpha = 0.0f;
		}
	}

	/**
	 * @brief 샘플의 포즈 추출 + Notify 수집
	 * @param Sample 샘플 (Animation, InternalTime 포함)
	 * @param OutPose 출력 포즈
	 */
	void ExtractPose(FBlendSample1D& Sample, FPoseContext& OutPose)
	{
		if (!Sample.Animation || !Skeleton)
		{
			OutPose.SetNumBones(Skeleton ? Skeleton->Bones.Num() : 0);
			return;
		}

		// 1. 포즈 추출 (SequencePlayer와 동일)
		FAnimExtractContext ExtractContext(Sample.InternalTime, bLooping);
		Sample.Animation->GetAnimationPose(OutPose, ExtractContext);

		// 2. Notify 수집 (PreviousTime ~ InternalTime 범위)
		TArray<FAnimNotifyEvent> Notifies;
		Sample.Animation->GetAnimNotifiesInRange(Sample.PreviousTime, Sample.InternalTime, Notifies);

		// 3. OutPose에 Notify 추가 (트리 누적)
		OutPose.AnimNotifies.Append(Notifies);
	}
};

#pragma once
#include "SkinnedMeshComponent.h"
#include "USkeletalMeshComponent.generated.h"

// 전방 선언
class UAnimInstance;
class UAnimSequence;
struct FAnimNotifyEvent;
enum class EAnimationMode : uint8;

UCLASS(DisplayName="스켈레탈 메시 컴포넌트", Description="스켈레탈 메시를 렌더링하는 컴포넌트입니다")
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    GENERATED_REFLECTION_BODY()
    
    USkeletalMeshComponent();
    ~USkeletalMeshComponent() override = default;

    void BeginPlay() override;
    void TickComponent(float DeltaTime) override;
    void SetSkeletalMesh(const FString& PathFileName) override;

// Animation Section
public:
    // 애니메이션 모드
    // UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animation", Tooltip="애니메이션 모드")
    EAnimationMode AnimationMode;

    // 애니메이션 인스턴스
    // UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animation", Tooltip="애니메이션 인스턴스")
    UAnimInstance* AnimInstance = nullptr;

    // 단일 노드 모드용 애니메이션
    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animation", Tooltip="재생할 애니메이션")
    UAnimSequence* AnimationData = nullptr;

    // 재생 제어
    UFUNCTION(DisplayName="PlayAnimation", LuaBind)
    void PlayAnimation(UAnimSequence* NewAnimToPlay, bool bLooping);

    UFUNCTION(DisplayName="StopAnimation", LuaBind)
    void StopAnimation();

    void SetAnimationMode(EAnimationMode InMode);
    void SetAnimation(UAnimSequence* InAnim);
    void SetAnimInstance(class UAnimInstance* InAnimInstance);
    void Play(bool bLooping);

    // AnimNotify 핸들링 (발제 문서 구조)
    void HandleAnimNotify(const FAnimNotifyEvent& Notify);

protected:
    // TickComponent에서 호출
    void TickAnimation(float DeltaTime);

// Editor Section
public:
    /**
     * @brief 특정 뼈의 부모 기준 로컬 트랜스폼을 설정
     * @param BoneIndex 수정할 뼈의 인덱스
     * @param NewLocalTransform 새로운 부모 기준 로컬 FTransform
     */
    void SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform);

    void SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform);
    
    /**
     * @brief 특정 뼈의 현재 로컬 트랜스폼을 반환
     */
    FTransform GetBoneLocalTransform(int32 BoneIndex) const;
    
    /**
     * @brief 기즈모를 렌더링하기 위해 특정 뼈의 월드 트랜스폼을 계산
     */
    FTransform GetBoneWorldTransform(int32 BoneIndex);

protected:
    /**
     * @brief CurrentLocalSpacePose의 변경사항을 ComponentSpace -> FinalMatrices 계산까지 모두 수행
     */
    void ForceRecomputePose();

    /**
     * @brief CurrentLocalSpacePose를 기반으로 CurrentComponentSpacePose 채우기
     */
    void UpdateComponentSpaceTransforms();

    /**
     * @brief CurrentComponentSpacePose를 기반으로 TempFinalSkinningMatrices 채우기
     */
    void UpdateFinalSkinningMatrices();

protected:
    /**
     * @brief 각 뼈의 부모 기준 로컬 트랜스폼
     */
    TArray<FTransform> CurrentLocalSpacePose;

    /**
     * @brief LocalSpacePose로부터 계산된 컴포넌트 기준 트랜스폼
     */
    TArray<FTransform> CurrentComponentSpacePose;

    /**
     * @brief 부모에게 보낼 최종 스키닝 행렬 (임시 계산용)
     */
    TArray<FMatrix> TempFinalSkinningMatrices;
    /**
     * @brief CPU 스키닝에 전달할 최종 노말 스키닝 행렬
     */
    TArray<FMatrix> TempFinalSkinningNormalMatrices;
};

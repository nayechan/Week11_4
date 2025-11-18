#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "AnimInstance.h"
#include "AnimSingleNodeInstance.h"
#include "AnimSequence.h"
#include "AnimationTypes.h"
#include "ResourceManager.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
    // 테스트용 기본 메시 설정
    // SetSkeletalMesh(GDataDir + "/Test.fbx");
}

void USkeletalMeshComponent::BeginPlay()
{
    Super::BeginPlay();

    // AnimInstance 자동 생성 (AnimationMode 기반)
    if (!AnimInstance)
    {
        UClass* Class = nullptr;

        switch (AnimationMode)
        {
        case EAnimationMode::AnimationClass:
            // AnimClass 사용 (C++ 또는 Lua AnimInstance)
            if (AnimClass)
            {
                Class = AnimClass.Get();
                UE_LOG("USkeletalMeshComponent::BeginPlay - AnimationMode::AnimationClass, using AnimClass: %s", Class ? Class->Name : "nullptr");
            }
            else
            {
                UE_LOG("USkeletalMeshComponent::BeginPlay - AnimationMode::AnimationClass, but AnimClass is not set!");
            }
            break;

        case EAnimationMode::AnimationSingleNode:
            // 단일 AnimSequence 직접 재생
            Class = UAnimSingleNodeInstance::StaticClass();
            UE_LOG("USkeletalMeshComponent::BeginPlay - AnimationMode::AnimationSingleNode, using AnimSingleNodeInstance");
            break;

        case EAnimationMode::None:
            // RefPose (T-Pose) - AnimInstance 생성 안 함
            UE_LOG("USkeletalMeshComponent::BeginPlay - AnimationMode::None, no AnimInstance created (RefPose)");
            break;

        default:
            UE_LOG("USkeletalMeshComponent::BeginPlay - Unknown AnimationMode!");
            break;
        }

        if (Class)
        {
            // UClass로부터 객체 생성
            UObject* NewObj = NewObject(Class);

            // UAnimInstance로 Cast (타입 안전성이 보장되지만 방어적 코딩)
            AnimInstance = Cast<UAnimInstance>(NewObj);

            if (AnimInstance)
            {
                AnimInstance->OwnerComponent = this;
                AnimInstance->InitializeAnimation();
                UE_LOG("USkeletalMeshComponent::BeginPlay - Created AnimInstance: %s", Class->Name);

                // AnimationSingleNode 모드에서 AnimSequence 설정
                if (AnimationMode == EAnimationMode::AnimationSingleNode)
                {
                    UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance);
                    if (SingleNode)
                    {
                        // AnimToPlay가 설정되어 있으면 재생 (None이면 재생하지 않음)
                        if (AnimToPlay)
                        {
                            UE_LOG("USkeletalMeshComponent::BeginPlay - Using AnimToPlay: %s (Length: %.2fs)",
                                AnimToPlay->GetFilePath().c_str(), AnimToPlay->SequenceLength);

                            // AnimSequence 설정 및 루프 재생
                            SingleNode->SetAnimationAsset(AnimToPlay);
                            SingleNode->Play(true); // 무조건 루프 재생
                        }
                    }
                }
            }
            else
            {
                // Cast 실패 - 이론상 발생하지 않아야 함 (TSubclassOf가 타입 보장)
                UE_LOG("USkeletalMeshComponent::BeginPlay - Failed to cast %s to UAnimInstance", Class->Name);
                ObjectFactory::DeleteObject(NewObj); // 생성된 객체 삭제
            }
        }
    }
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    // 애니메이션 틱
    TickAnimation(DeltaTime);
}

void USkeletalMeshComponent::SetSkeletalMesh(const FString& PathFileName)
{
    Super::SetSkeletalMesh(PathFileName);

    if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshData())
    {
        const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
        const int32 NumBones = Skeleton.Bones.Num();

        CurrentLocalSpacePose.SetNum(NumBones);
        CurrentComponentSpacePose.SetNum(NumBones);
        TempFinalSkinningMatrices.SetNum(NumBones);
        TempFinalSkinningNormalMatrices.SetNum(NumBones);

        for (int32 i = 0; i < NumBones; ++i)
        {
            const FBone& ThisBone = Skeleton.Bones[i];
            const int32 ParentIndex = ThisBone.ParentIndex;
            FMatrix LocalBindMatrix;

            if (ParentIndex == -1) // 루트 본
            {
                LocalBindMatrix = ThisBone.BindPose;
            }
            else // 자식 본
            {
                const FMatrix& ParentInverseBindPose = Skeleton.Bones[ParentIndex].InverseBindPose;
                LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
            }
            // 계산된 로컬 행렬을 로컬 트랜스폼으로 변환
            CurrentLocalSpacePose[i] = FTransform(LocalBindMatrix);
        }

        ForceRecomputePose();
    }
    else
    {
        // 메시 로드 실패 시 버퍼 비우기
        CurrentLocalSpacePose.Empty();
        CurrentComponentSpacePose.Empty();
        TempFinalSkinningMatrices.Empty();
        TempFinalSkinningNormalMatrices.Empty();
    }
}

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        CurrentLocalSpacePose[BoneIndex] = NewLocalTransform;
        ForceRecomputePose();
    }
}

void USkeletalMeshComponent::SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform)
{
    if (BoneIndex < 0 || BoneIndex >= CurrentLocalSpacePose.Num())
        return;

    const int32 ParentIndex = SkeletalMesh->GetSkeleton()->Bones[BoneIndex].ParentIndex;

    const FTransform& ParentWorldTransform = GetBoneWorldTransform(ParentIndex);
    FTransform DesiredLocal = ParentWorldTransform.GetRelativeTransform(NewWorldTransform);

    SetBoneLocalTransform(BoneIndex, DesiredLocal);
}


FTransform USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        return CurrentLocalSpacePose[BoneIndex];
    }
    return FTransform();
}

FTransform USkeletalMeshComponent::GetBoneWorldTransform(int32 BoneIndex)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex && BoneIndex >= 0)
    {
        // 뼈의 컴포넌트 공간 트랜스폼 * 컴포넌트의 월드 트랜스폼
        return GetWorldTransform().GetWorldTransform(CurrentComponentSpacePose[BoneIndex]);
    }
    return GetWorldTransform(); // 실패 시 컴포넌트 위치 반환
}

void USkeletalMeshComponent::ForceRecomputePose()
{
    if (!SkeletalMesh) { return; } 

    // LocalSpace -> ComponentSpace 계산
    UpdateComponentSpaceTransforms();
    // ComponentSpace -> Final Skinning Matrices 계산
    UpdateFinalSkinningMatrices();
    UpdateSkinningMatrices(TempFinalSkinningMatrices, TempFinalSkinningNormalMatrices);
}

void USkeletalMeshComponent::UpdateComponentSpaceTransforms()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FTransform& LocalTransform = CurrentLocalSpacePose[BoneIndex];
        const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;

        if (ParentIndex == -1) // 루트 본
        {
            CurrentComponentSpacePose[BoneIndex] = LocalTransform;
        }
        else // 자식 본
        {
            const FTransform& ParentComponentTransform = CurrentComponentSpacePose[ParentIndex];
            CurrentComponentSpacePose[BoneIndex] = ParentComponentTransform.GetWorldTransform(LocalTransform);
        }
    }
}

void USkeletalMeshComponent::UpdateFinalSkinningMatrices()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FMatrix& InvBindPose = Skeleton.Bones[BoneIndex].InverseBindPose;
        const FMatrix ComponentPoseMatrix = CurrentComponentSpacePose[BoneIndex].ToMatrix();

        TempFinalSkinningMatrices[BoneIndex] = InvBindPose * ComponentPoseMatrix;
        TempFinalSkinningNormalMatrices[BoneIndex] = TempFinalSkinningMatrices[BoneIndex].Inverse().Transpose();
    }
}

// Animation Section Implementation

void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* InAnimInstance)
{
    if (InAnimInstance)
    {
        AnimInstance = InAnimInstance;
        AnimInstance->OwnerComponent = this;

        UE_LOG("USkeletalMeshComponent::SetAnimInstance - %s", InAnimInstance->GetClass()->Name);
    }
}

void USkeletalMeshComponent::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Owner->HandleAnimNotify(Notify);
    }
}

void USkeletalMeshComponent::TickAnimation(float DeltaTime)
{
    if (!AnimInstance)
        return;

    // 애니메이션 파이프라인 실행 (Native + Lua)
    AnimInstance->UpdateAnimation(DeltaTime);

    // 포즈 추출 및 적용
    FPoseContext Pose;
    AnimInstance->GetAnimationPose(Pose);

    // Pose를 CurrentLocalSpacePose에 적용
    const int32 NumBones = FMath::Min(Pose.GetNumBones(), CurrentLocalSpacePose.Num());
    for (int32 i = 0; i < NumBones; ++i)
    {
        CurrentLocalSpacePose[i] = Pose.BoneTransforms[i];
    }

    // 스키닝 업데이트
    ForceRecomputePose();
}

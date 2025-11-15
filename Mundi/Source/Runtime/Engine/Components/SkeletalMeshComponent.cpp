#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "AnimInstance.h"
#include "AnimSingleNodeInstance.h"
#include "AnimSequence.h"
#include "AnimationTypes.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
    // 테스트용 기본 메시 설정
    SetSkeletalMesh(GDataDir + "/Test.fbx");
}

void USkeletalMeshComponent::BeginPlay()
{
    Super::BeginPlay();

    // PIE 모드 시작 시 AnimationData가 설정되어 있으면 자동으로 반복 재생
    if (AnimationData)
    {
        PlayAnimation(AnimationData, true);
        UE_LOG("USkeletalMeshComponent::BeginPlay - Auto-playing animation: %s", AnimationData->GetName().c_str());
    }
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    // 애니메이션 틱
    TickAnimation(DeltaTime);

    //// FOR TEST ////
    if (!SkeletalMesh) { return; } // 부모의 SkeletalMesh 확인

    // 1. 테스트할 뼈 인덱스 (모델에 따라 1, 5, 10 등 바꿔보세요)
    constexpr int32 TEST_BONE_INDEX = 2;
    
    // 3. 테스트 시간 누적
    if (!bIsInitialized)
    {
        TestBoneBasePose = CurrentLocalSpacePose[TEST_BONE_INDEX];
        bIsInitialized = true;
    }
    TestTime += DeltaTime;

    // 4. sin 함수를 이용해 -1 ~ +1 사이를 왕복하는 회전값 생성
    // (예: Y축(Yaw)을 기준으로 1초에 1라디안(약 57도)씩 왕복)
    float Angle = sinf(TestTime * 2.f);
    FQuat TestRotation = FQuat::FromAxisAngle(FVector(1.f, 0.f, 0.f), Angle);
    TestRotation.Normalize();

    // 5. [중요] 원본 T-Pose에 테스트 회전을 누적
    FTransform NewLocalPose = TestBoneBasePose;
    NewLocalPose.Rotation = TestRotation * TestBoneBasePose.Rotation;
    
    // 6. [핵심] 기즈모가 하듯이, 뼈의 로컬 트랜스폼을 강제 설정
    // (이 함수는 내부적으로 ForceRecomputePose()를 호출함)
    SetBoneLocalTransform(TEST_BONE_INDEX, NewLocalPose);
    //// FOR TEST ////
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

void USkeletalMeshComponent::PlayAnimation(UAnimSequence* NewAnimToPlay, bool bLooping)
{
    if (!NewAnimToPlay)
    {
        UE_LOG("USkeletalMeshComponent::PlayAnimation - Null animation");
        return;
    }

    SetAnimationMode(EAnimationMode::AnimationSingleNode);
    SetAnimation(NewAnimToPlay);
    Play(bLooping);

    UE_LOG("USkeletalMeshComponent::PlayAnimation - %s", NewAnimToPlay->GetName().c_str());
}

void USkeletalMeshComponent::StopAnimation()
{
    if (AnimInstance)
    {
        UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance);
        if (SingleNode)
        {
            SingleNode->Stop();
        }
    }

    UE_LOG("USkeletalMeshComponent::StopAnimation");
}

void USkeletalMeshComponent::SetAnimationMode(EAnimationMode InMode)
{
    AnimationMode = InMode;

    // 모드에 맞는 AnimInstance 생성
    if (AnimationMode == EAnimationMode::AnimationSingleNode)
    {
        if (!AnimInstance || !Cast<UAnimSingleNodeInstance>(AnimInstance))
        {
            // 새 SingleNode 인스턴스 생성
            AnimInstance = NewObject<UAnimSingleNodeInstance>();
            AnimInstance->OwnerComponent = this;
        }
    }
}

void USkeletalMeshComponent::SetAnimation(UAnimSequence* InAnim)
{
    AnimationData = InAnim;

    UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance);
    if (SingleNode)
    {
        SingleNode->SetAnimationAsset(InAnim);
    }
}

void USkeletalMeshComponent::Play(bool bLooping)
{
    UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance);
    if (SingleNode)
    {
        SingleNode->Play(bLooping);
    }
}

void USkeletalMeshComponent::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        // Actor의 HandleAnimNotify 호출 (발제 문서 구조)
        // TODO: AActor에 HandleAnimNotify 가상 함수 추가 필요
        UE_LOG("AnimNotify: %s at time %.2f", Notify.NotifyName.ToString().c_str(), Notify.TriggerTime);
    }
}

void USkeletalMeshComponent::TickAnimation(float DeltaTime)
{
    if (!AnimInstance)
        return;

    // 애니메이션 업데이트
    AnimInstance->NativeUpdateAnimation(DeltaTime);

    // 포즈 추출 및 적용
    if (AnimationData)
    {
        FPoseContext Pose;
        FAnimExtractContext Context(AnimInstance->GetCurrentTime(), false);

        AnimationData->GetAnimationPose(Pose, Context);

        // Pose를 CurrentLocalSpacePose에 적용
        const int32 NumBones = FMath::Min(Pose.GetNumBones(), CurrentLocalSpacePose.Num());
        for (int32 i = 0; i < NumBones; ++i)
        {
            CurrentLocalSpacePose[i] = Pose.BoneTransforms[i];
        }

        // 스키닝 업데이트
        ForceRecomputePose();
    }
}

#include "pch.h"
#include "SkinnedMeshActor.h"
#include "BoneAnchorComponent.h"

ASkinnedMeshActor::ASkinnedMeshActor()
{
    ObjectName = "Skinned Mesh Actor";

    // 스킨드 메시 렌더용 컴포넌트 생성 및 루트로 설정
    // - 프리뷰 장면에서 메시를 표시하는 실제 렌더링 컴포넌트
    SkinnedMeshComponent = CreateDefaultSubobject<USkinnedMeshComponent>("SkinnedMeshComponent");
    RootComponent = SkinnedMeshComponent;

    // 뼈 라인 오버레이용 컴포넌트 생성 후 루트에 부착
    // - 이 컴포넌트는 "라인 데이터"(시작/끝점, 색상)를 모아 렌더러에 배치합니다.
    // - 액터의 로컬 공간으로 선을 추가하면, 액터의 트랜스폼에 따라 선도 함께 변환됩니다.
    BoneLineComponent = CreateDefaultSubobject<ULineComponent>("BoneLines");
    if (BoneLineComponent && RootComponent)
    {
        // 부모 트랜스폼을 유지하면서(=로컬 좌표 유지) 루트에 붙입니다.
        BoneLineComponent->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
        // Render skeleton overlay always on top of geometry
        BoneLineComponent->SetAlwaysOnTop(true);
    }

    // Hidden anchor for gizmo placement on selected bone
    BoneAnchor = CreateDefaultSubobject<UBoneAnchorComponent>("BoneAnchor");
    if (BoneAnchor && RootComponent)
    {
        BoneAnchor->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
        BoneAnchor->SetVisibility(false); // not rendered; used only for selection/gizmo
    }
}

ASkinnedMeshActor::~ASkinnedMeshActor() = default;

void ASkinnedMeshActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

FAABB ASkinnedMeshActor::GetBounds() const
{
    // Be robust to component replacement: query current root
    if (auto* Current = Cast<USkinnedMeshComponent>(RootComponent))
    {
        return Current->GetWorldAABB();
    }
    return FAABB();
}

void ASkinnedMeshActor::SetSkinnedMeshComponent(USkinnedMeshComponent* InComp)
{
    SkinnedMeshComponent = InComp;
}

void ASkinnedMeshActor::SetSkeletalMesh(const FString& PathFileName)
{
    if (SkinnedMeshComponent)
    {
        SkinnedMeshComponent->SetSkeletalMesh(PathFileName);
    }
}

void ASkinnedMeshActor::RebuildBoneLines(int32 SelectedBoneIndex)
{
    if (!BoneLineComponent)
    {
        return;
    }

    BoneLineComponent->ClearLines();

    if (!SkinnedMeshComponent)
    {
        return;
    }

    USkeletalMesh* SkeletalMesh = SkinnedMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
    {
        return;
    }

    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
    {
        return;
    }

    const auto& Bones = Data->Skeleton.Bones;
    const int32 BoneCount = static_cast<int32>(Bones.size());
    if (BoneCount <= 0)
    {
        return;
    }

    TArray<FVector> JointPos; JointPos.resize(BoneCount);
    const FVector4 Origin(0, 0, 0, 1);
    for (int32 i = 0; i < BoneCount; ++i)
    {
        const FMatrix& Bind = Bones[i].BindPose;
        const FVector4 P = Origin * Bind; // row-vector convention
        JointPos[i] = FVector(P.X, P.Y, P.Z);
    }

    // 1) Bone connection lines (parent-child)
    for (int32 i = 0; i < BoneCount; ++i)
    {
        int32 parent = Bones[i].ParentIndex;
        if (parent >= 0 && parent < BoneCount)
        {
            FVector4 color = (SelectedBoneIndex == i || SelectedBoneIndex == parent)
                ? FVector4(1, 0, 0, 1)
                : FVector4(0, 1, 0, 1);
            BoneLineComponent->AddLine(JointPos[parent], JointPos[i], color);
        }
    }

    // 2) Joint spheres (three great-circle rings per joint)
    const float SmallRadius = 0.1f;
    const float SelectedRadius = 0.15f;
    const int NumSegments = 16;
    for (int32 i = 0; i < BoneCount; ++i)
    {
        const FVector Center = JointPos[i];
        const bool bSelected = (SelectedBoneIndex == i);
        const FVector4 RingColor = bSelected
            ? FVector4(1.0f, 0.85f, 0.2f, 1.0f)
            : FVector4(0.8f, 0.8f, 0.8f, 1.0f);

        const float Radius = bSelected ? SelectedRadius : SmallRadius;
        for (int k = 0; k < NumSegments; ++k)
        {
            const float a0 = (static_cast<float>(k) / NumSegments) * TWO_PI;
            const float a1 = (static_cast<float>((k + 1) % NumSegments) / NumSegments) * TWO_PI;
            // XY ring
            BoneLineComponent->AddLine(
                Center + FVector(Radius * std::cos(a0), Radius * std::sin(a0), 0.0f),
                Center + FVector(Radius * std::cos(a1), Radius * std::sin(a1), 0.0f),
                RingColor);
            // XZ ring
            BoneLineComponent->AddLine(
                Center + FVector(Radius * std::cos(a0), 0.0f, Radius * std::sin(a0)),
                Center + FVector(Radius * std::cos(a1), 0.0f, Radius * std::sin(a1)),
                RingColor);
            // YZ ring
            BoneLineComponent->AddLine(
                Center + FVector(0.0f, Radius * std::cos(a0), Radius * std::sin(a0)),
                Center + FVector(0.0f, Radius * std::cos(a1), Radius * std::sin(a1)),
                RingColor);
        }

        // Small axis cross for selected bone for extra visibility
        if (bSelected)
        {
            const float AxisLen = SelectedRadius * 0.6f;
            BoneLineComponent->AddLine(Center + FVector(-AxisLen, 0, 0), Center + FVector(AxisLen, 0, 0), RingColor);
            BoneLineComponent->AddLine(Center + FVector(0, -AxisLen, 0), Center + FVector(0, AxisLen, 0), RingColor);
            BoneLineComponent->AddLine(Center + FVector(0, 0, -AxisLen), Center + FVector(0, 0, AxisLen), RingColor);
        }
    }
}

void ASkinnedMeshActor::MoveGizmoToBone(int32 BoneIndex)
{
    if (!SkinnedMeshComponent || !BoneAnchor)
        return;

    USkeletalMesh* SkeletalMesh = SkinnedMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
        return;

    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
        return;

    const auto& Bones = Data->Skeleton.Bones;
    if (BoneIndex < 0 || BoneIndex >= (int32)Bones.size())
        return;

    // Compute approximate world-space joint position from bind pose
    const FVector4 Origin(0, 0, 0, 1);
    const FMatrix& Bind = Bones[BoneIndex].BindPose;
    const FVector4 P = Origin * Bind; // row-vector convention used throughout
    const FVector Local = FVector(P.X, P.Y, P.Z);
    const FVector4 W = FVector4(Local.X, Local.Y, Local.Z, 1.0f) * GetWorldMatrix();
    const FVector WorldTranslation = FVector(W.X, W.Y, W.Z);

    BoneAnchor->SetWorldLocation(WorldTranslation);
}

void ASkinnedMeshActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
    for (UActorComponent* Component : OwnedComponents)
    {
        if (auto* Comp = Cast<USkinnedMeshComponent>(Component))
        {
            SkinnedMeshComponent = Comp;
            break;
        }
    }
    for (UActorComponent* Component : OwnedComponents)
    {
        if (auto* Comp = Cast<ULineComponent>(Component))
        {
            BoneLineComponent = Comp;
            break;
        }
    }
}

void ASkinnedMeshActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        SkinnedMeshComponent = Cast<USkinnedMeshComponent>(RootComponent);
    }
}


#pragma once
#include "ResourceBase.h"
#include "USkeletalMesh.generated.h"

UCLASS(DisplayName="스켈레탈 메시", Description="스켈레탈 메시 애셋")
class USkeletalMesh : public UResourceBase
{
public:
    GENERATED_REFLECTION_BODY()

    USkeletalMesh();
    virtual ~USkeletalMesh() override;

    // 스켈레톤 고유 ID (호환성 판별용)
    UPROPERTY(EditAnywhere, Category="Skeleton", Tooltip="스켈레톤 고유 ID - 같은 ID를 가진 애니메이션과 호환됩니다")
    FString SkeletonID;

    void Load(const FString& InFilePath, ID3D11Device* InDevice);

    const FSkeletalMesh* GetSkeletalMeshData() const { return Data; }
    const FString& GetPathFileName() const { static FString EmptyString; if (Data) return Data->PathFileName; return EmptyString; }
    const FSkeleton* GetSkeleton() const { return Data ? Data->Skeleton : nullptr; }
    void SetSkeleton(FSkeleton* NewSkeleton) { if (Data) Data->Skeleton = NewSkeleton; }
    uint32 GetBoneCount() const { return (Data && Data->Skeleton) ? Data->Skeleton->Bones.Num() : 0; }

    ID3D11Buffer* GetVertexBuffer() const { return VertexBuffer; } // W10 CPU Skinning이라 Component가 VB 소유
    ID3D11Buffer* GetIndexBuffer() const { return IndexBuffer; }

    uint32 GetVertexCount() const { return VertexCount; }
    uint32 GetIndexCount() const { return IndexCount; }

    uint32 GetVertexStride() const { return VertexStride; }

    const TArray<FGroupInfo>& GetMeshGroupInfo() const { static TArray<FGroupInfo> EmptyGroup; return Data ? Data->GroupInfos : EmptyGroup; }
    bool HasMaterial() const { return Data ? Data->bHasMaterial : false; }

    uint64 GetMeshGroupCount() const { return Data ? Data->GroupInfos.size() : 0; }

    void CreateVertexBuffer(ID3D11Buffer** InVertexBuffer);
    void UpdateVertexBuffer(const TArray<FSkinnedVertex>& SkinnedVertices, ID3D11Buffer* InVertexBuffer);

private:
    void CreateIndexBuffer(FSkeletalMesh* InSkeletalMesh, ID3D11Device* InDevice);
    void ReleaseResources();

private:
    // GPU 리소스
    ID3D11Buffer* VertexBuffer = nullptr; // W10 CPU Skinning이라 Component가 VB 소유
    ID3D11Buffer* IndexBuffer = nullptr;
    uint32 VertexCount = 0;     // 정점 개수
    uint32 IndexCount = 0;     // 버텍스 점의 개수
    uint32 VertexStride = 0;

    // CPU 리소스
    FSkeletalMesh* Data = nullptr;
};
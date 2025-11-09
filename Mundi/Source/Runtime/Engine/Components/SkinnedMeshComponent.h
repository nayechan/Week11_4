#pragma once
#include "MeshComponent.h"
#include "SkeletalMesh.h"
#include "USkinnedMeshComponent.generated.h"

UCLASS(DisplayName="스킨드 메시 컴포넌트", Description="스켈레탈 메시를 렌더링하는 컴포넌트입니다")
class USkinnedMeshComponent : public UMeshComponent
{
public:
    GENERATED_REFLECTION_BODY()

    USkinnedMeshComponent();
    ~USkinnedMeshComponent() override = default;

    void BeginPlay() override;

    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
    
// Mesh Component Section
public:
    virtual void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;
    
    // UStaticMeshComponent에서 복사 (씬 관리에 필수)
    virtual FAABB GetWorldAABB() const override;
    virtual void OnTransformUpdated() override;

// Skeletal Section
public:
    /**
     * @brief 렌더링할 스켈레탈 메시 에셋 설정 (UStaticMeshComponent::SetStaticMesh와 동일한 역할)
     * @param PathFileName 새 스켈레탈 메시 에셋 경로
     */
    void SetSkeletalMesh(const FString& PathFileName);

    /**
     * @brief 이 컴포넌트의 USkeletalMesh 에셋을 반환
     */
    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }

protected:
    USkeletalMesh* SkeletalMesh;

    /**
    * CPU 스키닝 최종 결과물 (TODO: 스키닝 계산 결과 버텍스들)
    */
    // TArray<FNormalVertex> SkinnedVertices;
};

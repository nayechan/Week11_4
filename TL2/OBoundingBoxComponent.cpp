#include"pch.h"
#include "OBoundingBoxComponent.h"
#include "Vector.h"

std::vector<FVector> MakeVerticesFromFBox(const FBox& Box)
{
    const FVector& Min = Box.Min;
    const FVector& Max = Box.Max;

    return {
        {Min.X, Min.Y, Min.Z},
        {Max.X, Min.Y, Min.Z},
        {Min.X, Max.Y, Min.Z},
        {Max.X, Max.Y, Min.Z},
        {Min.X, Min.Y, Max.Z},
        {Max.X, Min.Y, Max.Z},
        {Min.X, Max.Y, Max.Z},
        {Max.X, Max.Y, Max.Z}
    };
}
UOBoundingBoxComponent::UOBoundingBoxComponent()
    : LocalMin(FVector{}), LocalMax(FVector{})
{
    SetMaterial("CollisionDebug.hlsl", EVertexLayoutType::PositionCollisionDebug);
}

void UOBoundingBoxComponent::SetFromVertices(const std::vector<FVector>& Verts)
{
    if (Verts.empty()) return;

    LocalMin = LocalMax = Verts[0];
    for (auto& v : Verts)
    {
        LocalMin = LocalMin.ComponentMin(v);
        LocalMax = LocalMax.ComponentMax(v);
    }
    FString MeshName = FString("OBB_") + AttachParent->GetName();
    UResourceManager::GetInstance().CreateBoxWireframeMesh(LocalMin, LocalMax, MeshName);
    //SetMeshResource(MeshName);
}

FBox UOBoundingBoxComponent::GetWorldBox() const
{
    auto corners = GetLocalCorners();

    FVector MinW = GetWorldTransform().TransformPosition(corners[0]);
    FVector MaxW = MinW;

    for (auto& c : corners)
    {
        FVector wc = GetWorldTransform().TransformPosition(c);
        MinW = MinW.ComponentMin(wc);
        MaxW = MaxW.ComponentMax(wc);
    }//MinW, MaxW
    return FBox();
}

FVector UOBoundingBoxComponent::GetExtent() const
{
    return (LocalMax - LocalMin) * 0.5f;
}

std::vector<FVector> UOBoundingBoxComponent::GetLocalCorners() const
{
    return {
        {LocalMin.X, LocalMin.Y, LocalMin.Z},
        {LocalMax.X, LocalMin.Y, LocalMin.Z},
        {LocalMin.X, LocalMax.Y, LocalMin.Z},
        {LocalMax.X, LocalMax.Y, LocalMin.Z},
        {LocalMin.X, LocalMin.Y, LocalMax.Z},
        {LocalMax.X, LocalMin.Y, LocalMax.Z},
        {LocalMin.X, LocalMax.Y, LocalMax.Z},
        {LocalMax.X, LocalMax.Y, LocalMax.Z}
    };
}


FBox UOBoundingBoxComponent::GetWorldOBBFromAttachParent() const
{
	
    if (!AttachParent) return FBox();

    // AttachParent의 로컬 코너들
    auto corners = GetLocalCorners();

    // 월드 변환된 첫 번째 점으로 초기화
    FVector MinW = AttachParent->GetWorldTransform().TransformPosition(corners[0]);
    FVector MaxW = MinW;

    for (auto& c : corners)
    {
        FVector wc = AttachParent->GetWorldTransform().TransformPosition(c);
        MinW = MinW.ComponentMin(wc);
        MaxW = MaxW.ComponentMax(wc);
    }
    //BBWorldMatrix
    return FBox(MinW, MaxW);
}

void UOBoundingBoxComponent::Render(URenderer* Renderer, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix)
{
    /*if(OOBB)
    SetupAttachment(NULL);*///OOBB시  조건문 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    Renderer->RSSetState(EViewModeIndex::VMI_Wireframe);
    Renderer->UpdateConstantBuffer(GetWorldMatrix(), ViewMatrix, ProjectionMatrix);
    Renderer->PrepareShader(GetMaterial()->GetShader());
    //Renderer->DrawIndexedPrimitiveComponent(GetMeshResource(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
}

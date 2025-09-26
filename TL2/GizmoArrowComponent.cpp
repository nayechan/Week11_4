#include "pch.h"
#include "GizmoArrowComponent.h"

UGizmoArrowComponent::UGizmoArrowComponent()
{
    SetStaticMesh("Data/Arrow.obj");
    SetMaterial("Primitive.hlsl", EVertexLayoutType::PositionColor);
}

UGizmoArrowComponent::~UGizmoArrowComponent()
{

}

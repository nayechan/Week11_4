#include "pch.h"
#include "GizmoScaleComponent.h"

UGizmoScaleComponent::UGizmoScaleComponent()
{
    SetStaticMesh("Data/ScaleHandle.obj");
    SetMaterial("Primitive.hlsl", EVertexLayoutType::PositionColor);
}

UGizmoScaleComponent::~UGizmoScaleComponent()
{
}

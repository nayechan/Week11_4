#include "pch.h"
#include "GizmoScaleComponent.h"

IMPLEMENT_CLASS(UGizmoScaleComponent)

UGizmoScaleComponent::UGizmoScaleComponent()
{
    SetStaticMesh("Data/Gizmo/ScaleHandle.obj");
    SetMaterialByName(0, "Shaders/StaticMesh/StaticMeshShader.hlsl");
}

UGizmoScaleComponent::~UGizmoScaleComponent()
{
}

void UGizmoScaleComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

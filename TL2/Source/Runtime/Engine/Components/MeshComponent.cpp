#include "pch.h"
#include "MeshComponent.h"
#include "StaticMesh.h"
#include "ObjManager.h"

IMPLEMENT_CLASS(UMeshComponent)

UMeshComponent::UMeshComponent()
{
}

UMeshComponent::~UMeshComponent()
{
    Material = nullptr;
}

void UMeshComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

//void UMeshComponent::SetMeshResource(const FString& FilePath)
//{
//	MeshResource = FObjManager::LoadObjStaticMesh(FilePath);
//}
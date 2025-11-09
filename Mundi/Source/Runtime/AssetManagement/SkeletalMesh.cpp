#include "pch.h"
#include "SkeletalMesh.h"

#include "FBXLoader.h"

IMPLEMENT_CLASS(USkeletalMesh)

USkeletalMesh::USkeletalMesh()
{
}

USkeletalMesh::~USkeletalMesh()
{
    ReleaseResources();
}

void USkeletalMesh::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
    FSkeletalMeshData SkeletalMeshData = UFbxLoader::GetInstance().LoadFbxMesh(InFilePath);
    if (SkeletalMeshData.Vertices.empty() || SkeletalMeshData.Indices.empty()) { return; }
    
    CreateVertexBuffer(&SkeletalMeshData, InDevice);
    CreateIndexBuffer(&SkeletalMeshData, InDevice);
    VertexCount = static_cast<uint32>(SkeletalMeshData.Vertices.size());
    IndexCount = static_cast<uint32>(SkeletalMeshData.Indices.size());
    VertexStride = sizeof(FVertexDynamic);
}

void USkeletalMesh::ReleaseResources()
{
}

void USkeletalMesh::CreateVertexBuffer(FSkeletalMeshData* InSkeletalMesh, ID3D11Device* InDevice)
{
    HRESULT hr = D3D11RHI::CreateVertexBuffer<FVertexDynamic>(InDevice, InSkeletalMesh->Vertices, &VertexBuffer);
    assert(SUCCEEDED(hr));
}

void USkeletalMesh::CreateIndexBuffer(FSkeletalMeshData* InSkeletalMesh, ID3D11Device* InDevice)
{
    HRESULT hr = D3D11RHI::CreateIndexBuffer(InDevice, InSkeletalMesh, &IndexBuffer);
    assert(SUCCEEDED(hr));
}

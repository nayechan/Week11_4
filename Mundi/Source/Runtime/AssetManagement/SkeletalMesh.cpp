#include "pch.h"
#include "SkeletalMesh.h"


#include "FbxLoader.h"
#include "FbxManager.h"
#include "WindowsBinReader.h"
#include "WindowsBinWriter.h"
#include "PathUtils.h"
#include "JsonSerializer.h"
#include "GlobalConsole.h"
#include <filesystem>

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
    if (Data)
    {
        ReleaseResources();
    }

    // FBXLoader가 캐싱을 내부적으로 처리합니다
    Data = UFbxLoader::GetInstance().LoadFbxMeshAsset(InFilePath);

    if (!Data || Data->Vertices.empty() || Data->Indices.empty())
    {
        UE_LOG("ERROR: Failed to load FBX mesh from '%s'", InFilePath.c_str());
        delete Data;
        Data = nullptr;
        return;
    }

    // GPU 버퍼 생성
    CreateVertexBuffer(&VertexBuffer);
    CreateIndexBuffer(Data, InDevice);
    VertexCount = static_cast<uint32>(Data->Vertices.size());
    IndexCount = static_cast<uint32>(Data->Indices.size());
    VertexStride = sizeof(FSkinnedVertex);
}

void USkeletalMesh::ReleaseResources()
{

    if (VertexBuffer)
    {
        VertexBuffer->Release();
        VertexBuffer = nullptr;
    }
    if (IndexBuffer)
    {
        IndexBuffer->Release();
        IndexBuffer = nullptr;
    }

    if (Data)
    {
        delete Data;
        Data = nullptr;
    }
}

void USkeletalMesh::CreateVertexBuffer(ID3D11Buffer** InVertexBuffer)
{
    if (!Data) { return; }
    ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();
    HRESULT hr = D3D11RHI::CreateVertexBuffer<FSkinnedVertex>(Device, Data->Vertices, InVertexBuffer);
    assert(SUCCEEDED(hr));
}

void USkeletalMesh::UpdateVertexBuffer(const TArray<FSkinnedVertex>& SkinnedVertices, ID3D11Buffer* InVertexBuffer)
{
    if (!InVertexBuffer) { return; }

    GEngine.GetRHIDevice()->VertexBufferUpdate(InVertexBuffer, SkinnedVertices);
}

void USkeletalMesh::CreateIndexBuffer(FSkeletalMesh* InSkeletalMesh, ID3D11Device* InDevice)
{
    HRESULT hr = D3D11RHI::CreateIndexBuffer(InDevice, InSkeletalMesh, &IndexBuffer);
    assert(SUCCEEDED(hr));
}

bool USkeletalMesh::SaveOverrideData(const FString& FilePath)
{
    // 기존 .skeleton 파일 로드 (있다면)
    JSON RootJson = JSON::Make(JSON::Class::Object);
    FWideString WidePath = UTF8ToWide(FilePath);

    // 기존 파일이 있으면 로드하여 다른 섹션 보존
    if (std::filesystem::exists(UTF8ToWide(FilePath)))
    {
        FJsonSerializer::LoadJsonFromFile(RootJson, WidePath);
    }

    // "Mesh" 섹션 생성/업데이트
    JSON MeshSection = JSON::Make(JSON::Class::Object);
    const FSkeleton* CurrentSkeleton = GetSkeleton();
    if (CurrentSkeleton && !CurrentSkeleton->Name.empty())
    {
        MeshSection["SkeletonName"] = CurrentSkeleton->Name;
    }
    RootJson["Mesh"] = MeshSection;

    // JSON 파일로 저장
    return FJsonSerializer::SaveJsonToFile(RootJson, WidePath);
}

bool USkeletalMesh::LoadOverrideData(const FString& FilePath)
{
    // JSON 파일 로드
    JSON RootJson;
    FWideString WidePath = UTF8ToWide(FilePath);
    if (!FJsonSerializer::LoadJsonFromFile(RootJson, WidePath))
    {
        return false;
    }

    // "Mesh" 섹션에서 Skeleton 오버라이드 복원
    JSON MeshSection;
    if (FJsonSerializer::ReadObject(RootJson, "Mesh", MeshSection, nullptr, false))
    {
        FString SkeletonName;
        if (FJsonSerializer::ReadString(MeshSection, "SkeletonName", SkeletonName, "", false) && !SkeletonName.empty())
        {
            FSkeleton* LoadedSkeleton = FFbxManager::GetSkeletonByName(SkeletonName);
            if (LoadedSkeleton)
            {
                SetSkeleton(LoadedSkeleton);
                SkeletonID = LoadedSkeleton->Name;
                UE_LOG("Loaded Mesh skeleton override: %s", SkeletonName.c_str());
            }
            else
            {
                UE_LOG("Warning: Could not find skeleton '%s' in FFbxManager", SkeletonName.c_str());
            }
        }
    }

    return true;
}

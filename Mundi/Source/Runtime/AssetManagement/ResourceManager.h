#pragma once
#include "ObjectFactory.h"
#include "Object.h"
#include "Shader.h"
#include "StaticMesh.h"
#include "Material.h"
#include "Texture.h"
#include "TextureConverter.h"
#include "DynamicMesh.h"
#include "Quad.h"
#include "LineDynamicMesh.h"

#pragma once
#include "ObjectFactory.h"
#include "Object.h"
// ... 기타 include ...

// --- 전방 선언 ---
class UStaticMesh;
class FMeshBVH;
class UResourceBase;
class UMaterial;

//================================================================================================
// UResourceManager
//================================================================================================

class UResourceManager : public UObject
{
public:
	DECLARE_CLASS(UResourceManager, UObject)

	// --- 싱글톤 및 초기화 ---
	static UResourceManager& GetInstance();
	void Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext);
	void Clear(); // 전체 해제

	// --- 생성자 ---
	UResourceManager() = default;

	// --- 리소스 로드 및 접근 ---
	template<typename T, typename... Args>
	T* Load(const FString& InFilePath, Args&&... InArgs);

	// 매크로가 있는 UShader를 위한 특수화 Load 함수
	template<>
	UShader* Load<UShader>(const FString& InFilePath, TArray<FShaderMacro>& InMacros);

	// UTexture용 sRGB 파라미터 지원 로드 함수
	UTexture* LoadTexture(const FString& InFilePath, bool bSRGB);

	template<typename T>
	bool Add(const FString& InFilePath, UObject* InObject);

	template<typename T>
	T* Get(const FString& InFilePath);

	template<typename T>
	TArray<T*> GetAll();

	template<typename T>
	TArray<FString> GetAllFilePaths();

	template<typename T>
	ResourceType GetResourceType();

	// --- 헬퍼 및 유틸리티 ---
	ID3D11Device* GetDevice() { return Device; }
	ID3D11DeviceContext* GetDeviceContext() { return Context; }
	TArray<D3D11_INPUT_ELEMENT_DESC>& GetProperInputLayout(const FString& InShaderName);
	FString& GetProperShader(const FString& InTextureName);

	// --- Shader Hot Reload ---
	void CheckAndReloadShaders(float DeltaTime);

	// --- 리소스 생성 및 관리 ---
	FTextureData* CreateOrGetTextureData(const FWideString& FilePath);
	void UpdateDynamicVertexBuffer(const FString& name, TArray<FBillboardVertexInfo_GPU>& vertices);
	UMaterial* GetDefaultMaterial();

	// --- 디버그 및 기본 메시 생성 ---
	void CreateDefaultShader();
	void CreateDefaultMaterial();
	void CreateAxisMesh(float Length, const FString& FilePath);
	void CreateGridMesh(int N, const FString& FilePath);
	void CreateBoxWireframeMesh(const FVector& Min, const FVector& Max, const FString& FilePath);
	void CreateBillboardMesh();
	void CreateTextBillboardMesh();
	void CreateTextBillboardTexture();

	// --- 캐시 관리 ---
	FMeshBVH* GetMeshBVH(const FString& ObjPath);
	FMeshBVH* GetOrBuildMeshBVH(const FString& ObjPath, const struct FStaticMesh* StaticMeshAsset);
	void SetStaticMeshs();
	const TArray<UStaticMesh*>& GetStaticMeshs() { return StaticMeshs; }

	// --- Deprecated (향후 제거될 함수들) ---
	TArray<UStaticMesh*> GetAllStaticMeshes() { return GetAll<UStaticMesh>(); }
	TArray<FString> GetAllStaticMeshFilePaths() { return GetAllFilePaths<UStaticMesh>(); }
	void InitShaderILMap();
	void InitTexToShaderMap();

protected:
	// --- 소멸자 및 복사/대입 방지 ---
	~UResourceManager() override;
	UResourceManager(const UResourceManager&) = delete;
	UResourceManager& operator=(const UResourceManager&) = delete;

	// --- 보호된 멤버 변수 ---
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* Context = nullptr;

	//Resource Type의 개수만큼 Array 생성 및 저장
	TArray<TMap<FString, UResourceBase*>> Resources;

	TMap<FString, TArray<D3D11_INPUT_ELEMENT_DESC>> ShaderToInputLayoutMap;
	TMap<FString, FString> TextureToShaderMap;

	TArray<UStaticMesh*> StaticMeshs;

	// Deprecated
	TMap<FString, FResourceData*> ResourceMap;
	TMap<FWideString, FTextureData*> TextureMap;
	FShader PrimitiveShader;
	TMap<FWideString, FShader*> ShaderList;

private:
	// --- 비공개 정적 헬퍼 함수 ---
	static FString GenerateShaderKey(const FString& InFilePath, const TArray<FShaderMacro>& InMacros);

	// --- 비공개 멤버 변수 ---
	TMap<FString, UMaterial*> MaterialMap;

	// Cache for per-mesh BVHs to avoid rebuilding for identical OBJ assets
	TMap<FString, FMeshBVH*> MeshBVHCache;

	UMaterial* DefaultMaterialInstance;

	// Shader Hot Reload
	float ShaderCheckTimer = 0.0f;
	const float ShaderCheckInterval = 0.5f; // Check every 0.5 seconds
};

//-----definition
// 리소스 매니저에 새로운 리소스 등록하는 함수이다.
template<typename T>
bool UResourceManager::Add(const FString& InFilePath, UObject* InObject)
{
	// 경로 정규화: 모든 백슬래시를 슬래시로 변환하여 일관성 유지
	FString NormalizedPath = NormalizePath(InFilePath);

	uint8 typeIndex = static_cast<uint8>(GetResourceType<T>());
	auto iter = Resources[typeIndex].find(NormalizedPath);
	if (iter == Resources[typeIndex].end())
	{
		Resources[typeIndex][NormalizedPath] = static_cast<T*>(InObject);
		// 경로 저장
		Resources[typeIndex][NormalizedPath]->SetFilePath(NormalizedPath);
		return true;
	}
	return false;
}

template<typename T>
T* UResourceManager::Get(const FString& InFilePath)
{
	// 경로 정규화: 모든 백슬래시를 슬래시로 변환하여 일관성 유지
	FString NormalizedPath = NormalizePath(InFilePath);

	uint8 typeIndex = static_cast<uint8>(GetResourceType<T>());
	auto iter = Resources[typeIndex].find(NormalizedPath);
	if (iter != Resources[typeIndex].end())
	{
		return static_cast<T*>(iter->second);
	}

	return nullptr;
}

template<typename T, typename ...Args>
inline T* UResourceManager::Load(const FString& InFilePath, Args && ...InArgs)
{
	// 경로 정규화: 모든 백슬래시를 슬래시로 변환하여 일관성 유지
	FString NormalizedPath = NormalizePath(InFilePath);

	uint8 typeIndex = static_cast<uint8>(GetResourceType<T>());
	auto iter = Resources[typeIndex].find(NormalizedPath);
	if (iter != Resources[typeIndex].end())
	{
		return static_cast<T*>((*iter).second);
	}
	else//없으면 해당 리소스의 Load실행
	{
		T* Resource = NewObject<T>();
		Resource->Load(NormalizedPath, Device);
		Resource->SetFilePath(NormalizedPath);
		Resources[typeIndex][NormalizedPath] = Resource;
		return Resource;
	}
}

// UTexture 특수화: DDS 캐시 사용 시 실제 로드된 경로를 키로 사용
template<>
inline UTexture* UResourceManager::Load(const FString& InFilePath)
{
	// 경로 정규화: 모든 백슬래시를 슬래시로 변환하여 일관성 유지
	FString NormalizedPath = NormalizePath(InFilePath);

	uint8 typeIndex = static_cast<uint8>(ResourceType::Texture);

	// 먼저 원본 경로로 검색
	auto iter = Resources[typeIndex].find(NormalizedPath);
	if (iter != Resources[typeIndex].end())
	{
		return static_cast<UTexture*>((*iter).second);
	}

#ifdef USE_DDS_CACHE
	// DDS 캐시 사용 시: DDS 캐시 경로로도 검색 시도
	FString PotentialDDSPath = FTextureConverter::GetDDSCachePath(NormalizedPath);
	if (PotentialDDSPath != NormalizedPath) // 경로가 변환되었으면
	{
		auto ddsIter = Resources[typeIndex].find(PotentialDDSPath);
		if (ddsIter != Resources[typeIndex].end())
		{
			// 이미 DDS 캐시로 로드된 텍스처가 있으면 반환
			return static_cast<UTexture*>((*ddsIter).second);
		}
	}
#endif

	// 없으면 새로 로드 (기본값: bSRGB = true)
	UTexture* Resource = NewObject<UTexture>();
	FString ActualLoadPath = Resource->Load(NormalizedPath, Device, true); // 기본값: sRGB = true

	// 실제 로드된 경로로 등록 (DDS 캐시 사용 시 DDS 경로, 이미 정규화되어 있음)
	Resource->SetFilePath(ActualLoadPath);
	Resources[typeIndex][ActualLoadPath] = Resource;

	return Resource;
}

// UTexture 로드 함수: bSRGB 파라미터 지원 (Diffuse=true, Normal=false)
inline UTexture* UResourceManager::LoadTexture(const FString& InFilePath, bool bSRGB)
{
	// 경로 정규화: 모든 백슬래시를 슬래시로 변환하여 일관성 유지
	FString NormalizedPath = NormalizePath(InFilePath);

	uint8 typeIndex = static_cast<uint8>(ResourceType::Texture);

	// 먼저 원본 경로로 검색
	auto iter = Resources[typeIndex].find(NormalizedPath);
	if (iter != Resources[typeIndex].end())
	{
		return static_cast<UTexture*>((*iter).second);
	}

#ifdef USE_DDS_CACHE
	// DDS 캐시 사용 시: DDS 캐시 경로로도 검색 시도
	FString PotentialDDSPath = FTextureConverter::GetDDSCachePath(NormalizedPath);
	if (PotentialDDSPath != NormalizedPath) // 경로가 변환되었으면
	{
		auto ddsIter = Resources[typeIndex].find(PotentialDDSPath);
		if (ddsIter != Resources[typeIndex].end())
		{
			// 이미 DDS 캐시로 로드된 텍스처가 있으면 반환
			return static_cast<UTexture*>((*ddsIter).second);
		}
	}
#endif

	// 없으면 새로 로드 (bSRGB 파라미터 전달)
	UTexture* Resource = NewObject<UTexture>();
	FString ActualLoadPath = Resource->Load(NormalizedPath, Device, bSRGB);

	// 실제 로드된 경로로 등록
	Resource->SetFilePath(ActualLoadPath);
	Resources[typeIndex][ActualLoadPath] = Resource;

	return Resource;
}

template<>
inline UShader* UResourceManager::Load(const FString& InFilePath, TArray<FShaderMacro>& InMacros)
{
	// 경로 정규화: 모든 백슬래시를 슬래시로 변환하여 일관성 유지
	FString NormalizedPath = NormalizePath(InFilePath);

	// 1. 파일 경로와 매크로를 조합하여 고유 키 생성
	FString UniqueKey = GenerateShaderKey(NormalizedPath, InMacros);

	// 2. 고유 키로 리소스 맵 검색
	uint8 typeIndex = static_cast<uint8>(ResourceType::Shader);
	auto iter = Resources[typeIndex].find(UniqueKey);
	if (iter != Resources[typeIndex].end())
	{
		return static_cast<UShader*>((*iter).second);
	}
	else
	{
		// 3. 캐시에 없으면 새로 생성하여 로드
		UShader* Resource = NewObject<UShader>();
		// UShader::Load는 이제 매크로 인자를 받도록 수정되어야 함
		Resource->Load(NormalizedPath, Device, InMacros);
		Resource->SetFilePath(UniqueKey); // 이름 저장

		// 4. 고유 키로 리소스 맵에 저장
		Resources[typeIndex][UniqueKey] = Resource;
		return Resource;
	}
}

template<typename T>
ResourceType UResourceManager::GetResourceType()
{
	if (T::StaticClass() == UStaticMesh::StaticClass())
		return ResourceType::StaticMesh;
	if (T::StaticClass() == UQuad::StaticClass())
		return ResourceType::Quad;
	if (T::StaticClass() == UDynamicMesh::StaticClass())
		return ResourceType::DynamicMesh;
	if (T::StaticClass() == ULineDynamicMesh::StaticClass())
		return ResourceType::DynamicMesh; // share bucket with DynamicMesh
	if (T::StaticClass() == UShader::StaticClass())
		return ResourceType::Shader;
	if (T::StaticClass() == UTexture::StaticClass())
		return ResourceType::Texture;
	if (T::StaticClass() == UMaterial::StaticClass())
		return ResourceType::Material;

	return ResourceType::None;
}

// Enumerate all resources of a type T
template<typename T>
TArray<T*> UResourceManager::GetAll()
{
	TArray<T*> Result;
	uint8 TypeIndex = static_cast<uint8>(GetResourceType<T>());
	if (TypeIndex >= Resources.size())
	{
		return Result;
	}

	for (auto& Pair : Resources[TypeIndex])
	{
		if (Pair.second)
		{
			Result.push_back(static_cast<T*>(Pair.second));
		}
	}
	return Result;
}

// Collect non-empty FilePath of all resources of type T
template<typename T>
TArray<FString> UResourceManager::GetAllFilePaths()
{
	TArray<FString> Paths;
	uint8 TypeIndex = static_cast<uint8>(GetResourceType<T>());
	if (TypeIndex >= Resources.size())
	{
		return Paths;
	}

	for (auto& Pair : Resources[TypeIndex])
	{
		if (Pair.second)
		{
			const FString& Path = Pair.second->GetFilePath();
			if (!Path.empty())
			{
				Paths.push_back(Path);
			}
		}
	}
	return Paths;
}

#include "pch.h"
#pragma warning(push)
#pragma warning(disable: 4244) // Disable double to float conversion warning for FBX SDK
#include "ObjectFactory.h"
#include "FbxLoader.h"
#include "FbxParser.h"
#include "FbxHelper.h"
#include "FbxMaterial.h"
#include "FbxAnimation.h"
#include "FbxScene.h"
#include "FbxMesh.h"
#include "ObjectIterator.h"
#include "WindowsBinReader.h"
#include "WindowsBinWriter.h"
#include "PathUtils.h"
#include <filesystem>
#include "AnimSequence.h"
#include "AnimationTypes.h"
#include "FbxManager.h"

IMPLEMENT_CLASS(UFbxLoader)

UFbxLoader::UFbxLoader()
	: Parser(nullptr)
{
	// FFbxParser 생성 (FBX SDK 관리 및 orchestration 담당)
	Parser = new FFbxParser();
}

void UFbxLoader::PreLoad()
{
	UFbxLoader& FbxLoader = GetInstance();

	const fs::path DataDir(GDataDir);

	if (!fs::exists(DataDir) || !fs::is_directory(DataDir))
	{
		UE_LOG("UFbxLoader::Preload: Data directory not found: %s", DataDir.string().c_str());
		return;
	}

	size_t LoadedCount = 0;
	std::unordered_set<FString> ProcessedFiles; // 중복 로딩 방지

	// Phase 1: Load .fbx files and textures first
	for (const auto& Entry : fs::recursive_directory_iterator(DataDir))
	{
		if (!Entry.is_regular_file())
			continue;

		const fs::path& Path = Entry.path();
		FString Extension = Path.extension().string();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		if (Extension == ".fbx")
		{
			FString PathStr = NormalizePath(Path.string());

			// 이미 처리된 파일인지 확인
			if (ProcessedFiles.find(PathStr) == ProcessedFiles.end())
			{
				ProcessedFiles.insert(PathStr);

				// 메쉬 로드
				USkeletalMesh* Mesh = FbxLoader.LoadFbxMesh(PathStr);
				++LoadedCount;

				// 스켈레톤이 있으면 애니메이션 로드 시도
				if (Mesh && Mesh->GetSkeleton())
				{
					UAnimSequence* Anim = FbxLoader.LoadFbxAnimation(PathStr, Mesh->GetSkeleton());
					if (Anim)
					{
						UE_LOG("  - Animation loaded: %d frames, %d bone tracks",
							   Anim->NumberOfFrames,
							   Anim->GetBoneAnimationTracks().Num());
					}
				}
			}
		}
		else if (Extension == ".dds" || Extension == ".jpg" || Extension == ".png")
		{
			// .fbm 폴더의 텍스처는 제외 (FBX embedded texture는 FBX 로드 시 자동 처리됨)
			FString PathStr = NormalizePath(Path.string());
			if (PathStr.find(".fbm") == FString::npos)
			{
				UResourceManager::GetInstance().Load<UTexture>(Path.string()); // 데칼 텍스쳐를 ui에서 고를 수 있게 하기 위해 임시로 만듬.
			}
		}
	}

	// Phase 2: Load .anim files after all Skeletons are loaded
	for (const auto& Entry : fs::recursive_directory_iterator(DataDir))
	{
		if (!Entry.is_regular_file())
			continue;

		const fs::path& Path = Entry.path();
		FString Extension = Path.extension().string();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		if (Extension == ".anim")
		{
			FString AnimPath = NormalizePath(Path.string());

			try
			{
				// 1. 파일에서 AnimSequence 역직렬화
				FWindowsBinReader Reader(AnimPath);
				if (!Reader.IsOpen())
				{
					continue;
				}

				UAnimSequence* LoadedAnimSeq = NewObject<UAnimSequence>();
				Reader << *LoadedAnimSeq;
				Reader.Close();

				// 2. Skeleton 찾기 (모든 SkeletalMesh 순회)
				// 우선순위: 같은 이름의 FBX 메시 > 아무 메시
				bool bFoundSkeleton = false;
				USkeletalMesh* FallbackMesh = nullptr;

				// Extract base name from .anim file (without extension)
				FString AnimBaseName = AnimPath;
				size_t lastSlash = AnimBaseName.find_last_of("/\\");
				if (lastSlash != FString::npos)
				{
					AnimBaseName = AnimBaseName.substr(lastSlash + 1);
				}
				size_t lastDot = AnimBaseName.find_last_of('.');
				if (lastDot != FString::npos)
				{
					AnimBaseName = AnimBaseName.substr(0, lastDot);
				}

				for (TObjectIterator<USkeletalMesh> It; It; ++It)
				{
					USkeletalMesh* ExistingMesh = *It;
					if (ExistingMesh && ExistingMesh->GetSkeleton())
					{
						// Keep first mesh as fallback
						if (!FallbackMesh)
						{
							FallbackMesh = ExistingMesh;
						}

						// Try to find mesh with matching name
						FString MeshPath = ExistingMesh->GetFilePath();
						if (!MeshPath.empty())
						{
							FString MeshBaseName = MeshPath;
							size_t meshSlash = MeshBaseName.find_last_of("/\\");
							if (meshSlash != FString::npos)
							{
								MeshBaseName = MeshBaseName.substr(meshSlash + 1);
							}
							size_t meshDot = MeshBaseName.find_last_of('.');
							if (meshDot != FString::npos)
							{
								MeshBaseName = MeshBaseName.substr(0, meshDot);
							}

							// If names match, use this skeleton
							if (MeshBaseName == AnimBaseName)
							{
								LoadedAnimSeq->Skeleton = const_cast<FSkeleton*>(ExistingMesh->GetSkeleton());
								bFoundSkeleton = true;
								break;
							}
						}
					}
				}

				// If no matching name found, use fallback mesh
				if (!bFoundSkeleton && FallbackMesh)
				{
					LoadedAnimSeq->Skeleton = const_cast<FSkeleton*>(FallbackMesh->GetSkeleton());
					bFoundSkeleton = true;
				}

				if (!bFoundSkeleton)
				{
					continue;
				}

				LoadedAnimSeq->SetFilePath(AnimPath);

				// 3. ObjectName 설정 (이미 추출된 AnimBaseName 사용)
				LoadedAnimSeq->ObjectName = FName(AnimBaseName);

				// 4. ResourceManager에 등록
				UResourceManager::GetInstance().Add<UAnimSequence>(AnimPath, LoadedAnimSeq);

				++LoadedCount;
			}
			catch (const std::exception&)
			{
			}
		}
	}
	RESOURCE.SetSkeletalMeshs();

	UE_LOG("UFbxLoader::Preload: Loaded %zu .fbx files from %s", LoadedCount, DataDir.string().c_str());
}


UFbxLoader::~UFbxLoader()
{
	// Phase 7: FFbxParser 소멸 (FbxManager는 FFbxParser 소멸자에서 정리됨)
	if (Parser)
	{
		delete Parser;
		Parser = nullptr;
	}
}
UFbxLoader& UFbxLoader::GetInstance()
{
	static UFbxLoader* FbxLoader = nullptr;
	if (!FbxLoader)
	{
		FbxLoader = ObjectFactory::NewObject<UFbxLoader>();
	}
	return *FbxLoader;
}

USkeletalMesh* UFbxLoader::LoadFbxMesh(const FString& FilePath)
{
	// 0) 경로
	FString NormalizedPathStr = NormalizePath(FilePath);

	// 1) 이미 로드된 USkeletalMesh가 있는지 전체 검색 (정규화된 경로로 비교)
	for (TObjectIterator<USkeletalMesh> It; It; ++It)
	{
		USkeletalMesh* SkeletalMesh = *It;

		if (SkeletalMesh->GetFilePath() == NormalizedPathStr)
		{
			// Skeleton 오버라이드 데이터 자동 로드
			FString OverridePath = NormalizedPathStr + ".skeleton";
			if (std::filesystem::exists(OverridePath))
			{
				SkeletalMesh->LoadOverrideData(OverridePath);
			}
			return SkeletalMesh;
		}
	}

	// 2) 없으면 새로 로드 (정규화된 경로 사용)
	USkeletalMesh* SkeletalMesh = UResourceManager::GetInstance().Load<USkeletalMesh>(NormalizedPathStr);

	// Skeleton 오버라이드 데이터 자동 로드
	FString OverridePath = NormalizedPathStr + ".skeleton";
	if (std::filesystem::exists(OverridePath))
	{
		SkeletalMesh->LoadOverrideData(OverridePath);
	}

	UE_LOG("USkeletalMesh(filename: \'%s\') is successfully crated!", NormalizedPathStr.c_str());
	return SkeletalMesh;
}

// ========================================
// Phase 7: GetOrLoadFbxScene()와 ClearCachedScene()는
// FFbxParser::LoadFbxScene()과 FFbxParser::Reset()으로 이동됨
// ========================================

FSkeletalMesh* UFbxLoader::LoadFbxMeshAsset(const FString& FilePath)
{
	MaterialInfos.clear();
	FString NormalizedPath = NormalizePath(FilePath);
	FSkeletalMesh* MeshData = nullptr;
#ifdef USE_OBJ_CACHE
	// 1. 캐시 파일 경로 설정
	FString CachePathStr = ConvertDataPathToCachePath(NormalizedPath);
	const FString BinPathFileName = CachePathStr + ".bin";

	// 캐시를 저장할 디렉토리가 없으면 생성
	std::filesystem::path CacheFileDirPath(BinPathFileName);
	if (CacheFileDirPath.has_parent_path())
	{
		std::filesystem::create_directories(CacheFileDirPath.parent_path());
	}

	bool bLoadedFromCache = false;

	// 2. 캐시 유효성 검사
	bool bShouldRegenerate = true;
	bool bCacheExists = std::filesystem::exists(UTF8ToWide(BinPathFileName));

	if (bCacheExists)
	{
		try
		{
			auto binTime = std::filesystem::last_write_time(UTF8ToWide(BinPathFileName));
			auto fbxTime = std::filesystem::last_write_time(UTF8ToWide(NormalizedPath));

			// FBX 파일이 캐시보다 오래되었으면 캐시 사용
			if (fbxTime <= binTime)
			{
				bShouldRegenerate = false;
			}
		}
		catch (const std::filesystem::filesystem_error& e)
		{
			UE_LOG("Filesystem error during cache validation: %s. Forcing regeneration.", e.what());
			bShouldRegenerate = true;
		}
	}

	// 3. 캐시에서 로드 시도
	if (!bShouldRegenerate)
	{
		try
		{
			MeshData = new FSkeletalMesh();
			MeshData->PathFileName = NormalizedPath;

			FWindowsBinReader Reader(BinPathFileName);
			if (!Reader.IsOpen())
			{
				throw std::runtime_error("Failed to open bin file for reading.");
			}
			Reader << *MeshData;
			Reader.Close();

			for (int Index = 0; Index < MeshData->GroupInfos.Num(); Index++)
			{
				if (MeshData->GroupInfos[Index].InitialMaterialName.empty())
					continue;

				const FString& MaterialName = MeshData->GroupInfos[Index].InitialMaterialName;
				const FString& MaterialFilePath = ConvertDataPathToCachePath(MaterialName + ".mat.bin");

				// 머티리얼 캐시 파일 존재 확인
				if (!std::filesystem::exists(UTF8ToWide(MaterialFilePath)))
				{
					UE_LOG("Material cache not found: %s", MaterialFilePath.c_str());
					throw std::runtime_error("Material cache file missing.");
				}

				// 머티리얼 캐시 타임스탬프 검증
				auto matTime = std::filesystem::last_write_time(UTF8ToWide(MaterialFilePath));
				auto binTime = std::filesystem::last_write_time(UTF8ToWide(BinPathFileName));

				if (matTime < binTime)
				{
					UE_LOG("Material cache older than mesh cache: %s", MaterialFilePath.c_str());
					throw std::runtime_error("Material cache outdated.");
				}

				// 머티리얼 캐시 로드
				FWindowsBinReader MatReader(MaterialFilePath);
				if (!MatReader.IsOpen())
				{
					throw std::runtime_error("Failed to open material bin file for reading.");
				}

				// for bin Load
				FMaterialInfo MaterialInfo{};
				Serialization::ReadAsset<FMaterialInfo>(MatReader, &MaterialInfo);

				UMaterial* NewMaterial = NewObject<UMaterial>();

				UMaterial* Default = UResourceManager::GetInstance().GetDefaultMaterial();
				NewMaterial->SetMaterialInfo(MaterialInfo);
				NewMaterial->SetShader(Default->GetShader());
				NewMaterial->SetShaderMacros(Default->GetShaderMacros());
				UResourceManager::GetInstance().Add<UMaterial>(MaterialInfo.MaterialName, NewMaterial);
			}

			MeshData->CacheFilePath = BinPathFileName;
			bLoadedFromCache = true;

			// Skeleton Name 설정 (캐시에서 로드된 경우에도 필요)
			if (MeshData->Skeleton && MeshData->Skeleton->Name.empty())
			{
				std::filesystem::path Path(NormalizedPath);
				MeshData->Skeleton->Name = Path.stem().string();  // "Character.fbx" → "Character"
			}

			// 캐시에서 로드한 경우 Skeleton 처리
			if (MeshData->Skeleton)
			{
				FSkeleton* ExistingSkeleton = FFbxManager::GetSkeleton(NormalizedPath);
				if (ExistingSkeleton)
				{
					// 이미 등록된 Skeleton이 있으면 재사용 (메모리 누수 방지)
					delete MeshData->Skeleton;
					MeshData->Skeleton = ExistingSkeleton;
				}
				else
				{
					// 없으면 새로 등록
					FFbxManager::RegisterSkeleton(NormalizedPath, MeshData->Skeleton);
				}
			}

			UE_LOG("Successfully loaded FBX '%s' from cache.", NormalizedPath.c_str());
			return MeshData;
		}
		catch (const std::exception& e)
		{
			UE_LOG("Error loading FBX from cache: %s. Cache might be corrupt or incompatible.", e.what());
			UE_LOG("Deleting corrupt cache and forcing regeneration for '%s'.", NormalizedPath.c_str());

			// 관련된 모든 캐시 파일 삭제
			// 1. 머티리얼 캐시 삭제
			if (MeshData && MeshData->GroupInfos.Num() > 0)
			{
				for (int Index = 0; Index < MeshData->GroupInfos.Num(); Index++)
				{
					if (!MeshData->GroupInfos[Index].InitialMaterialName.empty())
					{
						const FString& MaterialName = MeshData->GroupInfos[Index].InitialMaterialName;
						const FString& MaterialFilePath = ConvertDataPathToCachePath(MaterialName + ".mat.bin");

						if (std::filesystem::exists(UTF8ToWide(MaterialFilePath)))
						{
							std::filesystem::remove(UTF8ToWide(MaterialFilePath));
							UE_LOG("Deleted corrupt material cache: %s", MaterialFilePath.c_str());
						}
					}
				}
			}

			// 2. 메시 캐시 삭제
			if (std::filesystem::exists(UTF8ToWide(BinPathFileName)))
			{
				std::filesystem::remove(UTF8ToWide(BinPathFileName));
				UE_LOG("Deleted corrupt mesh cache: %s", BinPathFileName.c_str());
			}

			// 3. MeshData 정리
			if (MeshData)
			{
				delete MeshData;
				MeshData = nullptr;
			}
			bLoadedFromCache = false;
		}
	}

	// 4. 캐시 로드 실패 시 FBX 파싱
	UE_LOG("=== FBX PARSING START ===");
	UE_LOG("  Regenerating cache for FBX '%s'...", NormalizedPath.c_str());
#endif // USE_OBJ_CACHE

	// ========================================
	// Phase 7: FFbxParser로 위임 (Immediate Loading)
	// ========================================
	MeshData = Parser->LoadFbxMesh(NormalizedPath, MaterialInfos);
	if (!MeshData)
	{
		UE_LOG("Error: Failed to load FBX mesh: %s", NormalizedPath.c_str());
		return nullptr;
	}

	// Skeleton Name 설정 (FilePath 기반)
	if (MeshData->Skeleton)
	{
		std::filesystem::path Path(NormalizedPath);
		MeshData->Skeleton->Name = Path.stem().string();  // "Character.fbx" → "Character"
	}

	// FFbxManager에 Skeleton 등록 (중복 제거)
	if (MeshData->Skeleton)
	{
		FSkeleton* ExistingSkeleton = FFbxManager::GetSkeleton(NormalizedPath);
		if (ExistingSkeleton)
		{
			// 이미 등록된 Skeleton이 있으면 재사용 (메모리 누수 방지)
			delete MeshData->Skeleton;
			MeshData->Skeleton = ExistingSkeleton;
			UE_LOG("Reusing existing Skeleton from FFbxManager: %s", ExistingSkeleton->Name.c_str());
		}
		else
		{
			// 없으면 새로 등록
			FFbxManager::RegisterSkeleton(NormalizedPath, MeshData->Skeleton);
			UE_LOG("Registered Skeleton to FFbxManager: %s", MeshData->Skeleton->Name.c_str());
		}
	}

#ifdef USE_OBJ_CACHE
	// 5. 캐시 저장
	try
	{
		FWindowsBinWriter Writer(BinPathFileName);
		Writer << *MeshData;
		Writer.Close();

		for (FMaterialInfo& MaterialInfo : MaterialInfos)
		{
			
			const FString MaterialFilePath = ConvertDataPathToCachePath(MaterialInfo.MaterialName + ".mat.bin");
			FWindowsBinWriter MatWriter(MaterialFilePath);
			Serialization::WriteAsset<FMaterialInfo>(MatWriter, &MaterialInfo);
			MatWriter.Close();
		}

		MeshData->CacheFilePath = BinPathFileName;

		UE_LOG("Cache regeneration complete for FBX '%s'.", NormalizedPath.c_str());
	}
	catch (const std::exception& e)
	{
		UE_LOG("Failed to save FBX cache: %s", e.what());
	}
#endif // USE_OBJ_CACHE

	return MeshData;
}

// ========================================
// Animation Loading Implementation
// ========================================

UAnimSequence* UFbxLoader::LoadFbxAnimation(const FString& FilePath, const FSkeleton* TargetSkeleton)
{
	// 1. 경로 정규화
	FString NormalizedPath = NormalizePath(FilePath);

	// 2. 이미 로드된 리소스 확인
	for (TObjectIterator<UAnimSequence> It; It; ++It)
	{
		UAnimSequence* AnimSeq = *It;
		if (AnimSeq->GetFilePath() == NormalizedPath)
		{
			UE_LOG("Animation already loaded: %s", NormalizedPath.c_str());

			// 오버라이드 데이터 자동 로드
			FString OverridePath = NormalizedPath + ".skeleton";
			if (std::filesystem::exists(OverridePath))
			{
				AnimSeq->LoadOverrideData(OverridePath);
			}

			return AnimSeq;
		}
	}

	UAnimSequence* AnimSeq = nullptr;

#ifdef USE_OBJ_CACHE
	// 3. 캐시 파일 경로 설정
	FString CachePathStr = ConvertDataPathToCachePath(NormalizedPath);
	const FString AnimBinPath = CachePathStr + ".anim.bin";

	// 캐시 디렉토리 생성
	std::filesystem::path CacheFileDirPath(AnimBinPath);
	if (CacheFileDirPath.has_parent_path())
	{
		std::filesystem::create_directories(CacheFileDirPath.parent_path());
	}

	bool bLoadedFromCache = false;

	// 4. 캐시 유효성 검사
	bool bShouldRegenerate = true;
	bool bCacheExists = std::filesystem::exists(UTF8ToWide(AnimBinPath));

	if (bCacheExists)
	{
		try
		{
			auto binTime = std::filesystem::last_write_time(UTF8ToWide(AnimBinPath));
			auto fbxTime = std::filesystem::last_write_time(UTF8ToWide(NormalizedPath));

			if (fbxTime <= binTime)
			{
				bShouldRegenerate = false;
			}
		}
		catch (const std::filesystem::filesystem_error& e)
		{
			UE_LOG("Filesystem error during animation cache validation: %s. Forcing regeneration.", e.what());
			bShouldRegenerate = true;
		}
	}

	// 5. 캐시에서 로드 시도
	if (!bShouldRegenerate)
	{
		try
		{
			AnimSeq = NewObject<UAnimSequence>();
			AnimSeq->SetFilePath(NormalizedPath);

			FWindowsBinReader Reader(AnimBinPath);
			if (!Reader.IsOpen())
			{
				throw std::runtime_error("Failed to open animation cache file.");
			}

			Reader << *AnimSeq;
			Reader.Close();

			// Skeleton 포인터는 바이너리 캐시에 저장되지 않으므로 역직렬화 후 설정
			AnimSeq->Skeleton = const_cast<FSkeleton*>(TargetSkeleton);

			bLoadedFromCache = true;
			UE_LOG("Successfully loaded animation '%s' from cache.", NormalizedPath.c_str());
		}
		catch (const std::exception& e)
		{
			UE_LOG("Error loading animation from cache: %s. Regenerating...", e.what());

			// 손상된 캐시 삭제
			if (std::filesystem::exists(UTF8ToWide(AnimBinPath)))
			{
				std::filesystem::remove(UTF8ToWide(AnimBinPath));
			}

			if (AnimSeq)
			{
				delete AnimSeq;
				AnimSeq = nullptr;
			}

			bLoadedFromCache = false;
			bShouldRegenerate = true;
		}
	}
#endif

	// 6. 캐시가 없거나 유효하지 않으면 FBX에서 로드
	if (!AnimSeq)
	{
		// ========================================
		// Phase 7: FFbxParser로 위임 (Immediate Loading)
		// ========================================
		AnimSeq = Parser->LoadFbxAnimation(NormalizedPath, TargetSkeleton);
		if (!AnimSeq)
		{
			UE_LOG("Error: Failed to load FBX animation: %s", NormalizedPath.c_str());
			return nullptr;
		}

		// Skeleton 포인터 명시적 설정 (FFbxManager 관리 포인터 사용)
		AnimSeq->Skeleton = const_cast<FSkeleton*>(TargetSkeleton);

#ifdef USE_OBJ_CACHE
		// 7. 캐시 저장
		try
		{
			FWindowsBinWriter Writer(AnimBinPath);
			if (!Writer.IsOpen())
			{
				throw std::runtime_error("Failed to open animation cache for writing.");
			}

			Writer << *AnimSeq;
			Writer.Close();

			UE_LOG("Animation cache saved: %s", AnimBinPath.c_str());
		}
		catch (const std::exception& e)
		{
			UE_LOG("Warning: Failed to save animation cache: %s", e.what());
		}
#endif
	}

	// 8. 리소스 매니저에 등록
	UResourceManager::GetInstance().Add<UAnimSequence>(NormalizedPath, AnimSeq);

	// 오버라이드 데이터 자동 로드
	FString OverridePath = NormalizedPath + ".skeleton";
	if (std::filesystem::exists(OverridePath))
	{
		AnimSeq->LoadOverrideData(OverridePath);
	}

	return AnimSeq;
}


#pragma warning(pop) // Restore warning state

#pragma once
#include "Object.h"
#include "fbxsdk.h"

// Forward declarations for animation types
class UAnimSequence;
struct FBoneAnimationTrack;
struct FSkeleton;

class UFbxLoader : public UObject
{
public:

	DECLARE_CLASS(UFbxLoader, UObject)
	static UFbxLoader& GetInstance();
	UFbxLoader();

	static void PreLoad();

	USkeletalMesh* LoadFbxMesh(const FString& FilePath);

	FSkeletalMeshData* LoadFbxMeshAsset(const FString& FilePath);

	UAnimSequence* LoadFbxAnimation(const FString& FilePath, const FSkeleton* TargetSkeleton);

protected:
	~UFbxLoader() override;
private:
	UFbxLoader(const UFbxLoader&) = delete;
	UFbxLoader& operator=(const UFbxLoader&) = delete;


	void LoadMeshFromNode(FbxNode* InNode, FSkeletalMeshData& MeshData, TMap<int32, TArray<uint32>>& MaterialGroupIndexList, TMap<FbxNode*, int32>& BoneToIndex, TMap<FbxSurfaceMaterial*, int32>& MaterialToIndex, FbxScene* Scene);

	void LoadSkeletonFromNode(FbxNode* InNode, FSkeletalMeshData& MeshData, int32 ParentNodeIndex, TMap<FbxNode*, int32>& BoneToIndex);

	void LoadMeshFromAttribute(FbxNodeAttribute* InAttribute, FSkeletalMeshData& MeshData);

	void LoadMesh(FbxMesh* InMesh, FSkeletalMeshData& MeshData, TMap<int32, TArray<uint32>>& MaterialGroupIndexList, TMap<FbxNode*, int32>& BoneToIndex, TArray<int32> MaterialSlotToIndex, FbxScene* Scene, int32 DefaultMaterialIndex = 0);

	void LoadAnimationFromStack(FbxAnimStack* AnimStack, const FSkeleton* TargetSkeleton, UAnimSequence* OutAnim);

	void ExtractBoneAnimationTracks(FbxNode* RootNode, FbxAnimLayer* AnimLayer, const FSkeleton* TargetSkeleton, UAnimSequence* OutAnim, int& DebugBoneCount);

	void ExtractBoneCurve(FbxNode* BoneNode, FbxAnimLayer* AnimLayer, const FSkeleton* TargetSkeleton, FBoneAnimationTrack& OutTrack, int& DebugBoneCount);

	void ParseMaterial(FbxSurfaceMaterial* Material, FMaterialInfo& MaterialInfo);

	FString ParseTexturePath(FbxProperty& Property);

	FbxString GetAttributeTypeName(FbxNodeAttribute* InAttribute);

	void EnsureSingleRootBone(FSkeletalMeshData& MeshData);

	// Scene 캐싱 및 관리
	FbxScene* GetOrLoadFbxScene(const FString& FilePath, bool& bOutNewlyLoaded);
	void ClearCachedScene();

	// Scene 캐싱 구조
	struct FCachedFbxScene
	{
		FbxScene* Scene = nullptr;
		FString FilePath;
		bool bConverted = false;
	};

	FCachedFbxScene CachedScene;

	// bin파일 저장용
	TArray<FMaterialInfo> MaterialInfos;
	FbxManager* SdkManager = nullptr;
	
};
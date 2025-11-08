#pragma once
#include "Object.h"
#include "fbxsdk.h"

class UFbxLoader : public UObject
{
public:

	DECLARE_CLASS(UFbxLoader, UObject)
	static UFbxLoader& GetInstance();
	UFbxLoader();

	FSkeletalMeshData LoadFbxMesh(const FString& FilePath);

protected:
	~UFbxLoader() override;
private:
	UFbxLoader(const UFbxLoader&) = delete;
	UFbxLoader& operator=(const UFbxLoader&) = delete;


	void LoadMeshFromNode(FbxNode* InNode, FSkeletalMeshData& MeshData, TMap<FbxNode*, int32>& BoneToIndex);

	void LoadSkeletonFromNode(FbxNode* InNode, FSkeletalMeshData& MeshData, int32 ParentNodeIndex, TMap<FbxNode*, int32>& BoneToIndex);

	void LoadMeshFromAttribute(FbxNodeAttribute* InAttribute, FSkeletalMeshData& MeshData);

	void LoadMesh(FbxMesh* InMesh, FSkeletalMeshData& MeshData, TMap<FbxNode*, int32>& BoneToIndex);

	FbxString GetAttributeTypeName(FbxNodeAttribute* InAttribute);

	

	FbxManager* SdkManager = nullptr;
	
};
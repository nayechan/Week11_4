#pragma once

#include <fstream>
#include <sstream>

#include "nlohmann/json.hpp"   
#include "Vector.h"
#include "UEContainer.h"
using namespace json;

struct FPrimitiveData
{
    uint32 UUID = 0;
    FVector Location;
    FVector Rotation;
    FVector Scale;
    FString Type;
    FString ObjStaticMeshAsset;
};

struct FPerspectiveCameraData
{
    FVector Location;
	FVector Rotation;
	float FOV;
	float NearClip;
	float FarClip;
};

class FSceneLoader
{
public:
    static TArray<FPrimitiveData> Load(const FString& FileName, FPerspectiveCameraData* OutCameraData);
    static void Save(TArray<FPrimitiveData> InPrimitiveData, const FPerspectiveCameraData* InCameraData, const FString& SceneName);
    static bool TryReadNextUUID(const FString& FilePath, uint32& OutNextUUID);

private:
    static TArray<FPrimitiveData> Parse(const JSON& Json);
};
#include "pch.h"
#include "Level.h"
#include "SceneLoader.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "SceneRotationUtils.h"
#include "StaticMesh.h"
#include "PrimitiveComponent.h"
#include "CameraActor.h"
#include "CameraComponent.h"

static inline FString RemoveObjExtension(const FString& FileName)
{
    const FString Extension = ".obj";
    const uint64 Sep = FileName.find_last_of("/\\");
    const uint64 Start = (Sep == FString::npos) ? 0 : Sep + 1;
    uint64 End = FileName.size();
    if (End >= Extension.size() && FileName.compare(End - Extension.size(), Extension.size(), Extension) == 0)
        End -= Extension.size();
    if (Start <= End) return FileName.substr(Start, End - Start);
    return FileName;
}

std::unique_ptr<ULevel> ULevelService::CreateNewLevel()
{
    return std::make_unique<ULevel>();
}

FLoadedLevel ULevelService::LoadLevel(const FString& SceneName)
{
    namespace fs = std::filesystem;
    fs::path path = fs::path("Scene") / SceneName;
    if (path.extension().string() != ".Scene")
        path.replace_extension(".Scene");

    const FString FilePath = path.make_preferred().string();

    FLoadedLevel Result{};
    Result.Level = std::make_unique<ULevel>();

    FPerspectiveCameraData CamData{};
    const TArray<FPrimitiveData>& Primitives = FSceneLoader::Load(FilePath, &CamData);

    // Build actors from primitive data
    for (const FPrimitiveData& Primitive : Primitives)
    {
        AStaticMeshActor* StaticMeshActor = NewObject<AStaticMeshActor>();
        StaticMeshActor->SetActorTransform(
            FTransform(
                Primitive.Location,
                SceneRotUtil::QuatFromEulerZYX_Deg(Primitive.Rotation),
                Primitive.Scale));

        // Prefer using UUID from file if present
        if (Primitive.UUID != 0)
            StaticMeshActor->UUID = Primitive.UUID;

        if (UStaticMeshComponent* SMC = StaticMeshActor->GetStaticMeshComponent())
        {
            FPrimitiveData Temp = Primitive;
            SMC->Serialize(true, Temp);

            FString LoadedAssetPath;
            if (UStaticMesh* Mesh = SMC->GetStaticMesh())
            {
                LoadedAssetPath = Mesh->GetAssetPathFileName();
            }

            if (LoadedAssetPath == "Data/Sphere.obj")
            {
                StaticMeshActor->SetCollisionComponent(EPrimitiveType::Sphere);
            }
            else
            {
                StaticMeshActor->SetCollisionComponent();
            }

            FString BaseName = "StaticMesh";
            if (!LoadedAssetPath.empty())
            {
                BaseName = RemoveObjExtension(LoadedAssetPath);
            }
            StaticMeshActor->SetName(BaseName);
        }

        Result.Level->AddActor(StaticMeshActor);
    }

    Result.Camera = CamData;
    return Result;
}

void ULevelService::SaveLevel(const ULevel* Level, const ACameraActor* Camera, const FString& SceneName)
{
    if (!Level) return;

    TArray<FPrimitiveData> Primitives;
    for (AActor* Actor : Level->GetActors())
    {
        if (AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor))
        {
            FPrimitiveData Data;
            Data.UUID = Actor->UUID;
            Data.Type = "StaticMeshComp";
            if (UStaticMeshComponent* SMC = MeshActor->GetStaticMeshComponent())
            {
                SMC->Serialize(false, Data);
            }
            Primitives.push_back(Data);
        }
        else
        {
            FPrimitiveData Data;
            Data.UUID = Actor->UUID;
            Data.Type = "Actor";
            if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
            {
                Prim->Serialize(false, Data);
            }
            else
            {
                Data.Location = Actor->GetActorLocation();
                Data.Rotation = SceneRotUtil::EulerZYX_Deg_FromQuat(Actor->GetActorRotation());
                Data.Scale = Actor->GetActorScale();
            }
            Data.ObjStaticMeshAsset.clear();
            Primitives.push_back(Data);
        }
    }

    const FPerspectiveCameraData* CamPtr = nullptr;
    FPerspectiveCameraData CamData;
    if (Camera && Camera->GetCameraComponent())
    {
        const UCameraComponent* Cam = Camera->GetCameraComponent();
        CamData.Location = Camera->GetActorLocation();
        CamData.Rotation.X = 0.0f;
        CamData.Rotation.Y = Camera->GetCameraPitch();
        CamData.Rotation.Z = Camera->GetCameraYaw();
        CamData.FOV = Cam->GetFOV();
        CamData.NearClip = Cam->GetNearClip();
        CamData.FarClip = Cam->GetFarClip();
        CamPtr = &CamData;
    }

    FSceneLoader::Save(Primitives, CamPtr, SceneName);
}

#include "pch.h"
#include "SkinnedMeshComponent.h"
#include "MeshBatchElement.h"
#include "SceneView.h"

USkinnedMeshComponent::USkinnedMeshComponent() : SkeletalMesh(nullptr)
{
    // 테스트용 기본 메시 설정 (경로는 실제 스켈레탈 메시 캐시 파일로 변경)
   SetSkeletalMesh(GDataDir + "/Test.fbx"); 
}

void USkinnedMeshComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData()) { return; }

    const TArray<FGroupInfo>& MeshGroupInfos = SkeletalMesh->GetMeshGroupInfo();
    auto DetermineMaterialAndShader = [&](uint32 SectionIndex) -> TPair<UMaterialInterface*, UShader*>
    {
       UMaterialInterface* Material = GetMaterial(SectionIndex);
       UShader* Shader = nullptr;

       if (Material && Material->GetShader())
       {
          Shader = Material->GetShader();
       }
       else
       {
          UE_LOG("USkinnedMeshComponent: 머티리얼이 없거나 셰이더가 없어서 기본 머티리얼 사용 section %u.", SectionIndex);
          Material = UResourceManager::GetInstance().GetDefaultMaterial();
          if (Material)
          {
             Shader = Material->GetShader();
          }
          if (!Material || !Shader)
          {
             UE_LOG("USkinnedMeshComponent: 기본 머티리얼이 없습니다.");
             return { nullptr, nullptr };
          }
       }
       return { Material, Shader };
    };

    const bool bHasSections = !MeshGroupInfos.IsEmpty();
    const uint32 NumSectionsToProcess = bHasSections ? static_cast<uint32>(MeshGroupInfos.size()) : 1;

    for (uint32 SectionIndex = 0; SectionIndex < NumSectionsToProcess; ++SectionIndex)
    {
       uint32 IndexCount = 0;
       uint32 StartIndex = 0;

       if (bHasSections)
       {
          const FGroupInfo& Group = MeshGroupInfos[SectionIndex];
          IndexCount = Group.IndexCount;
          StartIndex = Group.StartIndex;
       }
       else
       {
          IndexCount = SkeletalMesh->GetIndexCount();
          StartIndex = 0;
       }

       if (IndexCount == 0)
       {
          continue;
       }

       auto [MaterialToUse, ShaderToUse] = DetermineMaterialAndShader(SectionIndex);
       if (!MaterialToUse || !ShaderToUse)
       {
          continue;
       }

       FMeshBatchElement BatchElement;
       TArray<FShaderMacro> ShaderMacros = View->ViewShaderMacros;
       if (0 < MaterialToUse->GetShaderMacros().Num())
       {
          ShaderMacros.Append(MaterialToUse->GetShaderMacros());
       }
       FShaderVariant* ShaderVariant = ShaderToUse->GetOrCompileShaderVariant(ShaderMacros);

       if (ShaderVariant)
       {
          BatchElement.VertexShader = ShaderVariant->VertexShader;
          BatchElement.PixelShader = ShaderVariant->PixelShader;
          BatchElement.InputLayout = ShaderVariant->InputLayout;
       }
       
       BatchElement.Material = MaterialToUse;

       BatchElement.VertexBuffer = SkeletalMesh->GetVertexBuffer();
       BatchElement.IndexBuffer = SkeletalMesh->GetIndexBuffer();
       BatchElement.VertexStride = SkeletalMesh->GetVertexStride();
       
       BatchElement.IndexCount = IndexCount;
       BatchElement.StartIndex = StartIndex;
       BatchElement.BaseVertexIndex = 0;
       BatchElement.WorldMatrix = GetWorldMatrix();
       BatchElement.ObjectID = InternalIndex;
       BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

       OutMeshBatchElements.Add(BatchElement);
    }
}

void USkinnedMeshComponent::SetSkeletalMesh(const FString& PathFileName)
{
    ClearDynamicMaterials();

    SkeletalMesh = UResourceManager::GetInstance().Load<USkeletalMesh>(PathFileName);
    
    // [변경] SkeletalMesh 및 데이터 유효성 검사
    if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshData())
    {
       const TArray<FGroupInfo>& GroupInfos = SkeletalMesh->GetMeshGroupInfo();

       MaterialSlots.resize(GroupInfos.size());

       for (int i = 0; i < GroupInfos.size(); ++i)
       {
          // FGroupInfo에 InitialMaterialName이 있다고 가정
          SetMaterialByName(i, GroupInfos[i].InitialMaterialName);
       }
       MarkWorldPartitionDirty();
    }
    else
    {
       SkeletalMesh = nullptr;
    }
}

FAABB USkinnedMeshComponent::GetWorldAABB() const
{
   return {};
    // const FTransform WorldTransform = GetWorldTransform();
    // const FMatrix WorldMatrix = GetWorldMatrix();
    //
    // if (!SkeletalMesh)
    // {
    //    const FVector Origin = WorldTransform.TransformPosition(FVector());
    //    return FAABB(Origin, Origin);
    // }
    //
    // const FAABB LocalBound = SkeletalMesh->GetLocalBound(); // <-- 이 함수 구현 필요
    // const FVector LocalMin = LocalBound.Min;
    // const FVector LocalMax = LocalBound.Max;
    //
    // // ... (이하 AABB 계산 로직은 UStaticMeshComponent와 동일) ...
    // const FVector LocalCorners[8] = {
    //    FVector(LocalMin.X, LocalMin.Y, LocalMin.Z),
    //    FVector(LocalMax.X, LocalMin.Y, LocalMin.Z),
    //    // ... (나머지 6개 코너) ...
    //    FVector(LocalMax.X, LocalMax.Y, LocalMax.Z)
    // };
    //
    // FVector4 WorldMin4 = FVector4(LocalCorners[0].X, LocalCorners[0].Y, LocalCorners[0].Z, 1.0f) * WorldMatrix;
    // FVector4 WorldMax4 = WorldMin4;
    //
    // for (int32 CornerIndex = 1; CornerIndex < 8; ++CornerIndex)
    // {
    //    const FVector4 WorldPos = FVector4(LocalCorners[CornerIndex].X
    //       , LocalCorners[CornerIndex].Y
    //       , LocalCorners[CornerIndex].Z
    //       , 1.0f)
    //       * WorldMatrix;
    //    WorldMin4 = WorldMin4.ComponentMin(WorldPos);
    //    WorldMax4 = WorldMax4.ComponentMax(WorldPos);
    // }
    //
    // FVector WorldMin = FVector(WorldMin4.X, WorldMin4.Y, WorldMin4.Z);
    // FVector WorldMax = FVector(WorldMax4.X, WorldMax4.Y, WorldMax4.Z);
    // return FAABB(WorldMin, WorldMax);
}

void USkinnedMeshComponent::OnTransformUpdated()
{
    Super::OnTransformUpdated();
    MarkWorldPartitionDirty();
}

void USkinnedMeshComponent::BeginPlay()
{
   Super::BeginPlay();
}

void USkinnedMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
    
    // @TODO - UStaticMeshComponent처럼 프로퍼티 기반 직렬화 로직 추가
}

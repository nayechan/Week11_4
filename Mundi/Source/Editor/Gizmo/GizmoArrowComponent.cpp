#include "pch.h"
#include "GizmoArrowComponent.h"
#include "EditorEngine.h"
#include "MeshBatchElement.h"
#include "SceneView.h"

IMPLEMENT_CLASS(UGizmoArrowComponent)

UGizmoArrowComponent::UGizmoArrowComponent()
{
	SetStaticMesh("Data/Gizmo/TranslationHandle.obj");

	// 기즈모 셰이더로 설정
	SetMaterialByName(0, "Shaders/UI/Gizmo.hlsl");
}

UGizmoArrowComponent::~UGizmoArrowComponent()
{

}

UMaterial* UGizmoArrowComponent::GetMaterial(uint32 InSectionIndex) const
{
	return GizmoMaterial;
}

void UGizmoArrowComponent::SetMaterial(uint32 InElementIndex, UMaterial* InNewMaterial)
{
	GizmoMaterial = InNewMaterial;
}

float UGizmoArrowComponent::ComputeScreenConstantScale(const FSceneView* View, float TargetPixels) const
{
	float H = (float)std::max<uint32>(1, View->ViewRect.Height());
	float W = (float)std::max<uint32>(1, View->ViewRect.Width());

	const FMatrix& Proj = View->ProjectionMatrix;
	const FMatrix& ViewMatrix = View->ViewMatrix;

	const bool bOrtho = std::fabs(Proj.M[3][3] - 1.0f) < KINDA_SMALL_NUMBER;
	float WorldPerPixel = 0.0f;
	if (bOrtho)
	{
		const float HalfH = 1.0f / Proj.M[1][1];
		WorldPerPixel = (2.0f * HalfH) / H;
	}
	else
	{
		const float YScale = Proj.M[1][1];
		const FVector GizPosWorld = GetWorldLocation();
		const FVector4 GizPos4(GizPosWorld.X, GizPosWorld.Y, GizPosWorld.Z, 1.0f);
		const FVector4 ViewPos4 = GizPos4 * ViewMatrix;
		float Z = ViewPos4.Z;
		if (Z < 1.0f) Z = 1.0f;
		WorldPerPixel = (2.0f * Z) / (H * YScale);
	}

	float ScaleFactor = TargetPixels * WorldPerPixel;
	if (ScaleFactor < 0.001f) ScaleFactor = 0.001f;
	if (ScaleFactor > 10000.0f) ScaleFactor = 10000.0f;
	return ScaleFactor;
}

void UGizmoArrowComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	if (!IsActive() || !StaticMesh)
	{
		return; // 그릴 메시 애셋이 없음
	}

	// --- 스케일 계산 ---
	if (bUseScreenConstantScale)
	{
		if (!View)
		{
			UE_LOG("GizmoArrowComponent requires a valid FSceneView to compute scale. Gizmo might not scale correctly.");
			return;
		}
		else
		{
			const float ScaleFactor = ComputeScreenConstantScale(View, 30.0f);
			SetWorldScale(DefaultScale * ScaleFactor);
		}
	}
	else
	{
		// Use world-space scale directly (for DirectionGizmo etc.)
		SetWorldScale(DefaultScale);
	}

	// --- 사용할 머티리얼 결정 ---
	UMaterial* MaterialToUse = GizmoMaterial;
	UShader* ShaderToUse = nullptr;

	if (MaterialToUse && MaterialToUse->GetShader())
	{
		ShaderToUse = MaterialToUse->GetShader();
	}
	else
	{
		// [Fallback 로직]
		UE_LOG("GizmoArrowComponent: GizmoMaterial is invalid. Falling back to default vertex color shader.");
		MaterialToUse = UResourceManager::GetInstance().Load<UMaterial>("Shaders/UI/Gizmo.hlsl");
		if (MaterialToUse)
		{
			ShaderToUse = MaterialToUse->GetShader();
		}

		if (!MaterialToUse || !ShaderToUse)
		{
			UE_LOG("GizmoArrowComponent: Default vertex color material/shader not found!");
			return;
		}
	}

	// --- FMeshBatchElement 수집 ---
	const TArray<FGroupInfo>& MeshGroupInfos = StaticMesh->GetMeshGroupInfo();

	// 처리할 섹션 수 결정
	const bool bHasSections = !MeshGroupInfos.IsEmpty();
	const uint32 NumSectionsToProcess = bHasSections ? static_cast<uint32>(MeshGroupInfos.size()) : 1;

	// 단일 루프로 통합
	for (uint32 SectionIndex = 0; SectionIndex < NumSectionsToProcess; ++SectionIndex)
	{
		uint32 IndexCount = 0;
		uint32 StartIndex = 0;

		// 1. 섹션 정보 유무에 따라 드로우 데이터 범위를 결정합니다.
		if (bHasSections)
		{
			// [서브 메시 처리]
			const FGroupInfo& Group = MeshGroupInfos[SectionIndex];
			IndexCount = Group.IndexCount;
			StartIndex = Group.StartIndex;
		}
		else
		{
			// [단일 배치 처리]
			// bHasSections가 false이면 이 루프는 SectionIndex가 0일 때 한 번만 실행됩니다.
			IndexCount = StaticMesh->GetIndexCount();
			StartIndex = 0;
		}

		// 2. (공통) 인덱스 수가 0이면 스킵
		if (IndexCount == 0)
		{
			continue;
		}

		// 3. (공통) FMeshBatchElement 생성 및 설정
		// 머티리얼과 셰이더는 루프 밖에서 이미 결정되었습니다.
		FMeshBatchElement BatchElement;

		// --- 정렬 키 ---
		BatchElement.VertexShader = ShaderToUse;
		BatchElement.PixelShader = ShaderToUse;
		BatchElement.Material = MaterialToUse;
		BatchElement.VertexBuffer = StaticMesh->GetVertexBuffer();
		BatchElement.IndexBuffer = StaticMesh->GetIndexBuffer();
		BatchElement.VertexStride = StaticMesh->GetVertexStride();

		// --- 드로우 데이터 (1번에서 결정된 값 사용) ---
		BatchElement.IndexCount = IndexCount;
		BatchElement.StartIndex = StartIndex;
		BatchElement.BaseVertexIndex = 0;

		// --- 인스턴스 데이터 ---
		BatchElement.WorldMatrix = GetWorldMatrix();
		BatchElement.ObjectID = 0; // 기즈모는 피킹 대상이 아니므로 0
		BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		if (bHighlighted)
		{
			BatchElement.InstanceColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);
		}
		else
		{
			BatchElement.InstanceColor = FLinearColor(GetColor());
		}

		OutMeshBatchElements.Add(BatchElement);
	}
}

void UGizmoArrowComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

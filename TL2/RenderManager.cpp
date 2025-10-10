#include "pch.h"
#include "RenderManager.h"
#include "WorldPartitionManager.h"
#include "World.h"
#include "Renderer.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "PrimitiveComponent.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "TextRenderComponent.h"
#include "GizmoActor.h"
#include "GridActor.h"
#include "Octree.h"
#include "BVHierachy.h"
#include "Occlusion.h"
#include "Frustum.h"
#include "AABoundingBoxComponent.h"
#include "ResourceManager.h"
#include "RHIDevice.h"
#include "StaticMesh.h"
#include "Material.h"
#include "Texture.h"
#include "RenderSettings.h"
#include "EditorEngine.h"
#include "DecalComponent.h"

URenderManager::URenderManager()
	: OcclusionCPU(new FOcclusionCullingManagerCPU())
{
}

URenderManager::~URenderManager()
{
}

void URenderManager::BeginFrame()
{
    if (Renderer)
        Renderer->BeginFrame();
}

void URenderManager::EndFrame()
{
    if (Renderer)
        Renderer->EndFrame();
}

void URenderManager::Render(UWorld* InWorld, FViewport* Viewport)
{
    if (!Viewport) return;
    if (InWorld) World = InWorld; // update current world

	//엔진 접근 수정
	if (!Renderer)
	{
		Renderer = GEngine.GetRenderer();
	}

    FViewportClient* Client = Viewport->GetViewportClient();
    if (!Client) return;

    ACameraActor* Camera = Client->GetCamera();
    if (!Camera) return;

	//기즈모 카메라 설정
	if(World->GetGizmoActor())
		World->GetGizmoActor()->SetCameraActor(Camera);
	
	World->GetRenderSettings().SetViewModeIndex(Client->GetViewModeIndex());
    RenderViewports(Camera, Viewport);
}

void URenderManager::RenderViewports(ACameraActor* Camera, FViewport* Viewport)
{
	if (!World || !Renderer || !Camera || !Viewport) return;

	int objCount = static_cast<int>(World->GetActors().size());
	int visibleCount = 0;
	float zNear = 0.1f, zFar = 100.f;
	// 뷰포트의 실제 크기로 aspect ratio 계산
	float ViewportAspectRatio = static_cast<float>(Viewport->GetSizeX()) / static_cast<float>(Viewport->GetSizeY());
	if (Viewport->GetSizeY() == 0) ViewportAspectRatio = 1.0f; // 0으로 나누기 방지

	// Provide per-viewport size to renderer (used by overlay/gizmo scaling)
	Renderer->SetCurrentViewportSize(Viewport->GetSizeX(), Viewport->GetSizeY());

	FMatrix ViewMatrix = Camera->GetViewMatrix();
	FMatrix ProjectionMatrix = Camera->GetProjectionMatrix(ViewportAspectRatio, Viewport);

	Frustum ViewFrustum;
	UCameraComponent* CamComp = nullptr;
	if (CamComp = Camera->GetCameraComponent())
	{
		ViewFrustum = CreateFrustumFromCamera(*CamComp, ViewportAspectRatio);
		zNear = CamComp->GetNearClip();
		zFar = CamComp->GetFarClip();
	}
	FVector rgb(1.0f, 1.0f, 1.0f);

	// === Begin Line Batch for all actors ===
	Renderer->BeginLineBatch();

	// === Draw Actors with Show Flag checks ===
	// Compute effective view mode with ShowFlag override for wireframe
	EViewModeIndex EffectiveViewMode = World->GetRenderSettings().GetViewModeIndex();
	if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Wireframe))
	{
		EffectiveViewMode = EViewModeIndex::VMI_Wireframe;
	}
	Renderer->SetViewModeType(EffectiveViewMode);

	// ============ Culling Logic Dispatch ========= //
	//for (AActor* Actor : World->GetActors())
	//	Actor->SetCulled(true);
	//if (World->GetPartitionManager())
	//	World->GetPartitionManager()->FrustumQuery(ViewFrustum);

	Renderer->UpdateHighLightConstantBuffer(false, rgb, 0, 0, 0, 0);

	// ---------------------- CPU HZB Occlusion ----------------------
	if (bUseCPUOcclusion)
	{
		// 1) 그리드 사이즈 보정(해상도 변화 대응)
		UpdateOcclusionGridSizeForViewport(Viewport);

		// 2) 오클루더/오클루디 수집
		TArray<FCandidateDrawable> Occluders, Occludees;
		BuildCpuOcclusionSets(ViewFrustum, ViewMatrix, ProjectionMatrix, zNear, zFar,
			Occluders, Occludees);

		// 3) 오클루더로 저해상도 깊이 빌드 + HZB
		OcclusionCPU->BuildOccluderDepth(Occluders, Viewport->GetSizeX(), Viewport->GetSizeY());
		OcclusionCPU->BuildHZB();

		// 4) 가시성 판정 → VisibleFlags[UUID] = 0/1
		//     VisibleFlags 크기 보장
		uint32_t maxUUID = 0;
		for (auto& C : Occludees) maxUUID = std::max(maxUUID, C.ActorIndex);
		if (VisibleFlags.size() <= size_t(maxUUID))
			VisibleFlags.assign(size_t(maxUUID + 1), 1); // 기본 보임

		OcclusionCPU->TestOcclusion(Occludees, Viewport->GetSizeX(), Viewport->GetSizeY(), VisibleFlags);
	}
	// ----------------------------------------------------------------

	{	// 일반 액터들 렌더링

		// 렌더 목록 수집
		FVisibleRenderProxySet RenderProxySet;

		if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Primitives))
		{
			for (AActor* Actor : World->GetActors())
			{
				if (!Actor) continue;
				if (Actor->GetActorHiddenInGame()) continue;
				if (Actor->GetCulled()) continue; // 컬링된 액터는 스킵

				// ★★★ CPU 오클루전 컬링: UUID로 보임 여부 확인
				if (bUseCPUOcclusion)
				{
					uint32_t id = Actor->UUID;
					if (id < VisibleFlags.size() && VisibleFlags[id] == 0)
					{
						continue; // 가려짐 → 스킵c
					}
				}

				if (Cast<AStaticMeshActor>(Actor) && !World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_StaticMeshes))
				{
					continue;
				}

				for (USceneComponent* Component : Actor->GetSceneComponents())
				{
					if (!Component)
					{
						continue;
					}
					if (UActorComponent* ActorComp = Cast<UActorComponent>(Component))
					{
						if (!ActorComp->IsActive())
						{
							continue;
						}

						if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
						{
							RenderProxySet.Primitives.Add(Primitive);
						}
						else if (UDecalComponent* Decal = Cast<UDecalComponent>(Component))
						{
							RenderProxySet.Decals.Add(Decal);
						}
					}
				}
			}

			Renderer->SetViewModeType(EffectiveViewMode);
			Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqual);

			// Primitives 그리기
			for (UPrimitiveComponent* Primitive : RenderProxySet.Primitives)
			{
				Primitive->Render(Renderer, ViewMatrix, ProjectionMatrix);

				visibleCount++;
			}

			// NOTE: 디버깅용 임시 Decals 외곽선 그리기
			for (UDecalComponent* Decal : RenderProxySet.Decals)
			{
				Decal->RenderDebugVolume(Renderer, ViewMatrix, ProjectionMatrix);
			}

			Renderer->SetViewModeType(EffectiveViewMode);
			Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly);
			Renderer->OMSetBlendState(true);

			// Decal 그리기
			if (UWorldPartitionManager* Partition = World->GetPartitionManager())
			{
				if (const FBVHierachy* BVH = Partition->GetBVH())
				{
					for (UDecalComponent* Decal : RenderProxySet.Decals)
					{
						// Decal이 그려질 Primitives
						TArray<UPrimitiveComponent*> TargetPrimitives;

						// 1. Decal의 World AABB와 충돌한 모든 Actor 쿼리
						TArray<AActor*> IntersectedActors = BVH->QueryIntersectedActors(Decal->GetWorldAABB());

						// 2. 충돌한 모든 visible Actor의 PrimitiveComponent를 TargetPrimitives에 추가 
						for (AActor* Actor : IntersectedActors)
						{
							if (!Actor->IsActorVisible())
								continue; // Skip hidden actor
							TArray<USceneComponent*> SceneComponents = Actor->GetSceneComponents();
							for (USceneComponent* SceneComp : SceneComponents)
							{
								UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(SceneComp);
								if (Primitive)
								{
									TargetPrimitives.push_back(Primitive);
								}
							}
						}

						// 3. TargetPrimitive 순회하며 렌더링
						for (UPrimitiveComponent* Target : TargetPrimitives)
						{
							Decal->RenderAffectedPrimitives(Renderer, Target, ViewMatrix, ProjectionMatrix);
						}

						visibleCount++;
					}
				}
			}
		}
	}

	// 엔진 액터들 (그리드 등)
	for (AActor* EngineActor : World->GetEditorActors())
	{
		if (!EngineActor || EngineActor->GetActorHiddenInGame())
			continue;

		if (Cast<AGridActor>(EngineActor) && !World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Grid))
			continue;

		for (USceneComponent* Component : EngineActor->GetSceneComponents())
		{
			if (!Component || !Component->IsActive())
				continue;
			if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			{
				Renderer->SetViewModeType(EffectiveViewMode);
				Primitive->Render(Renderer, ViewMatrix, ProjectionMatrix);
				Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqual);
			}
		}
		Renderer->OMSetBlendState(false);
	}

	// Debug draw (exclusive: BVH first, else Octree)
	if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_BVHDebug) &&
		World->GetPartitionManager())
	{
		if (FBVHierachy* BVH = World->GetPartitionManager()->GetBVH())
		{
			BVH->DebugDraw(Renderer);
		}
	}
	else if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_OctreeDebug) &&
		World->GetPartitionManager())
	{
		if (FOctree* Octree = World->GetPartitionManager()->GetSceneOctree())
		{
			Octree->DebugDraw(Renderer);
		}
	}
	Renderer->EndLineBatch(FMatrix::Identity(), ViewMatrix, ProjectionMatrix);

	Renderer->UpdateHighLightConstantBuffer(false, rgb, 0, 0, 0, 0);
	if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Culling))
	{
		UE_LOG("Obj count: %d, Visible count: %d\r\n", objCount, visibleCount);
	}
}

void URenderManager::UpdateOcclusionGridSizeForViewport(FViewport* Viewport)
{
    if (!Viewport) return;
    int vw = (1 > Viewport->GetSizeX()) ? 1 : Viewport->GetSizeX();
    int vh = (1 > Viewport->GetSizeY()) ? 1 : Viewport->GetSizeY();
    int gw = std::max(1, vw / std::max(1, OcclGridDiv));
    int gh = std::max(1, vh / std::max(1, OcclGridDiv));
    // 매 프레임 호출해도 싸다. 내부에서 동일크기면 skip
    OcclusionCPU->Initialize(gw, gh);
}

void URenderManager::BuildCpuOcclusionSets(
    const Frustum& ViewFrustum,
    const FMatrix& View,
    const FMatrix& Proj,
    float ZNear,
    float ZFar,
    TArray<FCandidateDrawable>& OutOccluders,
    TArray<FCandidateDrawable>& OutOccludees)
{
    OutOccluders.clear();
    OutOccludees.clear();

    size_t estimatedCount = 0;
    for (AActor* Actor : World->GetActors())
    {
        if (Actor && !Actor->GetActorHiddenInGame() && !Actor->GetCulled())
        {
            if (Actor->IsA<AStaticMeshActor>()) estimatedCount++;
        }
    }
    OutOccluders.reserve(estimatedCount);
    OutOccludees.reserve(estimatedCount);

    const FMatrix VP = View * Proj; // 행벡터: p_world * View * Proj

    for (AActor* Actor : World->GetActors())
    {
        if (!Actor) continue;
        if (Actor->GetActorHiddenInGame()) continue;
        if (Actor->GetCulled()) continue;

        AStaticMeshActor* SMA = Cast<AStaticMeshActor>(Actor);
        if (!SMA) continue;

        UAABoundingBoxComponent* Box = Cast<UAABoundingBoxComponent>(SMA->CollisionComponent);
        if (!Box) continue;

        OutOccluders.emplace_back();
        FCandidateDrawable& occluder = OutOccluders.back();
        occluder.ActorIndex = Actor->UUID;
        occluder.Bound = Box->GetWorldBound();
        occluder.WorldViewProj = VP;
        occluder.WorldView = View;
        occluder.ZNear = ZNear;
        occluder.ZFar = ZFar;

        OutOccludees.emplace_back(occluder);
    }
}
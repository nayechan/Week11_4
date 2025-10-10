#pragma once
#include "Object.h"

class UWorld;
class URenderer;
class ACameraActor;
class FViewport;
class FViewportClient;
class UPrimitiveComponent;
class UDecalComponent;

struct FCandidateDrawable;

// 렌더링할 대상들의 집합을 담는 구조체
struct FVisibleRenderProxySet
{
    TArray<UPrimitiveComponent*> Primitives;
    TArray<UDecalComponent*> Decals;

    // TArray<ULightComponent*> Lights; // 나중에 조명 등 다른 요소도 추가 가능
};

// High-level scene rendering orchestrator extracted from UWorld
class URenderManager : public UObject
{
public:
    DECLARE_CLASS(URenderManager, UObject)

    URenderManager();

    // Singleton accessor
    static URenderManager& GetInstance()
    {
        static URenderManager* Instance = nullptr;
        if (!Instance) Instance = NewObject<URenderManager>();
        return *Instance;
    }

    // Render using camera derived from the viewport's client
    void Render(UWorld* InWorld, FViewport* Viewport);

    // Low-level: Renders with explicit camera
    void RenderViewports(ACameraActor* Camera, FViewport* Viewport);

    // Optional frame hooks if you want to move frame begin/end here later
    void BeginFrame();
    void EndFrame();

    URenderer* GetRenderer() const { return Renderer; }

private:
    UWorld* World = nullptr;
    URenderer* Renderer = nullptr;

    ~URenderManager() override;

private:
    // ==================== CPU HZB Occlusion ====================
    void UpdateOcclusionGridSizeForViewport(FViewport* Viewport);
    void BuildCpuOcclusionSets(
        const Frustum& ViewFrustum,
        const FMatrix& View, const FMatrix& Proj,
        float ZNear, float ZFar,                       // ★ 추가
        TArray<FCandidateDrawable>& OutOccluders,
        TArray<FCandidateDrawable>& OutOccludees);

    std::unique_ptr<FOcclusionCullingManagerCPU> OcclusionCPU = nullptr;
    TArray<uint8_t>        VisibleFlags;   // ActorIndex(UUID)로 인덱싱 (0=가려짐, 1=보임)
    bool                        bUseCPUOcclusion = false; // False 하면 오클루전 컬링 안씁니다.
    int                         OcclGridDiv = 2; // 화면 크기/이 값 = 오클루전 그리드 해상도(1/6 권장)


};

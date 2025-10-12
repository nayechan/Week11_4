#pragma once
#include "Frustum.h"

// 전방 선언 (헤더 파일 의존성 최소화)
class UWorld;
class ACameraActor;
class FViewport;
class URenderer;
class D3D11RHI;
class UPrimitiveComponent;
class UDecalComponent;

struct FCandidateDrawable;

// 렌더링할 대상들의 집합을 담는 구조체
struct FVisibleRenderProxySet
{
	TArray<UPrimitiveComponent*> Primitives;
	TArray<UDecalComponent*> Decals;
};

/**
 * @class FSceneRenderer
 * @brief 한 프레임의 특정 뷰(View)에 대한 씬 렌더링을 총괄하는 임시(transient) 클래스.
 */
class FSceneRenderer
{
public:
	FSceneRenderer(UWorld* InWorld, ACameraActor* InCamera, FViewport* InViewport, URenderer* InOwnerRenderer);
	~FSceneRenderer();

	/** @brief 이 씬 렌더러의 모든 렌더링 파이프라인을 실행합니다. */
	void Render();

private:
	/** @brief 렌더링에 필요한 포인터들이 유효한지 확인합니다. */
	bool IsValid() const;

	/** @brief 렌더링에 필요한 뷰 행렬, 절두체 등 프레임 데이터를 준비합니다. */
	void PrepareView();

	/** @brief 월드의 모든 액터를 대상으로 절두체 컬링을 수행합니다. */
	void PerformFrustumCulling();

	/** @brief CPU 기반 오클루전 컬링을 수행하여 가시성 플래그를 계산합니다. */
	void PerformCPUOcclusion();

	/** @brief 씬을 순회하며 컬링을 통과한 모든 렌더링 대상을 수집합니다. */
	void GatherVisibleProxies();

	/** @brief 불투명(Opaque) 객체들을 렌더링하는 패스입니다. */
	void RenderOpaquePass();

	/** @brief 데칼(Decal)을 렌더링하는 패스입니다. */
	void RenderDecalPass();

	/** @brief 그리드 등 에디터 전용 객체들을 렌더링하는 패스입니다. */
	void RenderEditorPrimitivesPass();

	/** @brief BVH 등 디버그 시각화 요소를 렌더링하는 패스입니다. */
	void RenderDebugPass();

	/** @brief 프레임 렌더링의 마무리 작업을 수행합니다. (예: 로그 출력) */
	void FinalizeFrame();

	///** @brief 오클루전 컬링을 위한 오클루더/오클루디 목록을 생성합니다. */
	//void BuildCpuOcclusionSets(const Frustum& InViewFrustum, const FMatrix& InView, const FMatrix& InProj, float InZNear, float InZFar, TArray<FCandidateDrawable>& OutOccluders, TArray<FCandidateDrawable>& OutOccludees);
	//void UpdateOcclusionGridSizeForViewport(FViewport* InViewport);

private:
	// --- 렌더링 컨텍스트 (외부에서 주입받음) ---
	UWorld* World;
	ACameraActor* Camera;
	FViewport* Viewport;
	URenderer* OwnerRenderer;
	D3D11RHI* RHI;

	// --- 프레임 동안 계산되고 사용되는 데이터 ---
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;
	Frustum ViewFrustum;
	float ZNear = 0.1f;
	float ZFar = 1000.0f;
	EViewModeIndex EffectiveViewMode = EViewModeIndex::None;

	// 수집된 렌더링 대상 목록
	FVisibleRenderProxySet Proxies;

	// 컬링을 거친 가시성 목록, NOTE: 추후 컴포넌트 단위로 수정
	TArray<UPrimitiveComponent*> PotentiallyVisibleComponents;

	//// --- 오클루전 컬링 시스템 ---
	//std::unique_ptr<FOcclusionCullingManagerCPU> OcclusionCPU;
	//TArray<uint8_t> VisibleFlags;
	//bool bUseCPUOcclusion = false;
	//int OcclGridDiv = 2;
};
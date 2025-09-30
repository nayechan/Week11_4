#pragma once
#include "Object.h"
#include "Enums.h"

// Forward Declarations
class UResourceManager;
class UUIManager;
class UInputManager;
class USelectionManager;
class AActor;
class URenderer;
class ACameraActor;
class AGizmoActor;
class AGridActor;
class FViewport;
class SMultiViewportWindow;
struct FTransform;
struct FPrimitiveData;
class SViewportWindow;
class UWorldPartitionManager;
class AStaticMeshActor;
class BVHierachy;
class UStaticMesh;

class FOcclusionCullingManagerCPU;
class Frustum;
struct FCandidateDrawable;

class UWorld final : public UObject
{
public:
    DECLARE_CLASS(UWorld, UObject)
    UWorld();
    ~UWorld() override;
    static UWorld& GetInstance();

protected:


public:
    /** 초기화 */
    void Initialize();
    void InitializeMainCamera();
    void InitializeGrid();
    void InitializeGizmo();

    // 액터 인터페이스 관리
    void SetupActorReferences();

    // 선택 및 피킹 처리
    void ProcessActorSelection();

    void ProcessViewportInput();

    void SetRenderer(URenderer* InRenderer);
    URenderer* GetRenderer() { return Renderer; }

    void SetMainViewport(SViewportWindow* InViewport) { MainViewport = InViewport; }
    SViewportWindow* GetMainViewport() const { return MainViewport; }

    void SetMultiViewportWindow(SMultiViewportWindow* InMultiViewport) { MultiViewport = InMultiViewport; }
    SMultiViewportWindow* GetMultiViewportWindow() const { return MultiViewport; }

    template<class T>
    T* SpawnActor();

    template<class T>
    T* SpawnActor(const FTransform& Transform);

    bool DestroyActor(AActor* Actor);

    // Partial hooks
    void OnActorSpawned(AActor* Actor);
    void OnActorDestroyed(AActor* Actor);

    void CreateNewScene();
    void LoadScene(const FString& SceneName);
    void SaveScene(const FString& SceneName);
    ACameraActor* GetCameraActor() { return MainCameraActor; }

    EViewModeIndex GetViewModeIndex() { return ViewModeIndex; }
    void SetViewModeIndex(EViewModeIndex InViewModeIndex) { ViewModeIndex = InViewModeIndex; }

    /** === Show Flag 시스템 === */
    EEngineShowFlags GetShowFlags() const { return ShowFlags; }
    void SetShowFlags(EEngineShowFlags InShowFlags) { ShowFlags = InShowFlags; }
    void EnableShowFlag(EEngineShowFlags Flag) { ShowFlags |= Flag; }
    void DisableShowFlag(EEngineShowFlags Flag) { ShowFlags &= ~Flag; }
    void ToggleShowFlag(EEngineShowFlags Flag) { ShowFlags = HasShowFlag(ShowFlags, Flag) ? (ShowFlags & ~Flag) : (ShowFlags | Flag); }
    bool IsShowFlagEnabled(EEngineShowFlags Flag) const { return HasShowFlag(ShowFlags, Flag); }

    /** Generate unique name for actor based on type */
    FString GenerateUniqueActorName(const FString& ActorType);

    /** === 타임 / 틱 === */
    virtual void Tick(float DeltaSeconds);
    float GetTimeSeconds() const;

    /** === 렌더 === */
    void Render();
    void RenderViewports(ACameraActor* Camera, FViewport* Viewport);
    //void GameRender(ACameraActor* Camera, FViewport* Viewport);

    // Partition manager
    //UWorldPartitionManager* GetPartitionManager() const { return PartitionManager; }


    /** === 필요한 엑터 게터 === */
    const TArray<AActor*>& GetActors() { return Actors; }
    AGizmoActor* GetGizmoActor();
    AGridActor* GetGridActor() { return GridActor; }

    void PushBackToStaticMeshActors(AStaticMeshActor* InStaticMeshActor);
    void SetStaticMeshs();

    /** === 레벨 / 월드 구성 === */
    // TArray<ULevel*> Levels;

    /** === 플레이어 / 컨트롤러 === */
    // APlayerController* GetFirstPlayerController() const;
    // TArray<APlayerController*> GetPlayerControllerIterator() const;
private:
    // 렌더러 (월드가 소유)
    URenderer* Renderer = nullptr;

    // 메인 뷰포트
    SViewportWindow* MainViewport = nullptr;
    // 멀티 뷰포트 윈도우
    SMultiViewportWindow* MultiViewport = nullptr;

    /** === 액터 관리 === */
    TArray<AActor*> EngineActors;
    ACameraActor* MainCameraActor = nullptr;
    AGridActor* GridActor = nullptr;
    AGizmoActor* GizmoActor = nullptr;

    /** === 액터 관리 === */
    TArray<AActor*> Actors;
    TArray<FPrimitiveData> Primitives;

    /** A dedicated array for static mesh actors to optimize culling. */
    TArray<class AStaticMeshActor*> StaticMeshActors;
    TArray<UStaticMesh*> StaticMeshs;

    // Object naming system
    TMap<FString, int32> ObjectTypeCounts;

    /** === Show Flag 시스템 === */
    EEngineShowFlags ShowFlags = EEngineShowFlags::SF_DefaultEnabled;

    EViewModeIndex ViewModeIndex = EViewModeIndex::VMI_Unlit;

    // ==================== CPU HZB Occlusion ====================
    FOcclusionCullingManagerCPU* OcclusionCPU = nullptr;
    TArray<uint8_t>        VisibleFlags;   // ActorIndex(UUID)로 인덱싱 (0=가려짐, 1=보임)
    bool                        bUseCPUOcclusion = true; // False 하면 오클루전 컬링 안씁니다.
    int                         OcclGridDiv = 2; // 화면 크기/이 값 = 오클루전 그리드 해상도(1/6 권장)

    // 헬퍼들
    void UpdateOcclusionGridSizeForViewport(FViewport* Viewport);
    void BuildCpuOcclusionSets(
        const Frustum& ViewFrustum,
        const FMatrix& View, const FMatrix& Proj,
        float ZNear, float ZFar,                       // ★ 추가
        TArray<FCandidateDrawable>& OutOccluders,
        TArray<FCandidateDrawable>& OutOccludees);

};
template<class T>
inline T* UWorld::SpawnActor()
{
    return SpawnActor<T>(FTransform());
}

template<class T>
inline T* UWorld::SpawnActor(const FTransform& Transform)
{
    static_assert(std::is_base_of<AActor, T>::value, "T must be derived from AActor");

    // 새 액터 생성
    T* NewActor = NewObject<T>();

    // 초기 트랜스폼 적용
    NewActor->SetActorTransform(Transform);

    //  월드 등록
    NewActor->SetWorld(this);

    // 월드에 등록
    Actors.Add(NewActor);

    return NewActor;
}

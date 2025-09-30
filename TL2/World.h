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
class USlateManager;
class URenderManager;
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

public:
    /** 초기화 */
    void Initialize();
    void InitializeGrid();
    void InitializeGizmo();

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
    void SetCameraActor(ACameraActor* InCameraActor);
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

    /** === 필요한 엑터 게터 === */
    const TArray<AActor*>& GetActors() { return Actors; }
    const TArray<AActor*>& GetEditorActors() { return EditorActors; }
    AGizmoActor* GetGizmoActor();
    AGridActor* GetGridActor() { return GridActor; }

    void SetStaticMeshs();
    const TArray<UStaticMesh*>& GetStaticMeshs() { return StaticMeshs; }
    
    /** === 레벨 / 월드 구성 === */
    // TArray<ULevel*> Levels;
private:
    /** === 액터 관리 === */
    TArray<AActor*> EditorActors;
    ACameraActor* MainCameraActor = nullptr;
    AGridActor* GridActor = nullptr;
    AGizmoActor* GizmoActor = nullptr;

    /** === 액터 관리 === */
    TArray<AActor*> Actors;
    TArray<FPrimitiveData> Primitives;

    /** A dedicated array for static mesh actors to optimize culling. */
    TArray<UStaticMesh*> StaticMeshs;

    // Object naming system
    TMap<FString, int32> ObjectTypeCounts;

    /** === Show Flag 시스템 === */
    EEngineShowFlags ShowFlags = EEngineShowFlags::SF_DefaultEnabled;

    EViewModeIndex ViewModeIndex = EViewModeIndex::VMI_Unlit;
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

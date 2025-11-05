#pragma once
#include "Object.h"
#include "Enums.h"
#include "RenderSettings.h"
#include "Level.h"
#include "Gizmo/GizmoActor.h"
#include "LightManager.h"

// Forward Declarations
class UResourceManager;
class UUIManager;
class UInputManager;
class USelectionManager;
class FLuaManager;
class AActor;
class URenderer;
class ACameraActor;
class AGizmoActor;
class AGridActor;
class FViewport;
class USlateManager;
class URenderManager;
class SViewportWindow;
class UWorldPartitionManager;
class AStaticMeshActor;
class BVHierachy;
class UStaticMesh;
class FOcclusionCullingManagerCPU;
class APlayerCameraManager;

struct FTransform;
struct FSceneCompData;
struct Frustum;
struct FCandidateDrawable;


class UWorld final : public UObject
{
public:
    DECLARE_CLASS(UWorld, UObject)
    UWorld();
    ~UWorld() override;

    bool bPie = false;
public:
    /** 초기화 */
    void Initialize();
    void InitializeGrid();
    void InitializeGizmo();

    bool TryLoadLastUsedLevel();

    template<class T>
    T* SpawnActor();

    template<class T>
    T* SpawnActor(const FTransform& Transform);

    template<typename T>
    T* FindActor();

    template<typename T>
    TArray<T*> FindActors();

    AActor* SpawnActor(UClass* Class, const FTransform& Transform);
    AActor* SpawnActor(UClass* Class);
    AActor* SpawnPrefabActor(const FWideString& PrefabPath);
    void AddActorToLevel(AActor* Actor);

    void AddPendingKillActor(AActor* Actor);
    void ProcessPendingKillActors();

    void CreateLevel();

    void SpawnDefaultActors();

    // Level ownership API
    void SetLevel(std::unique_ptr<ULevel> InLevel);
    ULevel* GetLevel() const { return Level.get(); }
    FLightManager* GetLightManager() const { return LightManager.get(); }
    FLuaManager* GetLuaManager() const { return LuaManager.get(); }

    ACameraActor* GetCameraActor() { return MainCameraActor; }
    void SetCameraActor(ACameraActor* InCamera);

    /** Generate unique name for actor based on type */
    FString GenerateUniqueActorName(const FString& ActorType);

    /** === 타임 / 틱 === */
    
    virtual void Tick(float DeltaSeconds);
    // Overlap pair de-duplication (per-frame)
    bool TryMarkOverlapPair(const AActor* A, const AActor* B);


    /** === 필요한 엑터 게터 === */
    const TArray<AActor*>& GetActors() { static TArray<AActor*> Empty; return Level ? Level->GetActors() : Empty; }
    const TArray<AActor*>& GetEditorActors() { return EditorActors; }
    AGizmoActor* GetGizmoActor() { return GizmoActor; }
    AGridActor* GetGridActor() { return GridActor; }
    UWorldPartitionManager* GetPartitionManager() { return Partition.get(); }

    // Per-world render settings
    URenderSettings& GetRenderSettings() { return RenderSettings; }
    const URenderSettings& GetRenderSettings() const { return RenderSettings; }

    // Per-world SelectionManager accessor
    USelectionManager* GetSelectionManager() { return SelectionMgr.get(); }

    void SetFirstPlayerCameraManager(APlayerCameraManager* InPlayerCameraManager) {  PlayerCameraManager = InPlayerCameraManager; };
    APlayerCameraManager* GetFirstPlayerCameraManager() { return PlayerCameraManager; };

    // PIE용 World 생성
    static UWorld* DuplicateWorldForPIE(UWorld* InEditorWorld);

private:
    bool DestroyActor(AActor* Actor);   // 즉시 삭제

private:
    /** === 에디터 특수 액터 관리 === */
    TArray<AActor*> EditorActors;
    ACameraActor* MainCameraActor = nullptr;
    AGridActor* GridActor = nullptr;
    AGizmoActor* GizmoActor = nullptr;
    APlayerCameraManager* PlayerCameraManager;

    /** === 레벨 컨테이너 === */
    std::unique_ptr<ULevel> Level;
    TArray<AActor*> PendingKillActors;  // 지연 삭제 예정 액터 목록

    /** === 라이트 매니저 ===*/
    std::unique_ptr<FLightManager> LightManager;

    /** === 루아 매니저 ===*/
    std::unique_ptr<FLuaManager> LuaManager;
    
    // Object naming system
    TMap<FString, int32> ObjectTypeCounts;


    // Per-world render settings
    URenderSettings RenderSettings;

    //partition
    std::unique_ptr<UWorldPartitionManager> Partition = nullptr;

    // Per-world selection manager
    std::unique_ptr<USelectionManager> SelectionMgr;

    // Per-frame processed overlap pairs (A,B) keyed canonically
    TSet<uint64> FrameOverlapPairs;
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
    AddActorToLevel(NewActor);

    return NewActor;
}

// 월드에서 특정 클래스(T)의 '첫 번째' 액터를 찾아 반환합니다. 없으면 nullptr.
template<typename T>
T* UWorld::FindActor()
{
    // T::StaticClass()를 통해 템플릿 타입의 UClass를 가져옵니다.
    UClass* TypeClass = T::StaticClass();
    if (!TypeClass)
    {
        return nullptr;
    }

    for (AActor* Actor : Level->GetActors())
    {
        if (Actor && !Actor->IsPendingDestroy() &&Actor->IsA(TypeClass))
        {
            // 찾은 액터를 T* 타입으로 캐스팅하여 반환합니다.
            // (IsA를 통과했으므로 Cast는 안전함)
            return Cast<T>(Actor);
        }
    }
    return nullptr;
}

template<typename T>
TArray<T*> UWorld::FindActors()
{
    TArray<T*> FoundActors;

    // T::StaticClass()를 통해 템플릿 타입의 UClass를 가져옵니다.
    UClass* TypeClass = T::StaticClass();
    if (!TypeClass)
    {
        return FoundActors; // 빈 배열 반환
    }

    for (AActor* Actor : Level->GetActors())
    {
        if (Actor && !Actor->IsPendingDestroy() && Actor->IsA(TypeClass))
        {
            // 일치하는 모든 액터를 캐스팅하여 배열에 추가
            FoundActors.Add(Cast<T>(Actor));
        }
    }
    return FoundActors;
}
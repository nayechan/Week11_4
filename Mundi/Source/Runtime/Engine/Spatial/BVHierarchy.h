#pragma once

struct FFrustum;
struct FRay; // forward declaration for ray type
class UPrimitiveComponent;
class AActor;
struct FOBB;
struct FBoundingSphere;

/**
 * @brief Broad phase BVH based on UPrimitiveComponent
 */
class FBVHierarchy
{
    /**
     * *주의사항*
     * AActor 기반에서 UPrimitiveComponent 기반으로 변경을 거친 BVH입니다.
     * - AActor를 기준으로 만들어졌다가 재활용된 로직에 의한 혼동 및 버그 주의.
     * - 일반적인 USceneComponent에 대해서는 호환성 없음.
     */
public:
    // 생성자/소멸자
    FBVHierarchy(const FAABB& InBounds, int InDepth = 0, int InMaxDepth = 12, int InMaxObjects = 8);
    ~FBVHierarchy();

    // 초기화
    void Clear();

    void BulkUpdate(const TArray<UPrimitiveComponent*>& Components);
    void Update(UPrimitiveComponent* InComponent);
    void Remove(UPrimitiveComponent* InComponent);

    void FlushRebuild();

    void QueryRayClosestStrict(const FRay& Ray, AActor*& OutActor, OUT float& OutBestT, TArray<AActor*> ExcludeList = {}) const;
    void QueryRayClosest(const FRay& Ray, AActor*& OutActor, OUT float& OutBestT) const;
    void QueryFrustum(const FFrustum& InFrustum);
    TArray<UPrimitiveComponent*> QueryIntersectedComponents(const FAABB& InBound) const;
    TArray<UPrimitiveComponent*> QueryIntersectedComponents(const FOBB& InBound) const;
    TArray<UPrimitiveComponent*> QueryIntersectedComponents(const FBoundingSphere& InBound) const;

    void DebugDraw(URenderer* Renderer) const;

    // Debug/Stats
    int TotalNodeCount() const;
    int TotalActorCount() const; 
    int MaxOccupiedDepth() const;
    void DebugDump() const;
    const FAABB& GetBounds() const { return Bounds; }

    // 프러스텀 기준으로 오클루더(내부노드 AABB) / 오클루디(리프의 액터들) 수집
    // VP는 행벡터 기준(네 컨벤션): p' = p * VP

private:
    // === LBVH data ===
    struct FLBVHNode
    {
        FAABB Bounds;
        int32 Left = -1;
        int32 Right = -1;
        int32 First = -1;
        int32 Count = 0;
        bool IsLeaf() const { return Count > 0; }
    };
    void BuildLBVH();

private:
    template<typename BoundType, typename NodeIntersectFunc, typename ComponentIntersectFunc>
    TArray<UPrimitiveComponent*> QueryIntersectedComponentsGeneric(const BoundType& InBound
        , NodeIntersectFunc NodeIntersects
        , ComponentIntersectFunc ComponentIntersects) const;

    int BuildRange(int s, int e);

    int Depth;
    int MaxDepth;
    int MaxObjects;
    FAABB Bounds;

    TMap<UPrimitiveComponent*, FAABB> PrimitiveComponentBounds;
    TArray<UPrimitiveComponent*> PrimitiveComponentArray;

    // LBVH nodes
    TArray<FLBVHNode> Nodes;

    bool bPendingRebuild = false;
};

#pragma once

#include <vector>

struct Frustum;
struct FRay; // forward declaration for ray type

class FBVHierachy
{
public:
    // 생성자/소멸자
    FBVHierachy(const FBound& InBounds, int InDepth = 0, int InMaxDepth = 12, int InMaxObjects = 8);
    ~FBVHierachy();

    // 초기화
    void Clear();

    // 삽입 / 제거 / 갱신
    void Insert(AActor* InActor, const FBound& ActorBounds);
    void BulkInsert(const TArray<std::pair<AActor*, FBound>>& ActorsAndBounds);
    bool Remove(AActor* InActor, const FBound& ActorBounds);
    void Update(AActor* InActor, const FBound& OldBounds, const FBound& NewBounds);

    bool Contains(const FBound& Box) const;

    // Partition Manager Interface
    void Remove(AActor* InActor);
    void Update(AActor* InActor);

    void FlushRebuild();

    void QueryRay(const FRay& InRay, OUT TArray<AActor*>& Actors);
    void QueryRayOrdered(const FRay& InRay, OUT TArray<std::pair<AActor*, float>>& OutCandidates) const;
    void QueryFrustum(const Frustum& InFrustum);

    void DebugDraw(URenderer* Renderer) const;

    // Debug/Stats
    int TotalNodeCount() const;
    int TotalActorCount() const;
    int MaxOccupiedDepth() const;
    void DebugDump() const;
    const FBound& GetBounds() const { return Bounds; }

    // 프러스텀 기준으로 오클루더(내부노드 AABB) / 오클루디(리프의 액터들) 수집
// VP는 행벡터 기준(네 컨벤션): p' = p * VP
    void CollectOcclusionSets(
        const Frustum& InFrustum,
        const FMatrix& VP,
        TArray<FCandidateDrawable>& OutOccluders,
        TArray<FCandidateDrawable>& OutOccludees,
        int MaxOccluderNodes = 2048,
        int MaxOccludees = INT_MAX
    ) const;
private:
    static FBound UnionBounds(const FBound& A, const FBound& B);

    // === LBVH data ===
    struct FLBVHNode
    {
        FBound Bounds;
        int32 Left = -1;
        int32 Right = -1;
        int32 First = -1;
        int32 Count = 0;
        bool IsLeaf() const { return Count > 0; }
    };
    void BuildLBVHFromMap();

private:
    int BuildRange(int s, int e);

    int Depth;
    int MaxDepth;
    int MaxObjects;
    FBound Bounds;

    // 리프 페이로드(호환용): 유지하지만 트리 구조는 사용 안 함
    TArray<AActor*> Actors;

    // 액터의 마지막 바운드 캐시
    TMap<AActor*, FBound> ActorLastBounds;
    TArray<AActor*> ActorArray;

    // LBVH nodes
    TArray<FLBVHNode> Nodes;

    bool bPendingRebuild = false;
};

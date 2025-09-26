#pragma once
#include "Vector.h"

class UPrimitiveComponent;
class AStaticMeshActor;

struct FRay;
struct FBound;

class UWorldPartitionManager
{
public:
    UWorldPartitionManager() = default;
    ~UWorldPartitionManager() = default;

    void Clear();
    
    // Actor-based API (preferred)
    void Register(AActor* Actor);
    // 벌크 등록 - 대량 액터 처리용
    void BulkRegister(const TArray<AActor*>& Actors);
    void Unregister(AActor* Actor);
    void MarkDirty(AActor* Actor);

    void Update(float DeltaTime, uint32 budgetItems = 256);

    void Query(FRay InRay);
    void Query(FBound InBound);

private:
    // For FIFO Update 
    TQueue<AActor*> DirtyQueue;
    // For Avoid Duplication
    TSet<AActor*> DirtySet;
};
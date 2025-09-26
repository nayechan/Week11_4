# BVHierachy 설명서 (Binary BVH)

위치: TL2/BVHierachy.(h|cpp)

이 문서는 BVHierachy(경계 볼륨 계층, Bounding Volume Hierarchy)의 이진(BVH) 구현을 설명합니다. 리프 노드는 액터(AActor*) 목록을 보유하며, 내부 노드는 Left/Right 두 자식과 이들을 감싸는 AABB(FBound)를 보유합니다. 모든 노드의 Bounds는 해당 노드가 감싸는 자식(내부) 또는 액터(리프)의 FBound 합집합으로 유지됩니다.

주요 포인트(현재 구현)
- 자료구조: 옥트리(8분할) → 이진 BVH로 전환
- 리프 용량 초과 시 Split(): 가장 긴 축 기준으로 두 그룹으로 분할하여 Left/Right 생성
- Refit(): 리프는 액터들의 바운드 합집합, 내부는 Left/Right의 합집합으로 Bounds 재계산
- 삽입(Insert): 내부 노드에서는 볼륨 확장 비용이 더 작은 자식으로 내려보내는 간단한 SAH-유사 규칙 적용
- 제거(Remove): 리프에서 먼저 제거 시도, 내부는 자식 Bounds와의 교차 여부로 위임. 자식이 비면 병합(delete)

## 공개 인터페이스

생성/소멸/초기화
- BVHierachy(const FBound& InBounds, int InDepth = 0, int InMaxDepth = 5, int InMaxObjects = 32)
- ~BVHierachy()
- void Clear(): Actors/ActorLastBounds 비움, Left/Right 재귀 delete

삽입/제거/갱신
- void Insert(AActor* InActor, const FBound& ActorBounds)
  - 리프면 Actors에 추가 → 분할 조건(>MaxObjects && Depth<MaxDepth) 시 Split(), 아니면 Refit()
  - 내부면 자식 선택(볼륨 확장 비용이 작은 쪽) 후 Insert, 마지막에 Refit()
- bool Remove(AActor* InActor, const FBound& ActorBounds)
  - 리프에서 Actors에서 제거 시도 후 Refit()
  - 내부면 Left/Right Bounds와 교차 시 자식으로 위임, 제거 성공 시 자식이 비면 delete, Refit()
- void Update(AActor* InActor, const FBound& OldBounds, const FBound& NewBounds)
  - Remove(old) → Insert(new)
- void BulkInsert(const TArray<std::pair<AActor*, FBound>>& ActorsAndBounds)
  - 단순히 Insert 반복 호출(대량 최적화가 필요하면 이후 확장 가능)
- void Remove(AActor* InActor) / void Update(AActor* InActor)
  - ActorLastBounds 캐시 활용(프로젝트에서 최신 바운드 조회 API 연결 필요)

디버그/통계
- void DebugDraw(URenderer*): 현재 노드 Bounds 라인 렌더 + Left/Right 재귀
- void DebugDump(): 반복 DFS로 depth/actors/bounds 로깅
- int TotalNodeCount()/TotalActorCount()/MaxOccupiedDepth(): 재귀 집계
- const FBound& GetBounds() const

## 내부 유틸(비공개)

- void Split()
  - 가장 긴 축(ChooseSplitAxis)을 선택하고, 리프 Actors의 중심 평균 기준으로 두 그룹으로 분할
  - Left/Right 자식 노드 생성 후 각 그룹의 액터를 이동(자식의 ActorLastBounds도 채움)
  - 부모 Actors 비운 뒤, Left/Right Refit() → 부모 Refit()
- void Refit()
  - 내부: Left/Right Bounds의 합집합으로 갱신
  - 리프: 보유 Actors의 FBound 합집합으로 갱신(ActorLastBounds에서 조회)
- static FBound UnionBounds(const FBound& A, const FBound& B)
  - AABB 합집합
- int ChooseSplitAxis() const
  - 리프에 담긴 액터들의 합집합 AABB에서 가장 긴 축 반환(0:X, 1:Y, 2:Z)

## 동작 흐름(요약)

삽입(Insert)
1) 루트(또는 현재 노드)의 ActorLastBounds에 캐시 갱신
2) 리프면 Actors에 추가 → 조건 시 Split(), 아니면 Refit()
3) 내부면 Left/Right 중 볼륨 확장 비용이 작은 쪽으로 내려보내고 Refit()

제거(Remove)
1) 리프면 Actors에서 제거 → Refit()
2) 내부면 자식 Bounds와의 교차로 위임(Left→Right 순). 성공 시 자식이 비면 delete, Refit()

갱신(Update)
- Remove(old) → Insert(new)

## 사용 예시

```cpp
FBound worldBounds({-10000, -10000, -10000}, {10000, 10000, 10000});
BVHierachy bvh(worldBounds, /*depth=*/0, /*maxDepth=*/8, /*maxObjects=*/16);

// 삽입
for (AActor* actor : Actors) {
    FBound box = actor->GetWorldAABB();
    bvh.Insert(actor, box);
}

// 갱신
bvh.Update(actor, oldBox, newBox);

// 제거
bvh.Remove(actor, actorBox);

// 디버그
bvh.DebugDump();
// bvh.DebugDraw(Renderer);
```

## 주의/권장
- Split의 파티셔닝은 간단한 평균 중심 기준입니다. 성능/품질 향상이 필요하면 중앙값/SAH 근사로 개선 가능합니다.
- 삽입 시 볼륨 확장 비용 비교는 간단한 휴리스틱입니다. SAH 기반 비용함수로 교체하면 품질이 향상됩니다.
- Update(AActor*)는 프로젝트에서 액터의 최신 AABB를 얻는 경로가 있어야 의미 있게 구현할 수 있습니다.
- 대량 초기화 시에는 BulkInsert보다 별도의 Build 함수 도입을 고려하세요(한 번의 재귀 빌드로 더 적은 파편화).

## 변경 이력(이번 작업)
- 옥트리(8분할) → 이진 BVH로 전환
- Children[8] → Left/Right
- GetBVChildIndex/CanFitInBVChild 제거, Refit/ChooseSplitAxis/UnionBounds 추가
- Insert/Remove/Debug/통계/덤프 로직을 BVH 방식으로 재작성

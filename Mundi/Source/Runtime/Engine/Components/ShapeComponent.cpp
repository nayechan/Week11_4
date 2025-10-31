#include "pch.h"
#include "ShapeComponent.h"
#include "OBB.h"
#include "Collision.h"

UShapeComponent::UShapeComponent()
{
    ShapeColor = FVector4(0.2f, 0.8f, 1.0f, 1.0f);
}

 
void UShapeComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);
    
    GetWorldAABB();

    UpdateOverlaps();
}

void UShapeComponent::UpdateOverlaps()
{
    if (!bGenerateOverlapEvents)
    {
        OverlapInfos.clear();
        return;
    }
    
    UWorld* World = GetWorld();
    if (!World) return; 
    
    TSet<UShapeComponent*> Now;

    //Test용 O(N^2)
    for (AActor* Actor : World->GetActors())
    {
        for (USceneComponent* Comp : Actor->GetSceneComponents())
        {
            UShapeComponent* Other = Cast<UShapeComponent>(Comp);
            if (!Other || Other == this) continue;
            if (!Other->bGenerateOverlapEvents) continue;

            // 브로드페이즈: AABB 교차
            if (!WorldAABB.Intersects(Other->GetWorldAABB())) continue;

            // 내로우페이즈: Collision 모듈
            if (Collision::CheckOverlap(this, Other)) continue;

            Now.Add(Other);
            UE_LOG("Collision!!");
        }
    }
        
    // Overlap Info 갱신 


    // Broad Phase

    // Narrow Phase

    //Now.Add();

    //Begin
    //for( UShapeComponent* S : Now) 
    //OnComponentBeginOverlap.Broadcast(this, S);

    //for( UShapeComponent* S: Prev)
    //OnComponentEndOverlap.Broadcast(this, S);

    //공개 리스트 갱신
    //OverlapInfo <= Now
    //Prec <= Now
}

FAABB UShapeComponent::GetWorldAABB() const
{
    if (AActor* Owner = GetOwner())
    {
        FAABB OwnerBounds = Owner->GetBounds();
        const FVector HalfExtent = OwnerBounds.GetHalfExtent();
        WorldAABB = OwnerBounds; 
    }
    return WorldAABB; 
}
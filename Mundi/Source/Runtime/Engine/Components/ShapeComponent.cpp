#include "pch.h"
#include "ShapeComponent.h"
#include "OBB.h"
#include "Collision.h"
#include "World.h"

IMPLEMENT_CLASS(UShapeComponent)

UShapeComponent::UShapeComponent()
{
    ShapeColor = FVector4(0.2f, 0.8f, 1.0f, 1.0f);
    bGenerateOverlapEvents = true;
}

 
void UShapeComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);
    
    GetWorldAABB();

    UpdateOverlaps();
}

void UShapeComponent::OnTransformUpdated()
{
    GetWorldAABB();
    UpdateOverlaps();
    Super::OnTransformUpdated();
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
            if (Other->GetOwner() == this->GetOwner()) continue;
            if (!Other->bGenerateOverlapEvents) continue;
 
            // 내로우페이즈: Collision 모듈
            if (!Collision::CheckOverlap(this, Other)) continue;

            Now.Add(Other);
            UE_LOG("Collision!!");
        }
    } 

    // Publish current overlaps
    OverlapInfos.clear();
    for (UShapeComponent* Other : Now)
    {
        FOverlapInfo Info;
        Info.OtherActor = Other->GetOwner();
        Info.Other = Other;
        OverlapInfos.Add(Info);
    } 

    //Begin
    //for( UShapeComponent* Comp : Now) 
    //OnComponentBeginOverlap.Broadcast(this, Comp);

    //for( UShapeComponent* Comp : Prev)
    //OnComponentEndOverlap.Broadcast(this, Comp);

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



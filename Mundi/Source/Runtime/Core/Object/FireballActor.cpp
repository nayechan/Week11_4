#include "pch.h"
#include "FireballActor.h"
#include "PointLightComponent.h"
#include "StaticMeshComponent.h"
#include "Material.h"
#include "ResourceManager.h"1
#include "Texture.h"
#include "RotatingMovementComponent.h"


IMPLEMENT_CLASS(AFireBallActor)

BEGIN_PROPERTIES(AFireBallActor)
	MARK_AS_SPAWNABLE("파이어 볼", "파이어 볼을 렌더링하는 액터입니다.")
END_PROPERTIES()

AFireBallActor::AFireBallActor()
{
	Name = "Fire Ball Actor";
	PointLightComponent = CreateDefaultSubobject<UPointLightComponent>("PointLightComponent");
	PointLightComponent->SetupAttachment(RootComponent);
	 
	StaticMeshComponent->SetStaticMesh(GDataDir  + "/Model/SmoothSphere.obj");
	StaticMeshComponent->SetMaterialByName(0, "Shaders/Materials/Fireball.hlsl");

	RotatingComponent = CreateDefaultSubobject<URotatingMovementComponent>("RotatingComponent");
	RotatingComponent->SetRotationRate(FVector(0, 0, 400.0f));

	UTexture* NoiseTex = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Textures/FireballNoise.jpg");
	
	// lua scrupt 
}

AFireBallActor::~AFireBallActor()
{

}

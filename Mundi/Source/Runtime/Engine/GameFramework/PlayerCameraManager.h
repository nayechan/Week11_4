#pragma once
#include "Actor.h"

class UCameraComponent;

class APlayerCameraManager : public AActor
{
	DECLARE_CLASS(APlayerCameraManager, AActor)
	GENERATED_REFLECTION_BODY()

public:
	APlayerCameraManager();

protected:
	~APlayerCameraManager() override;

public:
	void Destroy() override;
	// Actor의 메인 틱 함수
	void Tick(float DeltaTime) override;
	void UpdateCamera(float DeltaTime);

	//// 렌더러가 호출할 최종 뷰 정보 GETTER (매우 빠름) return 모든 효과가 적용된 최종 뷰 정보 (캐시된 값)
	//FMinimalViewInfo GetFinalViewInfo() const;

	void SetMainCamera(UCameraComponent* InCamera) { MainCamera = InCamera; };
	UCameraComponent* GetMainCamera();

	DECLARE_DUPLICATE(APlayerCameraManager)

private:
	UCameraComponent* MainCamera{};
};

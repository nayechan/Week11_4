#pragma once
#include "Actor.h"

class UCameraComponent;
class UCameraModifierBase;
class FSceneView;
class APlayerCameraManager : public AActor
{
	DECLARE_CLASS(APlayerCameraManager, AActor)
	GENERATED_REFLECTION_BODY()

public:
	APlayerCameraManager();

	TArray<UCameraModifierBase*> ActiveModifiers;
	
	template<typename T> T* AddModifier(int32 InPriority = 0)
	{
		T* M = NewObject<T>(this);
		M->Priority = InPriority;
		ActiveModifiers.Add(M);
		ActiveModifiers.Sort([](auto A, auto B){ return *A < *B; });
		return M;
	}
	void BuildForFrame(float DeltaTime, FSceneView& InOutView);
	
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

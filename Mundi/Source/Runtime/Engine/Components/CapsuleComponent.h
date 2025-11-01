#pragma once
#include "ShapeComponent.h"

class UCapsuleComponent : public UShapeComponent
{
public:
	DECLARE_CLASS(UCapsuleComponent, UShapeComponent)
	GENERATED_REFLECTION_BODY();

	UCapsuleComponent();
	void OnRegister(UWorld* World) override;

	// Duplication
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UCapsuleComponent)

protected:
	float CapsuleHalfHeight;
	float CapsuleRadius;

	void GetShape(FShape& Out) const override; 

public:
	void RenderDebugVolume(class URenderer* Renderer) const override;
};
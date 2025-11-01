#pragma once
#include "ShapeComponent.h"

class UBoxComponent : public UShapeComponent
{
public:
	DECLARE_CLASS(UBoxComponent, UShapeComponent)
	GENERATED_REFLECTION_BODY();

	UBoxComponent(); 
	void OnRegister(UWorld* InWorld) override;

	// Duplication
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UBoxComponent)

private:
	FVector BoxExtent; // Half Extent

	void GetShape(FShape& Out) const override;

public:
	void RenderDebugVolume(class URenderer* Renderer) const override;
};
#pragma once
#include "Widget.h"
#include "../../Vector.h"
#include "../../UEContainer.h"

class UUIManager;
class USelectionManager;
class AActor;
class UActorComponent;

/**
 * DetailsPanelWidget
 * - Unreal Engine style property inspector
 * - Shows and allows editing of selected object properties
 * - Transform, name, components, material properties
 * - Auto-updates when selection changes
 */
class UDetailsPanelWidget : public UWidget
{
public:
    DECLARE_CLASS(UDetailsPanelWidget, UWidget)
    
    void Initialize() override;
    void Update() override;
    void RenderWidget() override;
    
    // Special Member Functions
    UDetailsPanelWidget();
    ~UDetailsPanelWidget() override;

private:
    UUIManager* UIManager = nullptr;
    USelectionManager* SelectionManager = nullptr;
    
    // Current selection state
    AActor* CurrentSelectedActor = nullptr;
    bool bNeedsRefresh = true;
    
    // UI Categories
    struct FPropertyCategory
    {
        FString Name;
        bool bIsExpanded = true;
        
        FPropertyCategory(const FString& InName) : Name(InName) {}
    };
    
    // Property rendering methods
    void RenderActorDetails(AActor* Actor);
    void RenderTransformSection(AActor* Actor);
    void RenderGeneralSection(AActor* Actor);
    void RenderComponentsSection(AActor* Actor);
    void RenderComponentDetails(UActorComponent* Component);
    
    // Transform editing helpers
    void RenderVectorProperty(const char* Label, FVector& Value, float Speed = 0.1f, const char* Format = "%.3f");
    void RenderQuaternionAsEuler(const char* Label, FQuat& Quat);
    void RenderFloatProperty(const char* Label, float& Value, float Speed = 0.1f, float Min = 0.0f, float Max = 0.0f);
    void RenderStringProperty(const char* Label, FString& Value);
    void RenderBoolProperty(const char* Label, bool& Value);
    
    // Category management
    bool BeginCategory(const FString& CategoryName, bool bDefaultExpanded = true);
    void EndCategory();
    
    // Selection tracking
    void CheckSelectionChanged();
    void OnSelectionChanged(AActor* NewSelection);
    
    // Property validation and apply
    void ValidateAndApplyTransformChanges(AActor* Actor);
    
    // Component type helpers
    const char* GetComponentTypeName(UActorComponent* Component);
    void RenderStaticMeshComponentProperties(class UStaticMeshComponent* MeshComp);
    void RenderCameraComponentProperties(class UCameraComponent* CameraComp);
    
    // Utility functions
    void RenderPropertySeparator();
    void RenderReadOnlyProperty(const char* Label, const char* Value);
    
    // Color theme
    ImVec4 GetCategoryHeaderColor() const { return ImVec4(0.2f, 0.3f, 0.4f, 1.0f); }
    ImVec4 GetPropertyLabelColor() const { return ImVec4(0.8f, 0.8f, 0.8f, 1.0f); }
};
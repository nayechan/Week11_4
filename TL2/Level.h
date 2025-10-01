#pragma once
#include "Object.h"
#include "UEContainer.h"
#include <algorithm>
#include "SceneLoader.h"

class AActor;

class ULevel : public UObject
{
public:
    DECLARE_CLASS(ULevel, UObject)
    ULevel() = default;
    ~ULevel() override = default;

    const TArray<AActor*>& GetActors() const { return Actors; }
    void AddActor(AActor* Actor) { if (Actor) Actors.Add(Actor); }
    bool RemoveActor(AActor* Actor)
    {
        auto it = std::find(Actors.begin(), Actors.end(), Actor);
        if (it != Actors.end()) { Actors.erase(it); return true; }
        return false;
    }
    void Clear() { Actors.Empty(); }

private:
    TArray<AActor*> Actors;
};

// Simple static service for creating/loading/saving levels
struct FLoadedLevel
{
    std::unique_ptr<ULevel> Level;
    FPerspectiveCameraData Camera; // optional camera data for editor
};

class ULevelService
{
public:
    // Create a new empty level
    static std::unique_ptr<ULevel> CreateNewLevel();

    // Load a level from Scene/<SceneName>.Scene and return constructed level + camera
    static FLoadedLevel LoadLevel(const FString& SceneName);

    // Save given level (actors) and optional camera to Scene/<SceneName>.Scene
    static void SaveLevel(const ULevel* Level, const ACameraActor* Camera, const FString& SceneName);
};

#pragma once

// Feature Flags
// Uncomment to enable DDS texture caching (faster loading, uses Data/TextureCache/)
#define USE_DDS_CACHE
#define USE_OBJ_CACHE

#define IMGUI_DEFINE_MATH_OPERATORS	// Imgui에서 곡선 표시를 위한 전용 벡터 연산자 활성화

// Linker
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

// DirectXTK
#pragma comment(lib, "DirectXTK.lib")

// Standard Library (MUST come before UEContainer.h)
#include <vector>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <stack>
#include <list>
#include <deque>
#include <string>
#include <array>
#include <algorithm>
#include <functional>
#include <memory>
#include <cmath>
#include <limits>
#include <iostream>
#include <fstream>
#include <utility>
#include <filesystem>
#include <sstream>
#include <iterator>

// Windows & DirectX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <cassert>

// Core Project Headers
#include "Vector.h"

// FRect 정의 (전역으로 사용)
#ifndef FRECT_DEFINED
#define FRECT_DEFINED
struct FRect
{
    float Left, Top, Right, Bottom;
    FVector2D Min, Max;

    FRect() : Left(0), Top(0), Right(0), Bottom(0), Min(0, 0), Max(0, 0) {}
    FRect(float InLeft, float InTop, float InRight, float InBottom)
        : Left(InLeft), Top(InTop), Right(InRight), Bottom(InBottom)
        , Min(InLeft, InTop), Max(InRight, InBottom) {}

    float GetWidth() const { return Right - Left; }
    float GetHeight() const { return Bottom - Top; }

    bool Contains(FVector2D P) const
    {
        return (P.X > Left && P.X < Right && P.Y > Top && P.Y < Bottom);
    }

    void UpdateMinMax()
    {
        Min.X = Left; Min.Y = Top;
        Max.X = Right; Max.Y = Bottom;
    }
};
#endif

#include "ResourceData.h"
#include "VertexData.h"
#include "UEContainer.h"
#include "Name.h"
#include "PathUtils.h"
#include "Object.h"
#include "ObjectFactory.h"
#include "ObjectMacros.h"
#include "Enums.h"
#include "GlobalConsole.h"
#include "D3D11RHI.h"
#include "World.h"
#include "ConstantBufferType.h"
// d3dtk
#include "SimpleMath.h"

// ImGui
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

// nlohmann
#include "nlohmann/json.hpp"

// Manager
#include "Renderer.h"
#include "InputManager.h"
#include "UIManager.h"
#include "ResourceManager.h"

#include "JsonSerializer.h"

#define RESOURCE UResourceManager::GetInstance()
#define UI UUIManager::GetInstance()
#define INPUT UInputManager::GetInstance()
#define RENDER URenderManager::GetInstance()
#define SLATE USlateManager::GetInstance()

//(월드 별 소유)
//#define PARTITION UWorldPartitionManager::GetInstance()
//#define SELECTION (GEngine.GetDefaultWorld()->GetSelectionManager())

extern TMap<FString, FString> EditorINI;
extern const FString GDataDir;
extern const FString GCacheDir;

// Editor & Game
#include "EditorEngine.h"
#include "GameEngine.h"

//CUR ENGINE MODE
//#define _EDITOR

#ifdef _EDITOR
extern UEditorEngine GEngine;
#endif

#ifdef _GAME
extern UGameEngine GEngine;
#endif

extern UWorld* GWorld;
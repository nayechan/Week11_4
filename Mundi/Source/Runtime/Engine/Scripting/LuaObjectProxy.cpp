#include "pch.h"
#include "LuaObjectProxy.h"
#include "ObjectFactory.h"  // For GUObjectArray

TMap<UClass*, FBoundClassDesc> GBoundClasses;

// External function from LuaManager.cpp
extern sol::object MakeCompProxy(sol::state_view SolState, void* Instance, UClass* Class);

// ===== Helper Functions =====

// Map EPropertyType to expected UClass* for UObject pointer types
// NOTE: Update this map when new UObject-derived types are added to EPropertyType enum
// ObjectPtr maps to UObject (accepts any UObject-derived type)
static const TMap<EPropertyType, UClass*> ObjectPointerTypeMap = {
    {EPropertyType::ObjectPtr,     UObject::StaticClass()},
    {EPropertyType::Texture,       UTexture::StaticClass()},
    {EPropertyType::StaticMesh,    UStaticMesh::StaticClass()},
    {EPropertyType::SkeletalMesh,  USkeletalMesh::StaticClass()},
    {EPropertyType::Material,      UMaterialInterface::StaticClass()},
    {EPropertyType::Sound,         USound::StaticClass()}
};

// Validate UObject pointer using GUObjectArray
static bool IsValidUObject(UObject* Ptr)
{
    if (!Ptr) return false;

    // Step 1: Check InternalIndex range
    uint32_t idx = Ptr->InternalIndex;
    if (idx >= static_cast<uint32_t>(GUObjectArray.Num()))
        return false;

    // Step 2: Verify GUObjectArray slot points to the same object
    UObject* RegisteredObj = GUObjectArray[idx];
    if (RegisteredObj != Ptr)
        return false;  // Deleted or different object

    // Step 3: UUID validation (commented for now)
    // TODO(SlotReuse): If ObjectFactory enables null slot reuse, uncomment below
    // to prevent ABA problem (same address, different object):
    // if (RegisteredObj->UUID != Ptr->UUID)
    //     return false;

    return true;
}

// Check if EPropertyType represents a UObject pointer
static bool IsObjectPointerType(EPropertyType Type)
{
    return ObjectPointerTypeMap.count(Type) > 0;
}

// Get expected UClass* for property type (for type validation)
static UClass* GetExpectedClassForPropertyType(EPropertyType Type)
{
    auto it = ObjectPointerTypeMap.find(Type);
    return (it != ObjectPointerTypeMap.end()) ? it->second : nullptr;
}

// ===== Core Functions =====

void BuildBoundClass(UClass* Class)
{
    if (!Class) return;
    if (GBoundClasses.count(Class)) return;

    FBoundClassDesc Desc;
    Desc.Class = Class;

    // Count bound properties first
    int BoundCount = 0;
    for (auto& Property : Class->GetAllProperties())
    {
        // LuaReadWrite 메타데이터가 있는 프로퍼티만 Lua에 노출
        auto LuaReadWriteIt = Property.Metadata.find(FName("LuaReadWrite"));
        if (LuaReadWriteIt == Property.Metadata.end()) continue;

        FBoundProp BoundProp;
        BoundProp.Property = &Property;
        Desc.PropsByName.emplace(Property.Name, BoundProp);
        BoundCount++;
    }

    // Log summary only
    UE_LOG("[LuaObjectProxy] Bound class: %s (%d/%d properties exposed to Lua)",
           Class->Name, BoundCount, (int)Class->GetAllProperties().size());
    GBoundClasses.emplace(Class, std::move(Desc));
}

bool LuaObjectProxy::IsValid() const
{
    return IsValidUObject(Instance);
}

sol::object LuaObjectProxy::Index(sol::this_state LuaState, LuaObjectProxy& Self, const char* Key)
{
    if (!Self.Instance) return sol::nil;
    sol::state_view LuaView(LuaState);

    BuildBoundClass(Self.Class);


    sol::table& FuncTable = FLuaBindRegistry::Get().EnsureTable(LuaView, Self.Class);
    sol::object Func = (FuncTable.valid() ? FuncTable.raw_get<sol::object>(Key) : sol::object());

    if (Func.valid() && Func.get_type() == sol::type::function)
        return Func;

    auto It = GBoundClasses.find(Self.Class);
    if (It == GBoundClasses.end()) return sol::nil;

    auto ItProp = It->second.PropsByName.find(Key);
    if (ItProp == It->second.PropsByName.end()) return sol::nil;

    const FProperty* Property = ItProp->second.Property;
    switch (Property->Type)
    {
    case EPropertyType::Bool:         return sol::make_object(LuaView, *Property->GetValuePtr<bool>(Self.Instance));
    case EPropertyType::Float:        return sol::make_object(LuaView, *Property->GetValuePtr<float>(Self.Instance));
    case EPropertyType::Int32:        return sol::make_object(LuaView, *Property->GetValuePtr<int>(Self.Instance));
    case EPropertyType::FString:      return sol::make_object(LuaView, *Property->GetValuePtr<FString>(Self.Instance));
    case EPropertyType::FVector:      return sol::make_object(LuaView, *Property->GetValuePtr<FVector>(Self.Instance));
    case EPropertyType::FLinearColor: return sol::make_object(LuaView, *Property->GetValuePtr<FLinearColor>(Self.Instance));
    case EPropertyType::FName:        return sol::make_object(LuaView, Property->GetValuePtr<FName>(Self.Instance)->ToString());

    // UObject pointer types (supports recursive access)
    case EPropertyType::ObjectPtr:
    case EPropertyType::Texture:
    case EPropertyType::SkeletalMesh:
    case EPropertyType::StaticMesh:
    case EPropertyType::Material:
    case EPropertyType::Sound:
    {
        UObject** ObjPtr = Property->GetValuePtr<UObject*>(Self.Instance);
        if (!ObjPtr || !IsValidUObject(*ObjPtr))
            return sol::nil;

        UObject* TargetObj = *ObjPtr;
        // MakeCompProxy automatically calls BuildBoundClass for the target object
        // This enables recursive property access (e.g., comp.CameraGizmo.StaticMesh)
        return MakeCompProxy(LuaView, TargetObj, TargetObj->GetClass());
    }

    // Array of UObject pointers
    case EPropertyType::Array:
    {
        // Only support arrays of UObject pointer types
        if (!IsObjectPointerType(Property->InnerType))
            return sol::nil;

        TArray<UObject*>* ArrayPtr = Property->GetValuePtr<TArray<UObject*>>(Self.Instance);
        if (!ArrayPtr) return sol::nil;

        // Create Lua table (1-based indexing)
        sol::table Result = LuaView.create_table();

        for (size_t i = 0; i < ArrayPtr->size(); ++i)
        {
            UObject* Element = (*ArrayPtr)[i];

            // Validate each element
            if (IsValidUObject(Element))
            {
                // Create proxy for each element (enables recursive access)
                Result[i + 1] = MakeCompProxy(LuaView, Element, Element->GetClass());
            }
            else
            {
                Result[i + 1] = sol::nil;
            }
        }

        return Result;
    }

    default: return sol::nil;
    }
}

void LuaObjectProxy::NewIndex(LuaObjectProxy& Self, const char* Key, sol::object Obj)
{
    auto IterateClass = GBoundClasses.find(Self.Class);
    if (IterateClass == GBoundClasses.end()) return;

    auto It = IterateClass->second.PropsByName.find(Key);
    if (It == IterateClass->second.PropsByName.end()) return;

    const FProperty* Property = It->second.Property;

    switch (Property->Type)
    {
    case EPropertyType::Bool:
        if (Obj.get_type() == sol::type::boolean)
            *Property->GetValuePtr<bool>(Self.Instance) = Obj.as<bool>();
        break;
    case EPropertyType::Float:
        if (Obj.get_type() == sol::type::number)
            *Property->GetValuePtr<float>(Self.Instance) = static_cast<float>(Obj.as<double>());
        break;
    case EPropertyType::Int32:
        if (Obj.get_type() == sol::type::number)
            *Property->GetValuePtr<int>(Self.Instance) = static_cast<int>(Obj.as<double>());
        break;
    case EPropertyType::FString:
        if (Obj.get_type() == sol::type::string)
            *Property->GetValuePtr<FString>(Self.Instance) = Obj.as<FString>();
        break;
    case EPropertyType::FVector:
        if (Obj.is<FVector>())
        {
            *Property->GetValuePtr<FVector>(Self.Instance) = Obj.as<FVector>();
        }
        else if (Obj.get_type() == sol::type::table)
        {
            sol::table t = Obj.as<sol::table>();
            FVector tmp{
                static_cast<float>(t.get_or("X", 0.0)),
                static_cast<float>(t.get_or("Y", 0.0)),
                static_cast<float>(t.get_or("Z", 0.0))
            };
            *Property->GetValuePtr<FVector>(Self.Instance) = tmp;
        }
        break;
    case EPropertyType::FLinearColor:
        if (Obj.is<FLinearColor>())
        {
            *Property->GetValuePtr<FLinearColor>(Self.Instance) = Obj.as<FLinearColor>();
        }
        else if (Obj.get_type() == sol::type::table)
        {
            sol::table t = Obj.as<sol::table>();
            FLinearColor tmp{
                static_cast<float>(t.get_or("R", 1.0)),
                static_cast<float>(t.get_or("G", 1.0)),
                static_cast<float>(t.get_or("B", 1.0)),
                static_cast<float>(t.get_or("A", 1.0))
            };
            *Property->GetValuePtr<FLinearColor>(Self.Instance) = tmp;
        }
        break;
    case EPropertyType::FName:
        if (Obj.get_type() == sol::type::string)
            *Property->GetValuePtr<FName>(Self.Instance) = FName(Obj.as<FString>());
        break;

    // UObject pointer types (with type validation)
    case EPropertyType::ObjectPtr:
    case EPropertyType::Texture:
    case EPropertyType::SkeletalMesh:
    case EPropertyType::StaticMesh:
    case EPropertyType::Material:
    case EPropertyType::Sound:
    {
        UObject** ObjPtr = Property->GetValuePtr<UObject*>(Self.Instance);
        if (!ObjPtr) break;

        // Handle nil assignment (set to nullptr)
        if (Obj.get_type() == sol::type::nil || Obj.get_type() == sol::type::none)
        {
            *ObjPtr = nullptr;
            break;
        }

        // Must be a proxy object
        if (!Obj.is<LuaObjectProxy>())
        {
            UE_LOG("[Lua][warning] Cannot assign non-UObject to property '%s'\n", Property->Name);
            break;
        }

        LuaObjectProxy& SourceProxy = Obj.as<LuaObjectProxy&>();
        UObject* SourceObj = SourceProxy.Instance;

        // Handle nil proxy
        if (!SourceObj)
        {
            *ObjPtr = nullptr;
            break;
        }

        // Validate object is still alive
        if (!IsValidUObject(SourceObj))
        {
            UE_LOG("[Lua][warning] Cannot assign deleted UObject to property '%s'\n", Property->Name);
            break;
        }

        // Type validation (Unreal-style covariant assignment)
        // ObjectPtr accepts any UObject-derived type
        if (Property->Type != EPropertyType::ObjectPtr)
        {
            UClass* ExpectedClass = GetExpectedClassForPropertyType(Property->Type);
            if (ExpectedClass && !SourceObj->IsA(ExpectedClass))
            {
                UE_LOG("[Lua][warning] Type mismatch: cannot assign %s to %s property '%s'\n",
                       SourceObj->GetClass()->Name, ExpectedClass->Name, Property->Name);
                break;
            }
        }

        // Assignment successful
        *ObjPtr = SourceObj;
        break;
    }

    // Array of UObject pointers
    case EPropertyType::Array:
    {
        // Only support arrays of UObject pointer types
        if (!IsObjectPointerType(Property->InnerType))
            break;

        TArray<UObject*>* ArrayPtr = Property->GetValuePtr<TArray<UObject*>>(Self.Instance);
        if (!ArrayPtr) break;

        // nil assignment → clear array
        if (Obj.get_type() == sol::type::nil || Obj.get_type() == sol::type::none)
        {
            ArrayPtr->clear();
            break;
        }

        // Must be a table
        if (Obj.get_type() != sol::type::table)
        {
            UE_LOG("[Lua][warning] Cannot assign non-table to array property '%s'\n", Property->Name);
            break;
        }

        sol::table SourceTable = Obj.as<sol::table>();
        TArray<UObject*> NewArray;

        // Iterate Lua table (1-based indexing)
        for (size_t i = 1; i <= SourceTable.size(); ++i)
        {
            sol::object Element = SourceTable[i];

            // nil element
            if (Element.get_type() == sol::type::nil)
            {
                NewArray.push_back(nullptr);
                continue;
            }

            // Must be proxy
            if (!Element.is<LuaObjectProxy>())
            {
                UE_LOG("[Lua][warning] Array[%zu] is not a valid UObject proxy\n", i);
                NewArray.push_back(nullptr);
                continue;
            }

            LuaObjectProxy& ElemProxy = Element.as<LuaObjectProxy&>();
            UObject* ElemObj = ElemProxy.Instance;

            // Validate element
            if (ElemObj && !IsValidUObject(ElemObj))
            {
                UE_LOG("[Lua][warning] Array[%zu] is a deleted object\n", i);
                NewArray.push_back(nullptr);
                continue;
            }

            // Type validation for array elements
            if (ElemObj && Property->InnerType != EPropertyType::ObjectPtr)
            {
                UClass* ExpectedClass = GetExpectedClassForPropertyType(Property->InnerType);
                if (ExpectedClass && !ElemObj->IsA(ExpectedClass))
                {
                    UE_LOG("[Lua][warning] Array[%zu] type mismatch: %s → %s\n",
                           i, ElemObj->GetClass()->Name, ExpectedClass->Name);
                    NewArray.push_back(nullptr);
                    continue;
                }
            }

            NewArray.push_back(ElemObj);
        }

        // Replace array
        *ArrayPtr = std::move(NewArray);
        break;
    }

    default:
        break;
    }
}

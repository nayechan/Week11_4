#include "pch.h"
#include "LuaStructProxy.h"
#include "LuaObjectProxy.h"
#include "LuaObjectProxyHelpers.h"

// External function from LuaManager.cpp (for nested UObject* access)
extern sol::object MakeCompProxy(sol::state_view SolState, void* Instance, UClass* Class);

sol::object LuaStructProxy::Index(sol::this_state LuaState, LuaStructProxy& Self, const char* Key)
{
	if (!Self.Instance || !Self.StructType)
		return sol::nil;

	sol::state_view LuaView(LuaState);

	// Find property by name
	const FProperty* Property = nullptr;
	for (const auto& Prop : Self.StructType->GetAllProperties())
	{
		if (strcmp(Prop.Name, Key) == 0)
		{
			// LuaReadWrite 메타데이터 체크 (Unreal의 BlueprintReadWrite와 동일)
			auto LuaReadWriteIt = Prop.Metadata.find(FName("LuaReadWrite"));
			if (LuaReadWriteIt == Prop.Metadata.end())
			{
				// LuaReadWrite가 없으면 Lua에 노출 안 함
				return sol::nil;
			}

			Property = &Prop;
			break;
		}
	}

	if (!Property)
		return sol::nil;

	// Return value based on property type
	switch (Property->Type)
	{
	case EPropertyType::Bool:
		return sol::make_object(LuaView, *Property->GetValuePtr<bool>(Self.Instance));

	case EPropertyType::Int32:
		return sol::make_object(LuaView, *Property->GetValuePtr<int>(Self.Instance));

	case EPropertyType::Float:
		return sol::make_object(LuaView, *Property->GetValuePtr<float>(Self.Instance));

	case EPropertyType::FString:
		return sol::make_object(LuaView, *Property->GetValuePtr<FString>(Self.Instance));

	case EPropertyType::FVector:
		return sol::make_object(LuaView, *Property->GetValuePtr<FVector>(Self.Instance));

	case EPropertyType::FLinearColor:
		return sol::make_object(LuaView, *Property->GetValuePtr<FLinearColor>(Self.Instance));

	case EPropertyType::FName:
		return sol::make_object(LuaView, Property->GetValuePtr<FName>(Self.Instance)->ToString());

	// Nested struct access (recursive) - 재귀적 구조체 접근
	case EPropertyType::Struct:
	{
		UStruct* NestedStructType = UStruct::FindStruct(FName(Property->StructTypeName));
		if (!NestedStructType)
		{
			UE_LOG("[Lua][error] Unknown struct type: %s", Property->StructTypeName);
			return sol::nil;
		}

		void* NestedStructInstance = (char*)Self.Instance + Property->Offset;
		return sol::make_object(LuaView, LuaStructProxy(NestedStructInstance, NestedStructType));
	}

	// UObject pointer types (mixed struct-class access)
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
		return MakeCompProxy(LuaView, TargetObj, TargetObj->GetClass());
	}

	default:
		return sol::nil;
	}
}

void LuaStructProxy::NewIndex(LuaStructProxy& Self, const char* Key, sol::object Obj)
{
	if (!Self.Instance || !Self.StructType)
		return;

	// Find property by name
	const FProperty* Property = nullptr;
	for (const auto& Prop : Self.StructType->GetAllProperties())
	{
		if (strcmp(Prop.Name, Key) == 0)
		{
			// LuaReadWrite 메타데이터 체크 (읽기 전용 방지)
			auto LuaReadWriteIt = Prop.Metadata.find(FName("LuaReadWrite"));
			if (LuaReadWriteIt == Prop.Metadata.end())
			{
				UE_LOG("[Lua][warning] Property '%s.%s' is not marked as LuaReadWrite",
					Self.StructType->Name, Key);
				return;
			}

			Property = &Prop;
			break;
		}
	}

	if (!Property)
		return;

	// Set value based on property type
	switch (Property->Type)
	{
	case EPropertyType::Bool:
		if (Obj.get_type() == sol::type::boolean)
			*Property->GetValuePtr<bool>(Self.Instance) = Obj.as<bool>();
		break;

	case EPropertyType::Int32:
		if (Obj.get_type() == sol::type::number)
			*Property->GetValuePtr<int>(Self.Instance) = static_cast<int>(Obj.as<double>());
		break;

	case EPropertyType::Float:
		if (Obj.get_type() == sol::type::number)
			*Property->GetValuePtr<float>(Self.Instance) = static_cast<float>(Obj.as<double>());
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

	// Nested struct: cannot replace entire struct, only modify its properties
	case EPropertyType::Struct:
		UE_LOG("[Lua][warning] Cannot assign to struct property '%s' directly. Modify its fields instead.", Key);
		break;

	// UObject pointers
	case EPropertyType::ObjectPtr:
	case EPropertyType::Texture:
	case EPropertyType::SkeletalMesh:
	case EPropertyType::StaticMesh:
	case EPropertyType::Material:
	case EPropertyType::Sound:
	{
		UObject** ObjPtr = Property->GetValuePtr<UObject*>(Self.Instance);
		if (!ObjPtr)
			break;

		// Handle nil assignment
		if (Obj.get_type() == sol::type::nil || Obj.get_type() == sol::type::none)
		{
			*ObjPtr = nullptr;
			break;
		}

		// Must be a proxy object
		if (!Obj.is<LuaObjectProxy>())
		{
			UE_LOG("[Lua][warning] Cannot assign non-UObject to property '%s'", Property->Name);
			break;
		}

		LuaObjectProxy& SourceProxy = Obj.as<LuaObjectProxy&>();
		UObject* SourceObj = SourceProxy.Instance;

		if (SourceObj && IsValidUObject(SourceObj))
		{
			// Type validation could be added here
			*ObjPtr = SourceObj;
		}
		break;
	}

	default:
		break;
	}
}

void LuaStructProxy::RegisterLua(sol::state_view LuaState)
{
	LuaState.new_usertype<LuaStructProxy>("LuaStructProxy",
		sol::meta_function::index, &LuaStructProxy::Index,
		sol::meta_function::new_index, &LuaStructProxy::NewIndex
	);
}

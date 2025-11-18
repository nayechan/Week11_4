#pragma once

#include <vector>
#include <functional>
#include <algorithm>
#include <memory>

using FDelegateHandle = size_t;

template<typename... Args>
class TDelegate
{
public:
	using HandlerType = std::function<void(Args...)>;

	TDelegate() : NextHandle(1) {}

	FDelegateHandle Add(const HandlerType& Handler)
	{
		FDelegateHandle Handle = NextHandle++;
		Handlers.push_back({ Handle, Handler });
		return Handle;
	}

	// original: template<typename T>
	template<typename TObj, typename TClass>
	FDelegateHandle AddDynamic(TObj* Instance, void(TClass::* Func)(Args...))
	{
		FDelegateHandle Handle = NextHandle++;
		Handlers.push_back({
			Handle,
			[=](Args... args) {
			(Instance->*Func)(args...); }
		});
		return Handle;
	}

	void Broadcast(Args... args) 
	{
		for (auto& Entry : Handlers) {
			if (Entry.Handler)
			{
				Entry.Handler(args...);
			}
		}
	}

	void Remove(FDelegateHandle Handle)
	{
		auto it = std::remove_if(Handlers.begin(), Handlers.end(),
		[&](const Entry& e)
		{
			return e.Handle == Handle;
		});
		Handlers.erase(it, Handlers.end());
	}

	void Clear()
	{
		Handlers.clear();
	}

private:
	struct Entry
	{
		FDelegateHandle Handle;
		HandlerType Handler;
	};

	std::vector<Entry> Handlers;
	FDelegateHandle NextHandle;
};

// 델리게이트 인스턴스 생성용 매크로 (실제 멤버 변수 선언)
#define DECLARE_DELEGATE(Name, ...)				TDelegate<__VA_ARGS__> Name
#define DECLARE_DELEGATE_OneParam(Name, T1)		TDelegate<T1> Name
#define DECLARE_DELEGATE_TwoParam(Name, T1, T2)	TDelegate<T1, T2> Name
#define DECLARE_DYNAMIC_DELEGATE(Name, ...)		std::shared_ptr<TDelegate<__VA_ARGS__>> Name = std::make_shared<TDelegate<__VA_ARGS__>>();

// 델리게이트 타입 정의용 매크로 (인스턴스 직접 선언해서 여러 군데에 재사용)
#define DECLARE_DELEGATE_TYPE(Name, ...)          using Name = TDelegate<__VA_ARGS__>;
#define DECLARE_DELEGATE_TYPE_OneParam(Name, T1)  using Name = TDelegate<T1>;
#define DECLARE_DELEGATE_TYPE_TwoParam(Name, T1, T2) using Name = TDelegate<T1, T2>;
#define DECLARE_DYNAMIC_DELEGATE_TYPE(Name, ...)  using Name = std::shared_ptr<TDelegate<__VA_ARGS__>>;

// ============================================================
// TFunction - 단일 핸들러, 반환값 지원
// ============================================================

template<typename Signature>
class TFunction;

template<typename RetType, typename... Args>
class TFunction<RetType(Args...)>
{
public:
	using FunctionType = std::function<RetType(Args...)>;

	TFunction() = default;

	// 람다/함수 바인딩
	template<typename Callable>
	void Bind(Callable&& InCallable)
	{
		Handler = std::forward<Callable>(InCallable);
	}

	// 멤버 함수 바인딩
	template<typename TObj, typename TClass>
	void BindMember(TObj* Instance, RetType(TClass::* Func)(Args...))
	{
		Handler = [=](Args... args) -> RetType {
			return (Instance->*Func)(args...);
		};
	}

	// 실행
	RetType Execute(Args... args) const
	{
		if (!Handler)
		{
			// 기본값 반환 (bool의 경우 false, 포인터는 nullptr 등)
			return RetType();
		}
		return Handler(args...);
	}

	// 바인딩 여부 확인
	bool IsBound() const
	{
		return static_cast<bool>(Handler);
	}

	// 바인딩 해제
	void Unbind()
	{
		Handler = nullptr;
	}

private:
	FunctionType Handler;
};

// TFunction 매크로 정의
#define DECLARE_FUNCTION_RetVal(Name, RetType) \
	using Name = TFunction<RetType()>;

#define DECLARE_FUNCTION_RetVal_OneParam(Name, RetType, ParamType) \
	using Name = TFunction<RetType(ParamType)>;

#define DECLARE_FUNCTION_RetVal_TwoParam(Name, RetType, Param1Type, Param2Type) \
	using Name = TFunction<RetType(Param1Type, Param2Type)>;
#pragma once
// Delegate의 안전성을 위해 필요한 weakptr, 추후 사용할 예정

template<typename T>
class TWeakPtr
{
public:
	TWeakPtr() = default;

	TWeakPtr(T* InObject)
	{
		static_assert(std::is_base_of_v<UObject, T>, "T must be derived from UObject");
		if (InObject)
		{
			Index = InObject->InternalIndex;
		}
	}

	TWeakPtr& operator=(T* InObject)
	{
		static_assert(std::is_base_of_v<UObject, T>, "T must be derived from UObject");
		if (InObject)
		{
			Index = InObject->InternalIndex;
		}
		else
		{
			Index = -1;
		}
		return *this;
	}

	// UObject가 소멸할때 GUObjectArray에서 InternalIndex element를 nullptr로 설정함
	// 객체를 직접 참조하지 않고 InternalIndex로 참조하면 정말 객체가 유효할때만 IsValid true 리턴함.
	// 근데 이건 InternalIndex가 중복되지 않는다는 가정이 필요함(현재 InternalIndex는 그냥 한없이 증가함, 중복 없음)
	// nullptr 설정 후 다른 객체에 이 InternalIndex를 할당하는 경우 문제가 생김
	// 이때는 SerialNumber를 따로 설정해줘야함.
	bool IsValid() const { return UObject::GetObjectFromIndex(Index); }

	T* Get() const { return static_cast<T*>(UObject::GetObjectFromIndex(Index)); }

	T* operator->() { return Get(); }

private:
	// 인덱스 재활용 안 한다고 가정
	uint32 Index = -1;
};
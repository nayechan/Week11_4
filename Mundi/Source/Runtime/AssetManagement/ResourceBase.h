#pragma once
#include <d3d11.h>
#include <filesystem>
#include "Object.h"
#include "UResourceBase.generated.h"

UCLASS(Abstract, DisplayName="리소스 베이스", Description="모든 리소스의 기본 클래스입니다")
class UResourceBase : public UObject
{
public:
	GENERATED_REFLECTION_BODY()

	UResourceBase() = default;
	virtual ~UResourceBase() {}

	const FString& GetFilePath() const { return FilePath; }
	void SetFilePath(const FString& InFilePath) { FilePath = InFilePath; }

	// Hot Reload Support
	std::filesystem::file_time_type GetLastModifiedTime() const { return LastModifiedTime; }
	void SetLastModifiedTime(std::filesystem::file_time_type InTime) { LastModifiedTime = InTime; }

protected:
	FString FilePath;	// 원본 파일의 경로이자, UResourceManager에 등록된 Key 
	std::filesystem::file_time_type LastModifiedTime;
};
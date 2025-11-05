#pragma once
#include "SceneComponent.h"


// 실제 파일을 로드한 USound
// 아직 USound가 없으므로 임시 객체
class USound;
struct IXAudio2SourceVoice;

class UAudioComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UAudioComponent, USceneComponent)
	GENERATED_REFLECTION_BODY();

	UAudioComponent();
	virtual ~UAudioComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;
	virtual void EndPlay() override;

	/**
	* @brief 이 컴포넌트에 할당된 Sound를 재생합니다.
	*/
	void Play();

	/**
	* @brief 현재 재생 중인 사운드를 정지합니다.
	*/
	void Stop();

	/**
	* @brief 재생할 사운드 리소스를 할당합니다.
	*/
	void SetSound(USound* NewSound) { Sound = NewSound; }

public:

	/** 재생할 사운드 리소스 */
	USound* Sound;

	/** 재생할 사운드 리소스의 볼륨 */
	float Volume;

	/** 재생할 사운드 리소스 높낮이 */
	float Pitch;

	/** 반복 재생 여부 */
	bool bIsLooping;
	/** 시작할 때 자동으로 재생시킬 지*/

	/** UI에서 설정하는 사운드 파일 경로(.wav) */
	bool bAutoPlay;

	// Duplication
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UAudioComponent)

protected:
	/** 현재 재생중인가*/
	bool bIsPlaying;

	/** 음원 재생기 like MP3 */
	IXAudio2SourceVoice* SourceVoice = nullptr;
};



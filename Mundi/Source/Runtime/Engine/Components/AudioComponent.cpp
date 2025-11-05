// Audio component that plays USound via FAudioDevice

#include "pch.h"
#include "AudioComponent.h"
#include "../GameFramework/FAudioDevice.h"
#include "../Audio/Sound.h"
#include "../../Core/Misc/PathUtils.h"
#include <xaudio2.h>
#include "ResourceManager.h"


IMPLEMENT_CLASS(UAudioComponent)

BEGIN_PROPERTIES(UAudioComponent)
    MARK_AS_COMPONENT("오디오 컴포넌트", "소리를 듣고 싶습니꽈?!#!!")
    ADD_PROPERTY_AUDIO(EPropertyType::Sound, Sound, "Sound", true)
    ADD_PROPERTY(float, Volume, "Audio", true, "Volume (0..1)")
    ADD_PROPERTY(float, Pitch,  "Audio", true, "Pitch (frequency ratio)")
    ADD_PROPERTY(bool,  bIsLooping, "Audio", true, "Loop playback")
    ADD_PROPERTY(bool,  bAutoPlay,  "Audio", true, "Auto play on BeginPlay")
END_PROPERTIES()

UAudioComponent::UAudioComponent()
    : Volume(1.0f)
    , Pitch(1.0f)
    , bIsLooping(false)
    , bAutoPlay(true)
    , bIsPlaying(false)
{
}

UAudioComponent::~UAudioComponent()
{
    Stop();
}

void UAudioComponent::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoPlay)
    {
        Play();
    }
}

void UAudioComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (bIsPlaying && SourceVoice)
    {
        FVector CurrentLocation = GetWorldLocation();
        FAudioDevice::UpdateSoundPosition(SourceVoice, CurrentLocation);

        if (!bIsLooping)
        {
            XAUDIO2_VOICE_STATE state{};
            SourceVoice->GetState(&state);
            if (state.BuffersQueued == 0)
            {
                Stop();
            }
        }
    }
}

void UAudioComponent::EndPlay()
{
    Super::EndPlay();
    Stop();
} 

void UAudioComponent::Play()
{
    USound* Selected = Sound;
   
    if (!Selected)
        return;

    if (bIsPlaying)
        return;

    FVector CurrentLocation = GetWorldLocation();
    SourceVoice = FAudioDevice::PlaySound3D(Selected, CurrentLocation, Volume, bIsLooping);

    if (SourceVoice)
    {
        SourceVoice->SetFrequencyRatio(Pitch);
        bIsPlaying = true;
    }
}

void UAudioComponent::Stop()
{
    if (!bIsPlaying)
        return;

    if (SourceVoice)
    {
        FAudioDevice::StopSound(SourceVoice);
        SourceVoice = nullptr;
    }
    bIsPlaying = false;
}

void UAudioComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}


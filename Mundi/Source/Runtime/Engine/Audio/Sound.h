#pragma once

#include "ResourceBase.h"
#include "Vector.h"
#include <mmreg.h>

// Minimal sound asset for PCM WAV playback
class USound : public UResourceBase
{
public:
    DECLARE_CLASS(USound, UResourceBase)

    USound() = default;
    ~USound() override = default;

    bool Load(const FString& InFilePath, ID3D11Device* /*UnusedDevice*/ = nullptr);
    void ReleaseResources();

    // Load 16-bit PCM WAV from disk
    bool LoadWavFromFile(const FWideString& FilePath);

    // Accessors
    const WAVEFORMATEX& GetWaveFormat() const { return WaveFormat; }
    const uint8*        GetPCMData()   const { return PCMData.empty() ? nullptr : PCMData.data(); }
    uint32              GetPCMSize()   const { return static_cast<uint32>(PCMData.size()); }
    float               GetDurationSec() const { return DurationSec; }
    const FWideString&  GetSourcePath() const { return SourcePath; }

private:
    WAVEFORMATEX  WaveFormat{};      // format description (PCM only in MVP)
    std::vector<uint8> PCMData;      // interleaved PCM16 samples
    float         DurationSec = 0.0f;
    FWideString   SourcePath;
};


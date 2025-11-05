#include "pch.h"
#include "Sound.h"
#include <fstream>
#include <filesystem>
#include "../../Core/Misc/PathUtils.h"

IMPLEMENT_CLASS(USound)

// Simple WAV reader for PCM (uncompressed) only
static bool ReadExact(std::ifstream& fs, void* dst, size_t bytes)
{
    fs.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(bytes));
    return fs.gcount() == static_cast<std::streamsize>(bytes);
}

bool USound::LoadWavFromFile(const FWideString& FilePath)
{
    using std::uint32_t; using std::uint16_t;

    SourcePath = FilePath;
    PCMData.clear();
    DurationSec = 0.0f;
    ZeroMemory(&WaveFormat, sizeof(WaveFormat));

    std::ifstream fs(std::filesystem::path(FilePath), std::ios::binary);
    if (!fs.is_open())
    {
        UE_LOG("[Audio] Failed to open WAV: %s", WideToUTF8(FilePath).c_str());
        return false;
    }

    struct RiffHeader { char id[4]; uint32_t size; char wave[4]; } hdr{};
    if (!ReadExact(fs, &hdr, sizeof(hdr))) return false;
    if (memcmp(hdr.id, "RIFF", 4) != 0 || memcmp(hdr.wave, "WAVE", 4) != 0)
    {
        UE_LOG("[Audio] Not a RIFF/WAVE file: %s", WideToUTF8(FilePath).c_str());
        return false;
    }

    bool haveFmt = false;
    bool haveData = false;
    std::vector<uint8> dataChunk;

    while (fs && (!haveFmt || !haveData))
    {
        struct ChunkHeader { char id[4]; uint32_t size; } ch{};
        if (!ReadExact(fs, &ch, sizeof(ch))) break;

        const std::streamoff chunkStart = fs.tellg();

        if (memcmp(ch.id, "fmt ", 4) == 0)
        {
            if (ch.size < sizeof(WAVEFORMATEX))
            {
                std::vector<uint8> tmp(ch.size);
                if (!ReadExact(fs, tmp.data(), tmp.size())) return false;
                ZeroMemory(&WaveFormat, sizeof(WaveFormat));
                memcpy(&WaveFormat, tmp.data(), tmp.size());
            }
            else
            {
                if (!ReadExact(fs, &WaveFormat, sizeof(WAVEFORMATEX))) return false;
                const uint32_t remain = ch.size - static_cast<uint32_t>(sizeof(WAVEFORMATEX));
                if (remain > 0) fs.seekg(remain, std::ios::cur);
            }

            if (WaveFormat.wFormatTag != WAVE_FORMAT_PCM)
            {
                UE_LOG("[Audio] Only PCM WAV supported. Tag=%u", (uint32)WaveFormat.wFormatTag);
                return false;
            }
            haveFmt = true;
        }
        else if (memcmp(ch.id, "data", 4) == 0)
        {
            if (ch.size > 0)
            {
                dataChunk.resize(ch.size);
                if (!ReadExact(fs, dataChunk.data(), dataChunk.size())) return false;
            }
            haveData = true;
        }
        else
        {
            fs.seekg(ch.size, std::ios::cur);
        }

        std::streamoff endPos = chunkStart + static_cast<std::streamoff>(ch.size);
        fs.seekg(endPos, std::ios::beg);
    }

    if (!haveFmt || !haveData)
    {
        UE_LOG("[Audio] WAV missing fmt or data chunk: %s", WideToUTF8(FilePath).c_str());
        return false;
    }

    PCMData = std::move(dataChunk);
    if (WaveFormat.nAvgBytesPerSec > 0)
    {
        DurationSec = static_cast<float>(static_cast<double>(PCMData.size()) / static_cast<double>(WaveFormat.nAvgBytesPerSec));
    }
    else
    {
        DurationSec = 0.0f;
    }

    return true;
}

void USound::ReleaseResources()
{
    PCMData.clear();
    ZeroMemory(&WaveFormat, sizeof(WaveFormat));
    DurationSec = 0.0f;
}

bool USound::Load(const FString& InFilePath, ID3D11Device* /*UnusedDevice*/)
{
    FWideString W = UTF8ToWide(InFilePath);
    return LoadWavFromFile(W);
}


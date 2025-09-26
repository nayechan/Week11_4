#pragma once
#include "Archive.h"
#include "UEContainer.h"
#include <fstream>

class FWindowsBinReader : public FArchive
{
public:
    FWindowsBinReader(const FString& Filename)
        : FArchive(true, false) // Loading 모드
    {
        File.open(Filename, std::ios::binary | std::ios::in);
    }
    ~FWindowsBinReader() { Close(); }

    void Serialize(void* Data, int64 Length) override
    {
        File.read(reinterpret_cast<char*>(Data), Length);
    }
    /*void Seek(size_t Position) override { File.seekg(Position); }
    size_t Tell() const override { return (size_t)File.tellg(); }*/
    bool Close() override
    {
        if (File.is_open()) { File.close(); return true; }
        return false;
    }

private:
    std::ifstream File;
};
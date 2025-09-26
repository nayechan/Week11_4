#include "pch.h"
#include "SceneLoader.h"

#include <algorithm>
#include <iomanip>

static bool ParsePerspectiveCamera(const JSON& Root, FPerspectiveCameraData& OutCam)
{
    if (!Root.hasKey("PerspectiveCamera"))
        return false;

    const JSON& Cam = Root.at("PerspectiveCamera");

    // 배열형 벡터(Location, Rotation) 파싱 (스칼라 실패 시 무시)
    auto readVec3 = [](JSON arr, FVector& outVec)
        {
            try
            {
                outVec = FVector(
                    (float)arr[0].ToFloat(),
                    (float)arr[1].ToFloat(),
                    (float)arr[2].ToFloat());
            }
            catch (...) {} // 실패 시 기본값 유지
        };

    if (Cam.hasKey("Location"))
        readVec3(Cam.at("Location"), OutCam.Location);
    if (Cam.hasKey("Rotation"))
        readVec3(Cam.at("Rotation"), OutCam.Rotation);

    // 스칼라 또는 [스칼라] 모두 허용
    auto readScalarFlexible = [](const JSON& parent, const char* key, float& outVal)
        {
            if (!parent.hasKey(key)) return;
            const JSON& node = parent.at(key);
            try
            {
                // 배열 형태 시도 (예: "FOV": [60.0])
                outVal = (float)node.at(0).ToFloat();
            }
            catch (...)
            {
                // 스칼라 (예: "FOV": 60.0)
                outVal = (float)node.ToFloat();
            }
        };

    readScalarFlexible(Cam, "FOV", OutCam.FOV);
    readScalarFlexible(Cam, "NearClip", OutCam.NearClip);
    readScalarFlexible(Cam, "FarClip", OutCam.FarClip);

    return true;
}

TArray<FPrimitiveData> FSceneLoader::Load(const FString& FileName, FPerspectiveCameraData* OutCameraData)
{
    std::ifstream file(FileName);
    if (!file.is_open())
    {
		UE_LOG("Scene load failed. Cannot open file: %s", FileName.c_str());
        return {};
    }

    std::stringstream Buffer;
    Buffer << file.rdbuf();
    std::string content = Buffer.str();

    try {
        JSON j = JSON::Load(content);

        // 카메라 먼저 파싱
        if (OutCameraData)
        {
            FPerspectiveCameraData Temp{};
            if (ParsePerspectiveCamera(j, Temp))
            {
                *OutCameraData = Temp;
            }
            else
            {
                // 카메라 블록이 없으면 값을 건드리지 않음
            }
        }

        return Parse(j);
    }
    catch (const std::exception& e) {
		UE_LOG("Scene load failed. JSON parse error: %s", e.what());
        return {};
    }
}

void FSceneLoader::Save(TArray<FPrimitiveData> InPrimitiveData, const FPerspectiveCameraData* InCameraData, const FString& SceneName)
{
    uint32 NextUUID = UObject::PeekNextUUID();

    namespace fs = std::filesystem;
    fs::path outPath(SceneName);
    if (!outPath.has_parent_path())
        outPath = fs::path("Scene") / outPath;
    if (outPath.extension().string() != ".Scene")
        outPath.replace_extension(".Scene");
    std::error_code ec;
    fs::create_directories(outPath.parent_path(), ec);

    auto NormalizePath = [](FString Path) -> FString
        {
            for (auto& ch : Path)
            {
                if (ch == '\\') ch = '/';
            }
            return Path;
        };

    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(6);

    auto writeVec3 = [&](const char* name, const FVector& v, int indent)
        {
            std::string tabs(indent, ' ');
            oss << tabs << "\"" << name << "\" : [" << v.X << ", " << v.Y << ", " << v.Z << "]";
        };

    oss << "{\n";
    oss << "  \"Version\" : 1,\n";
    oss << "  \"NextUUID\" : " << NextUUID;

    bool bHasCamera = (InCameraData != nullptr);

    if (bHasCamera)
    {
        oss << ",\n";
        oss << "  \"PerspectiveCamera\" : {\n";
        // 순서: FOV, FarClip, Location, NearClip, Rotation (FOV/Clip들은 단일 요소 배열)
        oss << "    \"FOV\" : [" << InCameraData->FOV << "],\n";
        oss << "    \"FarClip\" : [" << InCameraData->FarClip << "],\n";
        writeVec3("Location", InCameraData->Location, 4); oss << ",\n";
        oss << "    \"NearClip\" : [" << InCameraData->NearClip << "],\n";
        writeVec3("Rotation", InCameraData->Rotation, 4); oss << "\n";
        oss << "  }";
    }

    // Primitives 블록
    oss << (bHasCamera ? ",\n" : ",\n"); // 카메라 없더라도 컴마 후 줄바꿈
    oss << "  \"Primitives\" : {\n";
    for (size_t i = 0; i < InPrimitiveData.size(); ++i)
    {
        const FPrimitiveData& Data = InPrimitiveData[i];
        oss << "    \"" << Data.UUID << "\" : {\n";
        // 순서: Location, ObjStaticMeshAsset, Rotation, Scale, Type
        writeVec3("Location", Data.Location, 6); oss << ",\n";

        FString AssetPath = NormalizePath(Data.ObjStaticMeshAsset);
        oss << "      \"ObjStaticMeshAsset\" : " << "\"" << AssetPath << "\",\n";

        writeVec3("Rotation", Data.Rotation, 6); oss << ",\n";
        writeVec3("Scale", Data.Scale, 6); oss << ",\n";
        oss << "      \"Type\" : " << "\"" << Data.Type << "\"\n";
        oss << "    }" << (i + 1 < InPrimitiveData.size() ? "," : "") << "\n";
    }
    oss << "  }\n";
    oss << "}\n";

    const std::string finalPath = outPath.make_preferred().string();
    std::ofstream OutFile(finalPath.c_str(), std::ios::out | std::ios::trunc);
    if (OutFile.is_open())
    {
        OutFile << oss.str();
        OutFile.close();
    }
    else
    {
        UE_LOG("Scene save failed. Cannot open file: %s", finalPath.c_str());
    }
}

// ─────────────────────────────────────────────
// NextUUID 메타만 읽어오는 간단한 헬퍼
// 저장 포맷상 "NextUUID"는 "마지막으로 사용된 UUID"이므로,
// 호출 측에서 +1 해서 SetNextUUID 해야 함
// ─────────────────────────────────────────────
bool FSceneLoader::TryReadNextUUID(const FString& FilePath, uint32& OutNextUUID)
{
    std::ifstream file(FilePath);
    if (!file.is_open())
    {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    try
    {
        JSON j = JSON::Load(content);
        if (j.hasKey("NextUUID"))
        {
            // 정수 파서가 없으면 ToFloat로 받아서 캐스팅
			OutNextUUID = static_cast<uint32>(j.at("NextUUID").ToInt());
            return true;
        }
    }
    catch (...)
    {
        // 무시하고 false 반환
    }
    return false;
}

TArray<FPrimitiveData> FSceneLoader::Parse(const JSON& Json)
{
    TArray<FPrimitiveData> Primitives;

    if (!Json.hasKey("Primitives"))
    {
        std::cerr << "Primitives 섹션이 존재하지 않습니다." << std::endl;
        return Primitives;
    }

    auto PrimitivesJson = Json.at("Primitives");
    for (auto& kv : PrimitivesJson.ObjectRange())
    {
        // kv.first: 키(문자열), kv.second: 값(JSON 객체)
        const std::string& key = kv.first;
        const JSON& value = kv.second;

        FPrimitiveData data;

        // 키를 UUID로 파싱 (숫자가 아니면 0 유지)
        try
        {
            // 공백 제거 후 파싱
            std::string trimmed = key;
            trimmed.erase(std::remove_if(trimmed.begin(), trimmed.end(), ::isspace), trimmed.end());
            data.UUID = static_cast<uint32>(std::stoul(trimmed));
        }
        catch (...)
        {
            data.UUID = 0; // 레거시 호환: 숫자 키가 아니면 0
        }

        auto loc = value.at("Location");
        data.Location = FVector(
            (float)loc[0].ToFloat(),
            (float)loc[1].ToFloat(),
            (float)loc[2].ToFloat()
        );

        auto rot = value.at("Rotation");
        data.Rotation = FVector(
            (float)rot[0].ToFloat(),
            (float)rot[1].ToFloat(),
            (float)rot[2].ToFloat()
        );

        auto scale = value.at("Scale");
        data.Scale = FVector(
            (float)scale[0].ToFloat(),
            (float)scale[1].ToFloat(),
            (float)scale[2].ToFloat()
        );

        if (value.hasKey("ObjStaticMeshAsset"))
        {
            data.ObjStaticMeshAsset = value.at("ObjStaticMeshAsset").ToString();
        }
        else
        {
            data.ObjStaticMeshAsset = "";
        }

        data.Type = value.at("Type").ToString();

        Primitives.push_back(data);
    }

    return Primitives;
}

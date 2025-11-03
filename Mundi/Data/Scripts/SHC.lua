-- Fireball이 생성되는 위치를 정해주는 스크립트입니다
-- 현재는 X,Y의 바운드를 정할 수 있고, Z는 FireballArea Actor에서 가져옵니다.

local SpawnInterval = 1
local TimeAcc = 0.0

-- ===== 회전 제어 파라미터 =====
local AmplitudeDeg = 30.0     -- 시작 각도 기준 ±이 값(기본 45°)
local RotationSpeed = .1  -- 1초에 몇 번 왕복하는지(진동 빈도). 0.5 = 2초에 한 번 왕복
local RotTime = 0.0           -- 누적 시간
local BaseRotZ = nil          -- 시작 시 Z 기준 각도

-- ===== 전방(로컬 +X)을 월드로 변환하여 GlobalConfig에 기록 =====
local function ZRotFunction(rot)
    local rz = math.rad(rot.Z)
    local ry = math.rad(rot.Y)
    local rx = math.rad(rot.X)

    local cz, sz = math.cos(rz), math.sin(rz)
    local cy, sy = math.cos(ry), math.sin(ry)

    -- R = Rz * Ry * Rx 가정 시, 첫 번째 열이 로컬 +X의 월드 방향이 됨
    local rightX = cz * cy
    local rightY = sz * cy
    local rightZ = -sy

    -- 안전: 정규화(선택)
    local len = math.sqrt(rightX*rightX + rightY*rightY + rightZ*rightZ)
    if len > 1e-8 then
        rightX, rightY, rightZ = rightX/len, rightY/len, rightZ/len
    end

    if FireballSpawner then
        GlobalConfig.SpawnerForwardByUUID[FireballSpawner.UUID] = Vector(rightX, rightY, rightZ)
    end
end

function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)

    GlobalConfig.SpawnAreaPos = Obj.Location
    GlobalConfig.SpawnerForwardByUUID = GlobalConfig.SpawnerForwardByUUID or {}

    -- 기준 회전 저장(±AmplitudeDeg 범위의 중심)
    BaseRotZ = Obj.Rotation.Z

    -- 스폰너 생성 및 위치 맞추기
    FireballSpawner = SpawnPrefab("Data/Prefabs/FireballSpawnArea.prefab")
    FireballSpawner.Location = Obj.Location

    -- 초기 전방 기록
    ZRotFunction(Obj.Rotation)
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    -- Overlap hook if needed
end

function RandomInRange(MinRange, MaxRange)
    return MinRange + (MaxRange - MinRange) * math.random()
end

function Tick(dt)
    RotTime = RotTime + dt

    local level = GlobalConfig.CoachLevel or 1
    if level == 1 then
        RotationSpeed = .1 
    elseif level == 2 then
        RotationSpeed = .2
    elseif level == 3 then
        RotationSpeed = .3
    elseif level == 4 then
        RotationSpeed = .4
        

    -- 사인 진동으로 Z 각 계산: BaseRotZ ± AmplitudeDeg
    local zOffset = AmplitudeDeg * math.sin(2.0 * math.pi * RotationSpeed * RotTime)    
    local rot = Obj.Rotation
    rot.Z = BaseRotZ + zOffset
    Obj.Rotation = rot

    -- 스폰너 위치 동기(원하면 유지)
    if FireballSpawner then
        FireballSpawner.Location = Obj.Location
    end

    -- 스폰너 전방 갱신
    ZRotFunction(rot)
end

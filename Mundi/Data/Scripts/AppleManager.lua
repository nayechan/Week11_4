-- AppleManager.lua
local MaxAppleNumber = 10
local CurrentAppleNumber = 0
local ApplesPool = {}
local DestroyTime = 5.0

local function PushApple(apple)
    apple.bIsActive = false
    apple.Velocity = Vector(0,0,0)
    apple.Location = Vector(0,0,-1000)
    ApplesPool[#ApplesPool + 1] = apple
    CurrentAppleNumber = CurrentAppleNumber - 1
end

local function PopApple()
    if #ApplesPool == 0 then return nil end
    local apple = ApplesPool[#ApplesPool]
    ApplesPool[#ApplesPool] = nil
    apple.bIsActive = true
    CurrentAppleNumber = CurrentAppleNumber + 1
    return apple
end

function DestroyApple(apple)
    coroutine.yield("wait_time", DestroyTime)
    PushApple(apple)
end

function BeginPlay()
    print("[AppleManager BeginPlay] " .. Obj.UUID)
    for i = 1, MaxAppleNumber do
        local apple = SpawnPrefab("Data/Prefabs/Apple.prefab")
        CurrentAppleNumber = CurrentAppleNumber + 1
        if apple then PushApple(apple) end
    end

    -- 사과 생성 함수 등록
    GlobalConfig.SpawnAppleAt = function(Pos, Dir)
        if CurrentAppleNumber >= MaxAppleNumber then return false end
        local newApple = PopApple()
        if not newApple then return false end

        local speed = 30.0
        newApple.Location = Pos
        newApple.Velocity = Dir * speed
        newApple.bIsActive = true

        StartCoroutine(function() DestroyApple(newApple) end)
        return true
    end

    GlobalConfig.ResetApple = function(inactiveApple)
        PushApple(inactiveApple)
    end
end

function EndPlay()
    GlobalConfig.SpawnAppleAt = nil
    GlobalConfig.ResetApple = nil
end

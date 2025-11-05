-- Fireball 생성, 삭제를 관리해주는 스크립트입니다. 
local MaxFireNumber = 40
local CurrentFireNumber = 0
local FireballsPool = {} 
local AllFireballs = {}

local MinVelocity = 5
local MaxVelocity = 15

local DestroyTime = 6.0


local function PushFireball(Fireball) 
    if Fireball.bIsActive == false then
        return
    end


    Fireball.bIsActive = false
    Fireball.Velocity = Vector(0,0,0)
    --여기에 문제가 있는 것 같은,, 초기화 이슈 
    --Fireball.Location = GlobalConfig.SpawnAreaPos

    FireballsPool[#FireballsPool + 1] = Fireball

    CurrentFireNumber =  CurrentFireNumber - 1 
end 

local function PopFireball()
    local Count = #FireballsPool
    if Count == 0 then
        return nil
    end

    local Fireball = FireballsPool[Count]
    Fireball.bIsActive = true
    -- Velocity will be set by spawner using its front direction
    FireballsPool[Count] = nil
    
    CurrentFireNumber = CurrentFireNumber + 1
    return Fireball
end

function DestroyFireball(Fireball)
    coroutine.yield("wait_time", DestroyTime)
    PushFireball(Fireball)
end


function BeginPlay()
    

    for i = 1, MaxFireNumber do 
        local Fireball = SpawnPrefab("Data/Prefabs/Fireball.prefab")
        CurrentFireNumber = CurrentFireNumber + 1
        if Fireball then
            PushFireball(Fireball) 
        end

        AllFireballs[#AllFireballs + 1] = Fireball
    end

    -- Fireball 생성 함수를 전역에 등록    
    GlobalConfig.SpawnFireballAt = function(Pos, Dir)

        -- 최대 개수 조절
        if CurrentFireNumber >= MaxFireNumber then
            -- print("Full")
            return false
        end
        
        -- print(CurrentFireNumber)

        local newFireball = PopFireball()
        if newFireball ~= nil then
            newFireball.Location = Pos
            local speed = (MinVelocity + (MaxVelocity - MinVelocity) * math.random())
            if Dir == nil then
                Dir = Vector(0, 0, -1)
            end
            newFireball.Velocity = Dir * speed
            StartCoroutine(function()
            DestroyFireball(newFireball)
            end)
        end
 
        return true
    end 

        -- Fireball 생성 가능여부 함수를 전역에 등록
    GlobalConfig.IsCanSpawnFireball = function()
        return CurrentFireNumber < MaxFireNumber
    end  

    GlobalConfig.ResetFireballs = function(InactiveFireball)
        PushFireball(InactiveFireball)       
    end

    GlobalConfig.DestroyAllFireball = function()
        
        for i = 1, #AllFireballs do
            local fb = AllFireballs[i]
            if fb and fb.bIsActive == true then
                PushFireball(fb) 
            end
        end

    end 



end 

function EndPlay()
     
    -- Cleanup exposed functions
    GlobalConfig.SpawnFireballAt = nil
    GlobalConfig.IsCanSpawnFireball = nil
    GlobalConfig.ResetFireballs = nil
    GlobalConfig.DestroyAllFireball = nil
end

function Tick(dt)
    -- Manager could handle lifetime/cleanup here if needed
    
        
end

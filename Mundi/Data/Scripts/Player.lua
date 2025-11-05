UpVector = Vector(0, 0, 1)

local GroundHeight = 0
local VelocityZ = 0
local JumpSpeed = 6

local YawSensitivity        = 0.005
local PitchSensitivity      = 0.0025

local MovementDelta = 10

local GravityConst               = -9.8 * 0.4
local CurGravity            = 0
local bStart                = false
local bDie                  = false

local ActiveIDs = {}
local IDCount = 0

local PlayerInitPosition = Vector(0, 0, 4)
local PlayerInitVelocity = Vector(0, 0, 0)

local CameraLocation     = PlayerInitPosition
local ForwardVector      = Vector(1, 0, 0)

function AddID(id)
    if not ActiveIDs[id] then
        ActiveIDs[id] = true
        IDCount = IDCount + 1

        if CurGravity < 0 and not bStart then
            CurGravity = 0 
            bStart = true
            GroundHeight = Obj.Location.Z  
        end
    end
end

function RemoveID(id)
    if ActiveIDs[id] then
        ActiveIDs[id] = nil
        IDCount = IDCount - 1
        -- print("Removed ID:".. id.."Count:".. IDCount)
        
        if IDCount == 0 and (GroundHeight - 10) > Obj.Location.Z then
            Die()
        end
    end
end

------------------------------------------------------------
local function NormalizeCopy(V)
    local Out = Vector(V.X, V.Y, V.Z)
    Out:Normalize()
    return Out
end

local function RotateAroundAxis(VectorIn, Axis, Angle)
    local UnitAxis = NormalizeCopy(Axis)
    local CosA, SinA = math.cos(Angle), math.sin(Angle)
    local AxisCrossVector = FVector.Cross(UnitAxis, VectorIn)
    local AxisDotVector   = FVector.Dot(UnitAxis, VectorIn)
    
    return VectorIn * CosA + AxisCrossVector * SinA + UnitAxis * (AxisDotVector * (1.0 - CosA))
end

------------------------------------------------------------
function BeginPlay()  
    ActiveIDs = {}
    bDie = false
    CurGravity = GravityConst
    VelocityZ = 0

    Obj.Location = PlayerInitPosition
    Obj.Velocity = PlayerInitVelocity 
    
    local Camera = GetCamera()
    if Camera then
        Camera:SetCameraForward(ForwardVector)
    end

    ForwardVector = NormalizeCopy(ForwardVector)
end

function EndPlay()
    ActiveIDs = {}
end

function OnBeginOverlap(OtherActor)
    if OtherActor.Tag == "tile" then
        AddID(OtherActor.UUID)
    elseif OtherActor.Tag == "fireball" then
        Die()
    end
end

function OnEndOverlap(OtherActor)
    if OtherActor.Tag == "tile" then
        RemoveID(OtherActor.UUID)
    end
end

function Tick(Delta)
    PlayerMove(Delta)

    if not ManageGameState() then
        return
    end

    if InputManager:IsKeyDown('W') then MoveForward(MovementDelta * Delta) end
    if InputManager:IsKeyDown('S') then MoveForward(-MovementDelta * Delta) end
    if InputManager:IsKeyDown('A') then MoveRight(-MovementDelta * Delta) end
    if InputManager:IsKeyDown('D') then MoveRight(MovementDelta * Delta) end
    if InputManager:IsKeyPressed('Q') then Die() end -- 죽기를 선택
    if InputManager:IsKeyPressed(' ') then Jump(MovementDelta) end
    if InputManager:IsMouseButtonPressed(0) then ShootProjectile() end

    CameraMove()
    Rotate()
end

function PlayerMove(Delta)
    if GlobalConfig.GameState == "Playing" then
        
        if IsGrounded() then
            if VelocityZ < 0 then VelocityZ = 0 end
            if Obj.Location.Z < GroundHeight then Obj.Location.Z = GroundHeight + 1 end
        else
            VelocityZ = VelocityZ + (-10 * Delta)
        end
            
        local CurGravityAccel = Vector(0, 0, CurGravity)
        Obj.Velocity = CurGravityAccel * Delta +Vector(0,0,VelocityZ*Delta)
        Obj.Location = Obj.Location + Obj.Velocity
    end
end

function ShootProjectile()
    local projectile = SpawnPrefab("Data/Prefabs/Apple.prefab")
    if not projectile then
       
        return
    end

    -- 플레이어 기준 오프셋
    local forwardOffset = 1.5
    local upOffset = 1.2

    local UpVector = Vector(0, 0, 1)
    local speed = 30.0

    projectile.Location = Obj.Location + (ForwardVector * forwardOffset) + (UpVector * upOffset)
    projectile.Velocity = ForwardVector * speed
    projectile.bIsActive = true
end

------------------------------------------------------------
function ManageGameState()
    if GlobalConfig.GameState == "Playing" then
        if bDie then
            return false
        elseif IDCount == 0 and CurGravity == 0 and Obj.Location.Z < (GroundHeight - 5)  then
                Die()
            return false
        end
        return true

    elseif GlobalConfig.GameState == "End" then
        DeleteObject(Obj) 
    
    elseif GlobalConfig.GameState == "Init" then
        Rebirth()
    end

    return false
end

function Die()
    if bDie then 
        return
    end
 
    AudioComp = GetComponent(Obj, "UAudioComponent")
    AudioComp:PlayOneShot(0)   
    TargetHitStop(Obj, 0.5, 0)
     

    -- 전역 슬로모: 0.8초 동안 0.25배 속도
    -- (주: TargetHitStop은 Actor 포인터가 필요하며, 현재는 Obj(FGameObject)만 있으므로 전역으로 처리)
    --TargetHitStop(Obj, 0.5, 0)
    bDie = true
    VelocityZ = 0
    CurGravity = GravityConst
    local ActiveIDs = {}

    StartCoroutine(EndAfter)
end

function EndAfter()
    coroutine.yield("wait_time", 1)
    GlobalConfig.PlayerState = "Dead"
end

function Rebirth()
    ActiveIDs = {}
    bDie = false
    CurGravity = GravityConst
    VelocityZ = 0

    Obj.Location = PlayerInitPosition
    Obj.Velocity = PlayerInitVelocity
end
------------------------------------------------------------
function Rotate()
    local MouseDelta = InputManager:GetMouseDelta()
    local MouseDeltaX = MouseDelta.X
    local MouseDeltaY = MouseDelta.Y

    local Yaw = MouseDeltaX * YawSensitivity
    ForwardVector = RotateAroundAxis(ForwardVector, UpVector, Yaw)
    ForwardVector = NormalizeCopy(ForwardVector)

    local RightVector = FVector.Cross(UpVector, ForwardVector)
    RightVector = NormalizeCopy(RightVector)

    local Pitch = MouseDeltaY * PitchSensitivity
    local Candidate = RotateAroundAxis(ForwardVector, RightVector, Pitch)

    -- 수직 잠김 방지
    if (Candidate.Z > 0.4) then -- 아래로 각도 제한
        Candidate.Z = 0.4
    end
    if (Candidate.Z < -0.75) then -- 위로 각도 제한
        Candidate.Z = -0.75
    end

    ForwardVector = NormalizeCopy(Candidate)

    LootAt = Vector(-ForwardVector.X, -ForwardVector.Y, 0)
    SetPlayerForward(Obj, LootAt)
end

function IsGrounded()
    return IDCount > 0
end

function Jump(Delta)
    if IsGrounded() then
        VelocityZ = JumpSpeed
    end
end 

function MoveForward(Delta)
    Obj.Location = Obj.Location + Vector(ForwardVector.X,ForwardVector.Y, 0)  * Delta
end

function MoveRight(Delta)
    local RightVector = FVector.Cross(UpVector, ForwardVector)
    RightVector = NormalizeCopy(RightVector)
    Obj.Location = Obj.Location + Vector(RightVector.X,RightVector.Y, 0) * Delta
end

---------------------------------------------------------

function CameraMove()
    SetCamera()
    Billboard()
end

function SetCamera()
    local BackDistance = 7.0
    local UpDistance   = 2.0

    local Camera = GetCamera()
    if Camera then
        CameraLocation = Obj.Location + (ForwardVector * -BackDistance) + (UpVector * UpDistance)
        Camera:SetLocation(CameraLocation)
    end
end

function Billboard()
    local Camera = GetCamera()
    if Camera then
        local Eye = CameraLocation
        local At = Obj.Location
        local Direction = Vector(At.X - Eye.X, At.Y - Eye.Y, At.Z - Eye.Z)
        print(Direction)
        Camera:SetCameraForward(Direction)
    end
end

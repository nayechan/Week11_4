UpVector = Vector(0, 0, 1)

local YawSensitivity        = 0.005
local PitchSensitivity      = 0.0025
local PitchGuardDegrees     = 1.0
local VerticalDotLimit      = math.cos(math.rad(90 - PitchGuardDegrees)) -- ≈ cos(89°)

local MovementDelta = 0.1

local ForwardVector         = Vector(1, 0, 0)
local CameraLocation        = Vector(0, 0, 0)

local Gravity               = -1.0
local bGravity              = false
local bActive               = false

local ActiveIDs = {}
local IDCount = 0

local PlayerInitPosition = Vector(0, 0, 3)
local PlayerInitVelocity = Vector(0, 0, 0)

function AddID(id)
    if not ActiveIDs[id] then
        ActiveIDs[id] = true
        IDCount = IDCount + 1
        print("Added ID:".. id .. "Count:".. IDCount)
    end
end

function RemoveID(id)
    if ActiveIDs[id] then
        ActiveIDs[id] = nil
        IDCount = IDCount - 1
        print("Removed ID:".. id.."Count:".. IDCount)
        
        if IDCount == 0 then
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
    print("[BeginPlay] " .. Obj.UUID)
    Obj.Location = PlayerInitPosition
    Obj.Velocity = PlayerInitVelocity

    local Camera = GetCamera()
    if Camera then
        Camera:SetForward(ForwardVector)
    end

    ForwardVector = NormalizeCopy(ForwardVector)
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnBeginOverlap(OtherActor)
    if OtherActor.Tag == "tile" then
        AddID(OtherActor.UUID)
        if not bActive then
            bActive = true
        end
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
    local GravityAccel = Vector(0, 0, Gravity)
    Obj.Velocity = GravityAccel * Delta
    Obj.Location = Obj.Location + Obj.Velocity * Delta

    if not bActive then -- Not Started
        Gravity = -30
        MoveCamera()
        return

    elseif bGravity then -- and Active! => Die
        Gravity = -50
        
        if Obj.Location.Z < -5 then
            Rebirth()
        end

    elseif not bGravity then -- and Active!
        Gravity = 0.0
        MoveCamera()

        if IDCount == 0 then
            Die()
        end

        Rotate()

        if InputManager:IsKeyDown('W') then MoveForward(MovementDelta) end
        if InputManager:IsKeyDown('S') then MoveForward(-MovementDelta) end
        if InputManager:IsKeyDown('A') then MoveRight(-MovementDelta) end
        if InputManager:IsKeyDown('D') then MoveRight(MovementDelta) end
        if InputManager:IsKeyDown('E') then Die() end -- 죽기를 선택
    end
end

function Die()
    bGravity = true
    ActiveIDs = {}
end

function Rebirth()
    bActive = false
    bGravity = false
    Obj.Location = PlayerInitPosition
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
    if (Candidate.Z > 0.2) then -- 아래로 각도 제한
        Candidate.Z = 0.2
    end
    if (Candidate.Z < -0.6) then -- 위로 각도 제한
        Candidate.Z = -0.6
    end

    ForwardVector = NormalizeCopy(Candidate)
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

function MoveCamera()
    SetCamera()
    Billboard()
end

function Billboard()
    local Camera = GetCamera()
    if Camera then
        local Eye = CameraLocation
        local At = Obj.Location
        local Direction = Vector(At.X - Eye.X, At.Y - Eye.Y, At.Z - Eye.Z)
        Camera:SetForward(Direction)
    end
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
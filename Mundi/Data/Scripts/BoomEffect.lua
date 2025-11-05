function BeginPlay()
    BillboardComponent = GetComponent(Obj, "UBillboardComponent")
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnBeginOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function OnEndOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)
     -- 1. 카메라 액터를 가져옵니다.
    CameraActor = GetCamera()
    if not CameraActor then return end

    print("Bill TIck")
    
    -- 2. 카메라의 위치와 정면 벡터를 가져옵니다.
    local CamPosition = CameraActor:GetActorLocation()
    local CamForward = CameraActor:GetActorRight() -- C++에서 바인딩한 새 함수

    -- 3. 최종 위치 계산: 카메라 위치 + (카메라 정면 방향 * 거리)
    Obj.Location = CamPosition + (CamForward * 2)
 
end
function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    Obj:PrintLocation()
    
    -- Co = coroutine.create(AI)
end

function AI()
    print("AI start")
    coroutine.yield("wait_time", 1.0)
    print("Patrol End")
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
    Obj:PrintLocation()
end

function OnOverlap(OtherActor)
    OtherActor:PrintLocation();
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
end
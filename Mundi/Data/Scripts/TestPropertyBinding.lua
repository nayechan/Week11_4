function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    Obj.Velocity.X = 0.0
    Obj.Velocity.Y = 0.0
    Obj.Velocity.Z = 0.0

    A = GetComponent(Obj, "UPointLightComponent")
    A.Intensity = 100.0

    A = AddComponent(Obj, "USpotLightComponent")

    print("Yes Binding!")
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end
function BeginPlay()
    
end

function EndPlay()
    
end

function OnBeginOverlap(OtherActor)
    if OtherActor.Tag == "fireball" then
        if GlobalConfig and GlobalConfig.RemoveTileByUUID then
            GlobalConfig.RemoveTileByUUID(Obj.UUID)
            GetCameraManager():StartCameraShake(0.02, 0.1, 0.1, 40)
        end
    end
end

function OnEndOverlap(OtherActor)
end
function Tick(dt) 
    Obj.Location = Obj.Location + Obj.Velocity * dt * 0.01
end
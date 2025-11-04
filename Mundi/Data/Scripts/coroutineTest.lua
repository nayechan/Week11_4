-- StartCoroutine : Cpp API, Coroutine 생성

function BeginPlay() 
    Obj.Velocity.X = 6
    StartCoroutine(Gravity)
    
end

function Gravity()
    coroutine.yield("wait_predicate", function()
        return GlobalConfig.Gravity2 ==4
    end)
    
end

function AI()
     
    coroutine.yield("wait_time", 1.0)
    
end

function AI2()
     
    coroutine.yield("wait_predicate", function()
        return Obj.Velocity.X <= 5
    end)
    
end

function EndPlay()
   
end


function OnEndOverlap(OtherActor)
end

function OnBeginOverlap(OtherActor)
    Obj.Velocity.X = 0
     
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    if GlobalConfig and GlobalConfig.Gravity2 == 5 then
        GlobalConfig.Gravity2 = 4;
    end
end
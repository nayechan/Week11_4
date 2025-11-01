---@class Vector
---@field X number
---@field Y number
---@field Z number

---@class GameObject
---@field UUID string
---@field Location Vector
---@field Velocity Vector
---@field PrintLocation fun(self: GameObject)

---@type GameObject
Obj = Obj  -- C++에서 주입되지만, LSP에 타입 힌트 제공 && Lua에서 자동완성 가능
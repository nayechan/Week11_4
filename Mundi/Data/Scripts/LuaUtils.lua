-- AnimHelpers.lua
-- Common helper functions for animation scripts

-- ========================================
-- Variables Wrapper Helper
-- ========================================
-- Creates a metatable wrapper for TMap<FString, FString>
-- Auto converts between Lua types and strings
--
-- Usage:
--   self.Vars = CreateVarsWrapper(self.Variables)
--   self.Vars["Speed"] = 300.0  -- Auto converted to "300.0"
--   local speed = self.Vars["Speed"]  -- Auto converted to 300.0
-- ========================================

function CreateVarsWrapper(variablesMap)
    if not variablesMap then
        error("[LuaUtils] CreateVarsWrapper: variablesMap is nil")
    end

    local wrapper = setmetatable({}, {
        __index = function(t, key)
            local str = variablesMap[key]
            if not str then return nil end

            -- Try number conversion first
            local num = tonumber(str)
            if num then return num end

            -- Try boolean conversion
            if str == "true" then return true end
            if str == "false" then return false end

            -- Return as string
            return str
        end,
        __newindex = function(t, key, value)
            -- Auto convert to string and store in C++ TMap
            variablesMap[key] = tostring(value)
        end
    })

    return wrapper
end

print("[AnimHelpers] AnimHelpers.lua loaded successfully")

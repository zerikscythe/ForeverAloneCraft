-- LivingWorld Control Panel

local CMD_LW    = ".lw "
local CMD_LWBOT = ".lwbot "

-- -----------------------------------------------
-- State
-- -----------------------------------------------
local LW_Roster     = { [0] = { name = "Party" } }  -- slot 0 = party broadcast
local LW_SlotNum    = 0    -- default to Party
local LW_SlotMax    = 0    -- grows as roster entries arrive
local LW_ProfileNum = 1
local LW_LogLevel   = 1

-- Returns the bot reference string for the current slot.
-- Slot 0 → "party", any other slot → character name (or slot number as fallback).
local function GetBotRef()
    if LW_SlotNum == 0 then return "party" end
    local entry = LW_Roster[LW_SlotNum]
    return entry and entry.name or tostring(LW_SlotNum)
end

-- Returns true when Party is the active selection (commands that require a
-- single target should be suppressed).
local function IsPartySelected()
    return LW_SlotNum == 0
end

-- -----------------------------------------------
-- Roster display helpers
-- -----------------------------------------------
function LWCP_UpdateSlotDisplay()
    local entry = LW_Roster[LW_SlotNum]
    LWCPSlotLabel:SetText(entry and entry.name or "---")
end

-- Called from OnEvent when a CHAT_MSG_SYSTEM arrives.
-- Matches roster list lines:  [1] Tester lvl 80 Mage (account-alt) online
function LWCP_HandleSystemMsg(msg)
    local slot, name = string.match(msg, "%[(%d+)%]%s+(%S+)%s+lvl")
    if not slot then return end
    if string.lower(name) == string.lower(UnitName("player")) then return end
    local n = tonumber(slot)
    LW_Roster[n] = { name = name }
    if n > LW_SlotMax then LW_SlotMax = n end
    LWCP_UpdateSlotDisplay()
end

-- Clears the cached roster then re-issues .lwbot list.
-- Entries repopulate via LWCP_HandleSystemMsg as chat lines arrive.
function LWCP_RefreshRoster()
    LW_Roster  = { [0] = { name = "Party" } }
    LW_SlotMax = 0
    LWCP_UpdateSlotDisplay()
    SendChatMessage(CMD_LWBOT .. "list")
end

-- -----------------------------------------------
-- Slot selector
-- -----------------------------------------------
function LWCP_SlotInc()
    local start = LW_SlotNum
    repeat
        LW_SlotNum = LW_SlotNum + 1
        if LW_SlotNum > LW_SlotMax then LW_SlotNum = 0 end
    until LW_Roster[LW_SlotNum] or LW_SlotNum == start
    LWCP_UpdateSlotDisplay()
end

function LWCP_SlotDec()
    local start = LW_SlotNum
    repeat
        LW_SlotNum = LW_SlotNum - 1
        if LW_SlotNum < 0 then LW_SlotNum = LW_SlotMax end
    until LW_Roster[LW_SlotNum] or LW_SlotNum == start
    LWCP_UpdateSlotDisplay()
end

-- -----------------------------------------------
-- Roster commands
-- -----------------------------------------------
function LWCP_Spawn()
    if IsPartySelected() then return end
    SendChatMessage(CMD_LWBOT .. "request " .. LW_SlotNum)
end

function LWCP_Dismiss()
    if IsPartySelected() then return end
    SendChatMessage(CMD_LWBOT .. "dismiss " .. LW_SlotNum)
end

-- -----------------------------------------------
-- Combat profile selector
-- -----------------------------------------------
function LWCP_ProfileInc()
    LW_ProfileNum = LW_ProfileNum + 1
    if LW_ProfileNum > 10 then LW_ProfileNum = 1 end
    LWCPProfileLabel:SetText("Profile " .. LW_ProfileNum)
end

function LWCP_ProfileDec()
    LW_ProfileNum = LW_ProfileNum - 1
    if LW_ProfileNum < 1 then LW_ProfileNum = 10 end
    LWCPProfileLabel:SetText("Profile " .. LW_ProfileNum)
end

function LWCP_SetProfile()
    SendChatMessage(CMD_LWBOT .. GetBotRef() .. " profile " .. LW_ProfileNum)
end

-- -----------------------------------------------
-- Combat overrides  (always party-wide)
-- -----------------------------------------------
function LWCP_Attack()
    SendChatMessage(CMD_LWBOT .. GetBotRef() .. " attack")
end

function LWCP_Disengage()
    SendChatMessage(CMD_LWBOT .. GetBotRef() .. " disengage")
end

function LWCP_Retreat()
    SendChatMessage(CMD_LWBOT .. GetBotRef() .. " retreat")
end

-- -----------------------------------------------
-- Cast command
-- -----------------------------------------------
local function GetSpell()
    local spell = LWCPSpellBox:GetText()
    if not spell or spell == "" then
        DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Type a spell name in the box first.")
        return nil
    end
    return spell
end

function LWCP_CastOnTarget()
    if IsPartySelected() then DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Select a specific bot to cast.") return end
    local spell = GetSpell()
    if spell then SendChatMessage(CMD_LWBOT .. LW_SlotNum .. " cast " .. spell .. " on mytarget") end
end

function LWCP_CastOnMe()
    if IsPartySelected() then DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Select a specific bot to cast.") return end
    local spell = GetSpell()
    if spell then SendChatMessage(CMD_LWBOT .. LW_SlotNum .. " cast " .. spell .. " on me") end
end

function LWCP_CastOnFocus()
    if IsPartySelected() then DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Select a specific bot to cast.") return end
    local spell = GetSpell()
    if spell then SendChatMessage(CMD_LWBOT .. LW_SlotNum .. " cast " .. spell .. " on focus") end
end

function LWCP_CastOnSelf()
    if IsPartySelected() then DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Select a specific bot to cast.") return end
    local spell = GetSpell()
    if spell then SendChatMessage(CMD_LWBOT .. LW_SlotNum .. " cast " .. spell .. " on yourself") end
end

-- -----------------------------------------------
-- Follow / Refreshments / Buff
-- -----------------------------------------------
function LWCP_Follow()
    SendChatMessage(CMD_LWBOT .. GetBotRef() .. " follow")
end

function LWCP_Refreshments()
    SendChatMessage(CMD_LWBOT .. GetBotRef() .. " refreshments")
end

function LWCP_Buff()
    SendChatMessage(CMD_LWBOT .. GetBotRef() .. " buff")
end

-- -----------------------------------------------
-- Log level
-- -----------------------------------------------
function LWCP_LogInc()
    LW_LogLevel = LW_LogLevel + 1
    if LW_LogLevel > 4 then LW_LogLevel = 1 end
    LWCPLogLabel:SetText("Level: " .. LW_LogLevel)
    SendChatMessage(CMD_LW .. "loglevel " .. LW_LogLevel)
end

function LWCP_LogDec()
    LW_LogLevel = LW_LogLevel - 1
    if LW_LogLevel < 1 then LW_LogLevel = 4 end
    LWCPLogLabel:SetText("Level: " .. LW_LogLevel)
    SendChatMessage(CMD_LW .. "loglevel " .. LW_LogLevel)
end

-- -----------------------------------------------
-- Frame toggle
-- -----------------------------------------------
function LWCP_Close()
    LWCPFrame:Hide()
end

SLASH_LWCP1 = "/lwcp"
SlashCmdList["LWCP"] = function()
    if LWCPFrame:IsVisible() then
        LWCPFrame:Hide()
    else
        LWCPFrame:Show()
    end
end

-- -----------------------------------------------
-- Minimap button
-- -----------------------------------------------
local LWCPButtonAngle = 220

function LWCPButton_UpdatePosition()
    LWCPButtonFrame:SetPoint(
        "TOPLEFT", "Minimap", "TOPLEFT",
        54 - (78 * cos(LWCPButtonAngle)),
        (78 * sin(LWCPButtonAngle)) - 55
    )
end

function LWCPButton_BeingDragged()
    local xpos, ypos = GetCursorPosition()
    local xmin, ymin = Minimap:GetLeft(), Minimap:GetBottom()
    xpos = xmin - xpos / UIParent:GetScale() + 70
    ypos = ypos / UIParent:GetScale() - ymin - 70
    local angle = math.deg(math.atan2(ypos, xpos))
    if angle < 0 then angle = angle + 360 end
    LWCPButtonAngle = angle
    LWCPButton_UpdatePosition()
end

function LWCPButton_OnClick()
    if LWCPFrame:IsVisible() then
        LWCPFrame:Hide()
    else
        LWCPFrame:Show()
    end
end

function LWCPButton_OnEnter()
    GameTooltip:SetOwner(this, "ANCHOR_LEFT")
    GameTooltip:SetText("LivingWorld Control Panel\nLeft-click to open/close\nRight-drag to move")
    GameTooltip:Show()
end

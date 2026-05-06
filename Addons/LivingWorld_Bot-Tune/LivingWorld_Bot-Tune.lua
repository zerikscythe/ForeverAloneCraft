local LWBT_BUILD = "shell-v1"
local CMD_LWBOT = ".lwbot "

local LWBT_State = {
    ownedBots = {},
    selectedBot = nil,
}

local function NormalizeBotName(name)
    if not name then
        return nil
    end

    local shortName = string.match(name, "([^%-]+)") or name
    return string.lower(shortName)
end

local function ClearOwnedBots()
    for key in pairs(LWBT_State.ownedBots) do
        LWBT_State.ownedBots[key] = nil
    end
end

local function RefreshOwnedBots()
    ClearOwnedBots()
    SendChatMessage(CMD_LWBOT .. "list")
end

local function GetTargetBotRecord()
    if not UnitExists("target") or UnitIsUnit("target", "player") then
        return nil
    end

    local targetName = UnitName("target")
    if not targetName then
        return nil
    end

    return LWBT_State.ownedBots[NormalizeBotName(targetName)]
end

local function UpdateSelectedBot()
    LWBT_State.selectedBot = GetTargetBotRecord()
end

local function BuildBotMetaText(bot)
    local parts = {}

    if bot.level then
        parts[#parts + 1] = "Level " .. bot.level
    end

    if bot.className then
        parts[#parts + 1] = bot.className
    end

    if bot.sourceName then
        parts[#parts + 1] = bot.sourceName
    end

    if bot.statusText then
        parts[#parts + 1] = bot.statusText
    end

    return table.concat(parts, " | ")
end

function LWBT_Render()
    if not LWBTFrame then
        return
    end

    local bot = LWBT_State.selectedBot
    if not bot then
        LWBTBlankState:Show()
        LWBTDetailState:Hide()
        return
    end

    LWBTBlankState:Hide()
    LWBTDetailState:Show()

    LWBTOwnedBotName:SetText(bot.displayName or "Unknown Bot")
    LWBTOwnedBotMeta:SetText(BuildBotMetaText(bot))

    LWBTMainRotationBody:SetText(
        "Profile API not wired yet.\n\n"
        .. "This section will show the selected bot's main rotation rows,"
        .. " priorities, targets, and conditions once the server read path is added.")

    LWBTGlobalBreaksBody:SetText(
        "Profile API not wired yet.\n\n"
        .. "This section will show emergency / interrupt / cast-break rules"
        .. " such as heals, dispels, and defensive breaks.")
end

local function ParseRosterLine(message)
    local slot, name, level, className, sourceName, statusText =
        string.match(
            message,
            "%[(%d+)%]%s+(%S+)%s+lvl%s+(%d+)%s+(%S+)%s+%(([^%)]+)%)%s*(.*)")

    if not slot or not name then
        return false
    end

    if NormalizeBotName(name) == NormalizeBotName(UnitName("player")) then
        return false
    end

    local key = NormalizeBotName(name)
    LWBT_State.ownedBots[key] = {
        slot = tonumber(slot),
        displayName = name,
        level = tonumber(level),
        className = className,
        sourceName = sourceName,
        statusText = statusText ~= "" and statusText or nil,
    }

    return true
end

local function HandleSystemMessage(message)
    if not message then
        return
    end

    if ParseRosterLine(message) then
        UpdateSelectedBot()
        LWBT_Render()
    end
end

local function HandlePlayerTargetChanged()
    UpdateSelectedBot()
    LWBT_Render()
end

local function HandlePlayerLogin()
    DEFAULT_CHAT_FRAME:AddMessage("|cff33ccffLWBT:|r addon build " .. LWBT_BUILD .. " loaded.")
end

function LWBT_ToggleFrame()
    if not LWBTFrame then
        return
    end

    if LWBTFrame:IsVisible() then
        LWBTFrame:Hide()
        return
    end

    LWBTFrame:Show()
    RefreshOwnedBots()
    UpdateSelectedBot()
    LWBT_Render()
end

function LWBT_Close()
    if LWBTFrame then
        LWBTFrame:Hide()
    end
end

function LWBT_Refresh()
    RefreshOwnedBots()
    UpdateSelectedBot()
    LWBT_Render()
end

function LWBT_OnLoad(self)
    self:RegisterEvent("PLAYER_LOGIN")
    self:RegisterEvent("CHAT_MSG_SYSTEM")
    self:RegisterEvent("PLAYER_TARGET_CHANGED")
end

function LWBT_OnEvent(self, event, ...)
    if event == "PLAYER_LOGIN" then
        HandlePlayerLogin()
        return
    end

    if event == "CHAT_MSG_SYSTEM" then
        HandleSystemMessage(...)
        return
    end

    if event == "PLAYER_TARGET_CHANGED" then
        HandlePlayerTargetChanged()
    end
end

function LWBT_OnShow()
    RefreshOwnedBots()
    UpdateSelectedBot()
    LWBT_Render()
end

SLASH_LWBT1 = "/lwbtune"
SLASH_LWBT2 = "/lwbottune"
SlashCmdList["LWBT"] = function()
    LWBT_ToggleFrame()
end

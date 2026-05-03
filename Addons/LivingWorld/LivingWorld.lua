-- LivingWorld Control Panel

local LWCP_BUILD = "trainer-ui-t7"
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
local LW_ActiveTab  = "Bots"
local LW_QuestMode  = "SMART"
local LW_QuestRewards = {}
local LW_QuestActions = {}
local LW_QuestActionsReady = false
local LW_TrainActions = {}
local LW_TrainBotGold = {}
local LW_TrainOwnerGold = 0
local LW_TrainActionsReady = false
local LW_TrainPendingActions = {}
local LW_TrainPendingBotGold = {}
local LW_TrainPendingOwnerGold = 0
local LW_PendingTrainerAlert = false

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
-- Tab switching
-- -----------------------------------------------
local LW_Tabs = { "Bots", "Combat", "NPC", "Gear", "Bags", "Settings" }

function LWCP_ShowTab(name)
    if (name == "Gear" or name == "Bags") and not LWCP_CanOpenInventoryPanels() then
        name = "Bots"
    end

    LW_ActiveTab = name

    for _, t in ipairs(LW_Tabs) do
        local page = _G["LWCPPage" .. t]
        local btn  = _G["LWCPTab"  .. t] or (t == "Settings" and LWCPSettingsBtn)
        if t == name then
            page:Show()
            if btn then btn:Disable() end
        else
            page:Hide()
            if btn then btn:Enable() end
        end
    end

    if name ~= "NPC" and LWCP_HideTrainRows then
        LWCP_HideTrainRows()
    end

    if name == "Gear" then
        LWCP_RequestBotBags(true)
        LWCP_RenderGearTab()
    elseif name == "NPC" then
        if not LW_TrainActionsReady then
            LWCP_RequestTrainActions()
        end
        LWCP_RequestQuestRewards(true)
        LWCP_RequestQuestActions()
        LWCP_RenderQuestsTab()
    elseif name == "Bags" then
        LW_SelectedBagIdx = 0
        LWCP_RenderBagsTab()
    elseif name == "Settings" then
        LWCP_RequestQuestRewards(true)
        LWCP_UpdateQuestModeButtons()
    end

    LWCP_UpdateInventoryTabState()
end

function LWCP_RefreshActivePage()
    if LW_ActiveTab == "Gear" or LW_ActiveTab == "Bags" then
        LWCP_RequestBotBags()
    elseif LW_ActiveTab == "NPC" or LW_ActiveTab == "Settings" then
        LWCP_RequestQuestRewards(true)
        if LW_ActiveTab == "NPC" then
            LWCP_RequestTrainActions()
            LWCP_RequestQuestActions()
        end
    else
        LWCP_RefreshRoster()
    end
end

-- -----------------------------------------------
-- Roster display helpers
-- -----------------------------------------------
function LWCP_UpdateSlotDisplay()
    local entry = LW_Roster[LW_SlotNum]
    LWCPSlotLabel:SetText(entry and entry.name or "---")
    LWCP_UpdateInventoryTabState()
    if LWCPPageNPC and LWCPPageNPC:IsVisible() then
        LWCP_RenderQuestsTab()
    end
end

function LWCP_IsSelectedBotInParty()
    local entry = LW_Roster[LW_SlotNum]
    if not entry or not entry.name or LW_SlotNum == 0 then
        return false
    end

    local selectedName = string.lower(entry.name)
    for i = 1, GetNumPartyMembers() do
        local partyName = UnitName("party" .. i)
        if partyName and string.lower(partyName) == selectedName then
            return true
        end
    end

    return false
end

function LWCP_CanOpenInventoryPanels()
    return not IsPartySelected() and LWCP_IsSelectedBotInParty()
end

function LWCP_UpdateInventoryTabState()
    local enabled = LWCP_CanOpenInventoryPanels()

    if not enabled and (LW_ActiveTab == "Gear" or LW_ActiveTab == "Bags") then
        LWCP_ShowTab("Bots")
        return
    end

    local tabs = { "Gear", "Bags" }
    for _, name in ipairs(tabs) do
        local btn = _G["LWCPTab" .. name]
        if btn then
            btn:SetAlpha(enabled and 1 or 0.45)
            if enabled and LW_ActiveTab ~= name then
                btn:Enable()
            else
                btn:Disable()
            end
        end
    end
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

function LWCP_RequestQuestRewards(silent)
    SendChatMessage(CMD_LWBOT .. "quests")
end

function LWCP_SetQuestMode(mode)
    if not mode then return end
    SendChatMessage(CMD_LWBOT .. "questmode " .. string.lower(mode))
end

function LWCP_ChooseQuestReward(botName, questId, choiceNumber)
    if not botName or not questId or not choiceNumber then return end
    SendChatMessage(CMD_LWBOT .. botName .. " reward " .. questId .. " " .. choiceNumber)
end

function LWCP_RequestQuestActions()
    SendChatMessage(CMD_LWBOT .. "questactions")
end

function LWCP_RequestTrainActions()
    SendChatMessage(CMD_LWBOT .. "trainactions")
end

function LWCP_BotPickupQuest(botName, questId)
    if not botName or not questId then return end
    SendChatMessage(CMD_LWBOT .. botName .. " pickup " .. questId)
end

function LWCP_BotTurninQuest(botName, questId)
    if not botName or not questId then return end
    SendChatMessage(CMD_LWBOT .. botName .. " turnin " .. questId)
end

function LWCP_BotTrainSpell(botName, trainerSpellId)
    if not botName or not trainerSpellId then return end
    SendChatMessage(CMD_LWBOT .. botName .. " trainspell " .. trainerSpellId)
end

function LWCP_BotTrainAll(botName)
    if not botName then return end
    SendChatMessage(CMD_LWBOT .. botName .. " trainall")
end

function LWCP_HandleTargetChanged()
    LW_PendingTrainerAlert = true
    LWCP_RequestTrainActions()

    if LW_ActiveTab == "NPC" and LWCPPageNPC and LWCPPageNPC:IsVisible() then
        LWCP_RequestQuestActions()
    end
end

function LWCP_HandleTrainerShow()
    LW_PendingTrainerAlert = true
    LWCP_RequestTrainActions()
    if LW_ActiveTab == "NPC" and LWCPPageNPC and LWCPPageNPC:IsVisible() then
        LWCP_RenderQuestsTab()
    end
end

function LWCP_HandleTrainerClosed()
    LW_PendingTrainerAlert = false
end

function LWCP_HandlePlayerLogin()
    DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r addon build " .. LWCP_BUILD .. " loaded.")
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

function LWCP_Yoink()
    SendChatMessage(CMD_LWBOT .. GetBotRef() .. " yoink")
end

function LWCP_Train()
    if IsPartySelected() then
        DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Select a specific bot to train.")
        return
    end

    SendChatMessage(CMD_LWBOT .. GetBotRef() .. " train")
end

function LWCP_TrainSelectedAtTarget()
    if IsPartySelected() then
        DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Select a specific bot to train at the targeted trainer.")
        return
    end

    LWCP_BotTrainAll(GetBotRef())
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
-- Combat overrides
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
SlashCmdList["LWCP"] = function(msg)
    msg = string.lower(msg or "")
    msg = string.gsub(msg, "^%s+", "")
    msg = string.gsub(msg, "%s+$", "")
    if msg == "train" or msg == "trainer" or msg == "skills" then
        DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r trainer command received (" .. LWCP_BUILD .. ").")
        if LWCP_ToggleTrainerWindow then
            LWCP_ToggleTrainerWindow()
        elseif LWCP_ShowTrainerWindow then
            LWCP_ShowTrainerWindow()
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Trainer window is not ready yet.")
        end
        return
    end

    if LWCPFrame:IsVisible() then
        LWCPFrame:Hide()
    else
        LWCPFrame:Show()
    end
end

SLASH_LWTRAIN1 = "/lwtrain"
SlashCmdList["LWTRAIN"] = function()
    if LWCP_ToggleTrainerWindow then
        LWCP_ToggleTrainerWindow()
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

-- -----------------------------------------------
-- Quests tab
-- -----------------------------------------------
local LW_QuestRows = {}
local LW_QuestActionRows = {}
local LW_TrainRows = {}
local LW_TRAIN_ROW_COUNT = 10
local LW_LastVisibleTrainCount = 0
local LWCP_FormatMoney
local LW_TrainerDebugFrame = nil
local LW_TrainerDebugRows = {}
local LW_TRAIN_DEBUG_ROW_COUNT = 10

local function LWCP_CreateTrainRow(rowIdx, frame)
    local parent = LWCPFrame or frame
    local row = CreateFrame("Frame", "LWCPNPCTrainDynRow" .. rowIdx, parent)
    row:SetWidth(216)
    row:SetHeight(20)
    row:SetFrameStrata("DIALOG")
    row:SetFrameLevel(parent:GetFrameLevel() + 40)
    row:SetAlpha(1)
    row:EnableMouse(false)

    row.bg = row:CreateTexture(nil, "BACKGROUND")
    row.bg:SetAllPoints(row)
    row.bg:SetTexture(0, 0.25, 0.05, 0.55)

    row.label = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    row.label:SetPoint("LEFT", row, "LEFT", 4, 0)
    row.label:SetWidth(112)
    row.label:SetJustifyH("LEFT")

    row.costText = row:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    row.costText:SetPoint("RIGHT", row, "RIGHT", -54, 0)
    row.costText:SetWidth(44)
    row.costText:SetJustifyH("RIGHT")

    row.actionBtn = CreateFrame("Button", "LWCPNPCTrainDynBtn" .. rowIdx, row, "UIPanelButtonTemplate")
    row.actionBtn:SetWidth(50)
    row.actionBtn:SetHeight(18)
    row.actionBtn:SetPoint("RIGHT", row, "RIGHT", -2, 0)
    row.actionBtn:SetText("Learn")
    row.actionBtn:SetFrameStrata("DIALOG")
    row.actionBtn:SetFrameLevel(row:GetFrameLevel() + 1)
    row.actionBtn:SetScript("OnClick", function(self)
        if not self.botName or not self.trainerSpellId then return end
        LWCP_BotTrainSpell(self.botName, self.trainerSpellId)
    end)
    row.actionBtn:SetScript("OnEnter", function(self)
        if not self.spellLabel then return end
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText(self.spellLabel .. "\n|cffaaaaaaCost: " .. LWCP_FormatMoney(self.cost or 0) .. "|r")
        GameTooltip:Show()
    end)
    row.actionBtn:SetScript("OnLeave", function() GameTooltip:Hide() end)

    row:Hide()
    return row
end

local function LWCP_SetTrainRowPoint(row, offsetY)
    row:ClearAllPoints()
    if LWCPFrame then
        row:SetPoint("TOP", LWCPFrame, "TOP", 0, offsetY)
    elseif LWCPPageNPC then
        row:SetPoint("TOP", LWCPPageNPC, "TOP", 0, offsetY + 100)
    end
end

function LWCP_HideTrainRows()
    for _, row in ipairs(LW_TrainRows) do
        row:Hide()
    end
end

local function LWCP_UpdateNPCDebugText()
    if not LWCPNPCDebugText then
        return
    end

    LWCPNPCDebugText:SetText(
        LWCP_BUILD .. " | TA rows: " .. #LW_TrainActions ..
        " | vis: " .. LW_LastVisibleTrainCount ..
        " | pending: " .. #LW_TrainPendingActions ..
        " | ui: " .. #LW_TrainRows ..
        " | ready: " .. (LW_TrainActionsReady and "yes" or "no"))
end

LWCP_FormatMoney = function(amount)
    amount = tonumber(amount) or 0
    local gold = math.floor(amount / 10000)
    local silver = math.floor(math.mod(amount, 10000) / 100)
    local copper = math.mod(amount, 100)

    local parts = {}
    if gold > 0 then parts[#parts + 1] = gold .. "g" end
    if silver > 0 then parts[#parts + 1] = silver .. "s" end
    if copper > 0 or #parts == 0 then parts[#parts + 1] = copper .. "c" end
    return table.concat(parts, " ")
end

local function LWCP_BuildTrainerFallbackText(trainActions)
    local lines = {}
    for idx, data in ipairs(trainActions) do
        if idx > 6 then
            lines[#lines + 1] = "... " .. (#trainActions - 6) .. " more"
            break
        end

        local label = data.spellLabel or "Spell"
        if data.botName then
            label = data.botName .. " - " .. label
        end
        lines[#lines + 1] = label .. "  " .. LWCP_FormatMoney(data.cost)
    end

    return table.concat(lines, "\n")
end

local function LWCP_EnsureTrainerDebugWindow()
    if LW_TrainerDebugFrame then
        return LW_TrainerDebugFrame
    end

    local frame = CreateFrame("Frame", "LWCPTrainDebugFrame", UIParent)
    frame:SetWidth(380)
    frame:SetHeight(318)
    frame:SetPoint("CENTER", UIParent, "CENTER", 260, 0)
    frame:SetFrameStrata("FULLSCREEN_DIALOG")
    frame:SetFrameLevel(200)
    frame:SetMovable(true)
    frame:EnableMouse(true)
    frame:RegisterForDrag("LeftButton")
    frame:SetScript("OnDragStart", function(self) self:StartMoving() end)
    frame:SetScript("OnDragStop", function(self) self:StopMovingOrSizing() end)
    frame:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
        tile = true,
        tileSize = 16,
        edgeSize = 24,
        insets = { left = 6, right = 6, top = 6, bottom = 6 }
    })
    frame:SetBackdropColor(0, 0, 0, 0.92)

    frame.title = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightLarge")
    frame.title:SetPoint("TOP", frame, "TOP", 0, -18)
    frame.title:SetText("LivingWorld Trainer Data")

    frame.status = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    frame.status:SetPoint("TOP", frame.title, "BOTTOM", 0, -8)
    frame.status:SetWidth(340)
    frame.status:SetJustifyH("CENTER")

    frame.empty = frame:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    frame.empty:SetPoint("TOP", frame.status, "BOTTOM", 0, -34)
    frame.empty:SetWidth(320)
    frame.empty:SetJustifyH("CENTER")
    frame.empty:SetText("No trainer actions received yet.\nOpen a trainer, then click Refresh.")

    frame.closeBtn = CreateFrame("Button", "LWCPTrainDebugClose", frame, "UIPanelButtonTemplate")
    frame.closeBtn:SetWidth(70)
    frame.closeBtn:SetHeight(22)
    frame.closeBtn:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -22, 18)
    frame.closeBtn:SetText("Close")
    frame.closeBtn:SetScript("OnClick", function() frame:Hide() end)

    frame.refreshBtn = CreateFrame("Button", "LWCPTrainDebugRefresh", frame, "UIPanelButtonTemplate")
    frame.refreshBtn:SetWidth(70)
    frame.refreshBtn:SetHeight(22)
    frame.refreshBtn:SetPoint("RIGHT", frame.closeBtn, "LEFT", -10, 0)
    frame.refreshBtn:SetText("Refresh")
    frame.refreshBtn:SetScript("OnClick", function()
        LWCP_RequestTrainActions()
        LWCP_RenderTrainerDebugWindow()
    end)

    for rowIdx = 1, LW_TRAIN_DEBUG_ROW_COUNT do
        local row = CreateFrame("Frame", "LWCPTrainDebugRow" .. rowIdx, frame)
        row:SetWidth(334)
        row:SetHeight(22)
        row:SetPoint("TOP", frame, "TOP", 0, -66 - ((rowIdx - 1) * 22))

        row.bg = row:CreateTexture(nil, "BACKGROUND")
        row.bg:SetAllPoints(row)
        row.bg:SetTexture(0, 0.18, 0.05, 0.45)

        row.label = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        row.label:SetPoint("LEFT", row, "LEFT", 4, 0)
        row.label:SetWidth(210)
        row.label:SetJustifyH("LEFT")

        row.costText = row:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
        row.costText:SetPoint("LEFT", row.label, "RIGHT", 4, 0)
        row.costText:SetWidth(52)
        row.costText:SetJustifyH("RIGHT")

        row.actionBtn = CreateFrame("Button", "LWCPTrainDebugLearn" .. rowIdx, row, "UIPanelButtonTemplate")
        row.actionBtn:SetWidth(58)
        row.actionBtn:SetHeight(18)
        row.actionBtn:SetPoint("RIGHT", row, "RIGHT", -2, 0)
        row.actionBtn:SetText("Learn")
        row.actionBtn:SetScript("OnClick", function(self)
            if not self.botName or not self.trainerSpellId then return end
            LWCP_BotTrainSpell(self.botName, self.trainerSpellId)
        end)
        row.actionBtn:SetScript("OnEnter", function(self)
            if not self.spellLabel then return end
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetText(self.spellLabel .. "\n|cffaaaaaa" .. (self.botName or "Unknown") .. "\nCost: " .. LWCP_FormatMoney(self.cost or 0) .. "|r")
            GameTooltip:Show()
        end)
        row.actionBtn:SetScript("OnLeave", function() GameTooltip:Hide() end)

        row:Hide()
        LW_TrainerDebugRows[rowIdx] = row
    end

    frame:Hide()
    LW_TrainerDebugFrame = frame
    return frame
end

function LWCP_RenderTrainerDebugWindow()
    local frame = LWCP_EnsureTrainerDebugWindow()
    local targetName = UnitName("target") or "no target"
    frame.status:SetText(LWCP_BUILD .. " | target: " .. targetName .. " | actions: " .. #LW_TrainActions .. " | pending: " .. #LW_TrainPendingActions)

    if #LW_TrainActions == 0 then
        frame.empty:Show()
    else
        frame.empty:Hide()
    end

    for rowIdx, row in ipairs(LW_TrainerDebugRows) do
        local data = LW_TrainActions[rowIdx]
        if data then
            local combinedGold = (LW_TrainBotGold[data.botName] or 0) + (LW_TrainOwnerGold or 0)
            local canAfford = combinedGold >= (data.cost or 0)
            row.label:SetText((data.botName or "Unknown") .. " - " .. (data.spellLabel or "Spell"))
            row.costText:SetText(LWCP_FormatMoney(data.cost))
            row.actionBtn.botName = data.botName
            row.actionBtn.trainerSpellId = data.trainerSpellId
            row.actionBtn.spellLabel = data.spellLabel
            row.actionBtn.cost = data.cost
            row.actionBtn:SetAlpha(canAfford and 1 or 0.55)
            if canAfford then
                row.actionBtn:Enable()
            else
                row.actionBtn:Disable()
            end
            row:Show()
        else
            row.label:SetText("")
            row.costText:SetText("")
            row.actionBtn.botName = nil
            row.actionBtn.trainerSpellId = nil
            row.actionBtn.spellLabel = nil
            row.actionBtn.cost = nil
            row:Hide()
        end
    end
end

function LWCP_ShowTrainerWindow()
    local frame = LWCP_EnsureTrainerDebugWindow()
    LWCP_RenderTrainerDebugWindow()
    frame:Show()
end

function LWCP_ToggleTrainerWindow()
    local frame = LWCP_EnsureTrainerDebugWindow()
    if frame:IsVisible() then
        frame:Hide()
    else
        LWCP_ShowTrainerWindow()
    end
end

local function LWCP_GetVisibleTrainActions()
    if LW_SlotNum == 0 then
        return LW_TrainActions
    end

    local visible = {}
    local entry = LW_Roster[LW_SlotNum]
    local selectedName = entry and entry.name
    if not selectedName then
        return visible
    end

    local selectedNameLower = string.lower(selectedName)
    for _, action in ipairs(LW_TrainActions) do
        if action.botName and string.lower(action.botName) == selectedNameLower then
            visible[#visible + 1] = action
        end
    end

    if #visible == 0 and #LW_TrainActions > 0 then
        return LW_TrainActions
    end

    return visible
end

local function LWCP_GetVisibleQuestRewards()
    if LW_SlotNum == 0 then
        return LW_QuestRewards
    end

    local visible = {}
    local entry = LW_Roster[LW_SlotNum]
    local selectedName = entry and entry.name
    if not selectedName then
        return visible
    end

    local selectedNameLower = string.lower(selectedName)
    for _, reward in ipairs(LW_QuestRewards) do
        if reward.botName and string.lower(reward.botName) == selectedNameLower then
            visible[#visible + 1] = reward
        end
    end
    return visible
end

local function LWCP_GetVisibleQuestActions()
    if LW_SlotNum == 0 then
        return LW_QuestActions
    end

    local visible = {}
    local entry = LW_Roster[LW_SlotNum]
    local selectedName = entry and entry.name
    if not selectedName then
        return visible
    end

    local selectedNameLower = string.lower(selectedName)
    for _, action in ipairs(LW_QuestActions) do
        if action.botName and string.lower(action.botName) == selectedNameLower then
            visible[#visible + 1] = action
        end
    end
    return visible
end

function LWCP_UpdateQuestModeButtons()
    if LWCPQuestModeLabel then
        LWCPQuestModeLabel:SetText("Mode: " .. string.upper(LW_QuestMode or "SMART"))
    end

    if LWCPQuestModeSmart then
        if LW_QuestMode == "SMART" then
            LWCPQuestModeSmart:Disable()
        else
            LWCPQuestModeSmart:Enable()
        end
    end

    if LWCPQuestModeManual then
        if LW_QuestMode == "MANUAL" then
            LWCPQuestModeManual:Disable()
        else
            LWCPQuestModeManual:Enable()
        end
    end
end

function LWCP_InitQuestsPage(frame)
    if #LW_TrainRows == 0 then
        for rowIdx = 1, LW_TRAIN_ROW_COUNT do
            local row = _G["LWCPNPCTrainRow" .. rowIdx]
            if not row then
                row = LWCP_CreateTrainRow(rowIdx, frame)
            else
                row.label = _G[row:GetName() .. "Label"]
                row.costText = _G[row:GetName() .. "Cost"]
                row.actionBtn = _G[row:GetName() .. "Button"]
                row:SetFrameStrata("DIALOG")
                row:SetFrameLevel((LWCPFrame or frame):GetFrameLevel() + 40)
                if row.actionBtn then
                    row.actionBtn:SetFrameStrata("DIALOG")
                    row.actionBtn:SetFrameLevel(row:GetFrameLevel() + 1)
                    row.actionBtn:SetText("Learn")
                    row.actionBtn:SetScript("OnClick", function(self)
                        if not self.botName or not self.trainerSpellId then return end
                        LWCP_BotTrainSpell(self.botName, self.trainerSpellId)
                    end)
                    row.actionBtn:SetScript("OnEnter", function(self)
                        if not self.spellLabel then return end
                        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
                        GameTooltip:SetText(self.spellLabel .. "\n|cffaaaaaaCost: " .. LWCP_FormatMoney(self.cost or 0) .. "|r")
                        GameTooltip:Show()
                    end)
                    row.actionBtn:SetScript("OnLeave", function() GameTooltip:Hide() end)
                end
                row:Hide()
            end

            LW_TrainRows[rowIdx] = row
        end
    end

    -- Quest action rows (pick up / turn in from targeted NPC)
    if #LW_QuestActionRows == 0 then
        for rowIdx = 1, 8 do
            local row = CreateFrame("Frame", "LWCPQuestActionRow" .. rowIdx, frame)
            row:SetWidth(216)
            row:SetHeight(20)

            row.label = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
            row.label:SetPoint("LEFT", row, "LEFT", 4, 0)
            row.label:SetWidth(138)
            row.label:SetJustifyH("LEFT")

            local btnName = "LWCPQuestActionBtn" .. rowIdx
            row.actionBtn = CreateFrame("Button", btnName, row, "UIPanelButtonTemplate")
            row.actionBtn:SetWidth(68)
            row.actionBtn:SetHeight(18)
            row.actionBtn:SetPoint("RIGHT", row, "RIGHT", -2, 0)
            row.actionBtn:SetScript("OnClick", function(self)
                if not self.botName or not self.questId then return end
                if self.actionType == "PICKUP" then
                    LWCP_BotPickupQuest(self.botName, self.questId)
                elseif self.actionType == "TURNIN" then
                    LWCP_BotTurninQuest(self.botName, self.questId)
                end
            end)
            row.actionBtn:SetScript("OnEnter", function(self)
                if not self.questTitle then return end
                GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
                local tip = self.questTitle
                if self.hasChoices then
                    tip = tip .. "\n|cffaaaaaa(has reward choices)|r"
                end
                GameTooltip:SetText(tip)
                GameTooltip:Show()
            end)
            row.actionBtn:SetScript("OnLeave", function() GameTooltip:Hide() end)

            row:Hide()
            LW_QuestActionRows[rowIdx] = row
        end
    end

    -- Quest reward rows (pending choices)
    if #LW_QuestRows == 0 then
        for rowIdx = 1, 5 do
            local row = CreateFrame("Frame", "LWCPQuestRow" .. rowIdx, frame)
            row:SetWidth(216)
            row:SetHeight(48)
            if rowIdx == 1 then
                row:SetPoint("TOP", frame, "TOP", 0, -56)
            else
                row:SetPoint("TOP", LW_QuestRows[rowIdx - 1], "BOTTOM", 0, -6)
            end

            row.botText = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
            row.botText:SetPoint("TOPLEFT", row, "TOPLEFT", 4, -2)
            row.botText:SetWidth(208)
            row.botText:SetJustifyH("LEFT")

            row.questText = row:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
            row.questText:SetPoint("TOPLEFT", row.botText, "BOTTOMLEFT", 0, -2)
            row.questText:SetWidth(208)
            row.questText:SetJustifyH("LEFT")

            row.emptyText = row:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
            row.emptyText:SetPoint("LEFT", row, "LEFT", 6, -6)
            row.emptyText:SetText("")

            row.choiceButtons = {}
            for choiceIdx = 1, 6 do
                local btnName = "LWCPQuestChoice" .. rowIdx .. "_" .. choiceIdx
                local btn = CreateFrame("Button", btnName, row, "LWCPItemSlotTemplate")
                btn:SetWidth(26)
                btn:SetHeight(26)
                if choiceIdx == 1 then
                    btn:SetPoint("BOTTOMLEFT", row, "BOTTOMLEFT", 4, 2)
                else
                    btn:SetPoint("LEFT", row.choiceButtons[choiceIdx - 1], "RIGHT", 4, 0)
                end

                btn.choiceNumber = nil
                btn.itemId = nil
                btn.count = _G[btnName .. "Count"]
                btn.icon = _G[btnName .. "Icon"]
                btn.hl = _G[btnName .. "Highlight"]

                btn:SetScript("OnEnter", function(self)
                    if not self.itemId then return end
                    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
                    GameTooltip:SetHyperlink("item:" .. self.itemId .. ":0:0:0:0:0:0:0")
                    if self.count and self.countValue and self.countValue > 1 then
                        GameTooltip:AddLine("Count: " .. self.countValue, 0.7, 0.7, 0.7)
                        GameTooltip:Show()
                    end
                end)
                btn:SetScript("OnLeave", function() GameTooltip:Hide() end)
                btn:SetScript("OnClick", function(self)
                    if not self.botName or not self.questId or not self.choiceNumber then return end
                    LWCP_ChooseQuestReward(self.botName, self.questId, self.choiceNumber)
                end)

                row.choiceButtons[choiceIdx] = btn
                btn:Hide()
            end

            row:Hide()
            LW_QuestRows[rowIdx] = row
        end
    end
end

function LWCP_RenderQuestsTab()
    if (#LW_TrainRows == 0 or #LW_QuestActionRows == 0 or #LW_QuestRows == 0) and LWCPPageNPC then
        LWCP_InitQuestsPage(LWCPPageNPC)
    end

    local trainActions = LWCP_GetVisibleTrainActions()
    local actions = LWCP_GetVisibleQuestActions()
    local rewards = LWCP_GetVisibleQuestRewards()
    LW_LastVisibleTrainCount = #trainActions
    LWCP_UpdateNPCDebugText()
    LWCP_UpdateQuestModeButtons()

    local currentTop = 10
    local hasTrainActions = #trainActions > 0

    if hasTrainActions and LWCPQuestsEmptyText then
        LWCPQuestsEmptyText:Hide()
    end

    if LWCPTrainActionsHeader then
        if hasTrainActions then
            LWCPTrainActionsHeader:ClearAllPoints()
            LWCPTrainActionsHeader:SetPoint("TOP", LWCPPageNPC, "TOP", 0, -currentTop)
            if #trainActions > #LW_TrainRows then
                LWCPTrainActionsHeader:SetText("-- Trainer Actions (" .. #LW_TrainRows .. "/" .. #trainActions .. ") --")
            else
                LWCPTrainActionsHeader:SetText("-- Trainer Actions --")
            end
            LWCPTrainActionsHeader:Show()
            currentTop = currentTop + 20
        else
            LWCPTrainActionsHeader:Hide()
        end
    end

    for rowIdx, row in ipairs(LW_TrainRows) do
            LWCP_SetTrainRowPoint(row, -currentTop)
        local data = trainActions[rowIdx]
        if data then
            local combinedGold = (LW_TrainBotGold[data.botName] or 0) + (LW_TrainOwnerGold or 0)
            local canAfford = combinedGold >= (data.cost or 0)
            local labelText = data.spellLabel or "Spell"
            local entry = LW_Roster[LW_SlotNum]
            local selectedName = entry and entry.name
            if IsPartySelected() or not selectedName or (data.botName and string.lower(data.botName) ~= string.lower(selectedName)) then
                labelText = (data.botName or "Unknown") .. " - " .. labelText
            end
            row.label:SetText(labelText)
            if row.costText then row.costText:SetText(LWCP_FormatMoney(data.cost)) end
            row.actionBtn:SetText("Learn")
            row.actionBtn.botName = data.botName
            row.actionBtn.trainerSpellId = data.trainerSpellId
            row.actionBtn.spellLabel = data.spellLabel
            row.actionBtn.cost = data.cost
            row.actionBtn:SetAlpha(canAfford and 1 or 0.55)
            if canAfford then
                row.actionBtn:Enable()
            else
                row.actionBtn:Disable()
            end
            row:Show()
            currentTop = currentTop + 22
        else
            row.label:SetText("")
            if row.costText then row.costText:SetText("") end
            row.actionBtn.botName = nil
            row.actionBtn.trainerSpellId = nil
            row.actionBtn.spellLabel = nil
            row.actionBtn.cost = nil
            row.actionBtn:SetText("")
            row.actionBtn:SetAlpha(1)
            row.actionBtn:Enable()
            row:Hide()
        end
    end

    if hasTrainActions then
        if LWCPQuestsEmptyText then
            LWCPQuestsEmptyText:ClearAllPoints()
            LWCPQuestsEmptyText:SetPoint("TOP", LWCPPageNPC, "TOP", 0, -42)
            LWCPQuestsEmptyText:SetHeight(150)
            LWCPQuestsEmptyText:SetText(LWCP_BuildTrainerFallbackText(trainActions))
            LWCPQuestsEmptyText:Show()
        end

        if LWCPQuestActionsHeader then
            LWCPQuestActionsHeader:Hide()
        end

        if LWCPQuestRewardsHeader then
            LWCPQuestRewardsHeader:Hide()
        end

        for _, row in ipairs(LW_QuestActionRows) do
            row.label:SetText("")
            row.actionBtn.botName = nil
            row.actionBtn.questId = nil
            row.actionBtn.questTitle = nil
            row.actionBtn.actionType = nil
            row.actionBtn.hasChoices = nil
            row.actionBtn:SetText("")
            row:Hide()
        end

        for _, row in ipairs(LW_QuestRows) do
            row.botText:SetText("")
            row.questText:SetText("")
            row.emptyText:SetText("")
            for _, btn in ipairs(row.choiceButtons) do
                btn.botName = nil
                btn.questId = nil
                btn.choiceNumber = nil
                btn.itemId = nil
                btn.countValue = nil
                btn.icon:SetTexture(nil)
                btn.icon:SetAlpha(0)
                if btn.count then btn.count:SetText("") end
                btn:Hide()
            end
            row:Hide()
        end

        if LWCPNPCTrainAll then
            local canTrainAll = not IsPartySelected() and #trainActions > 0
            LWCPNPCTrainAll:SetAlpha(canTrainAll and 1 or 0.55)
            if canTrainAll then
                LWCPNPCTrainAll:Enable()
            else
                LWCPNPCTrainAll:Disable()
            end
        end

        return
    end

    if LWCPNPCTrainAll then
        local canTrainAll = not IsPartySelected() and #trainActions > 0
        LWCPNPCTrainAll:SetAlpha(canTrainAll and 1 or 0.55)
        if canTrainAll then
            LWCPNPCTrainAll:Enable()
        else
            LWCPNPCTrainAll:Disable()
        end
    end

    -- Render quest action rows
    local hasActions = #actions > 0

    if LWCPQuestActionsHeader then
        if hasActions then
            LWCPQuestActionsHeader:ClearAllPoints()
            LWCPQuestActionsHeader:SetPoint("TOP", LWCPPageNPC, "TOP", 0, -currentTop)
            LWCPQuestActionsHeader:Show()
            currentTop = currentTop + 20
        else
            LWCPQuestActionsHeader:Hide()
        end
    end

    for rowIdx, row in ipairs(LW_QuestActionRows) do
        row:ClearAllPoints()
        row:SetPoint("TOP", LWCPPageNPC, "TOP", 0, -currentTop)
        local data = actions[rowIdx]
        if data then
            local labelText = data.botName .. " - " .. data.questTitle
            row.label:SetText(labelText)

            local btn = row.actionBtn
            btn.botName = data.botName
            btn.questId = data.questId
            btn.questTitle = data.questTitle
            btn.actionType = data.actionType
            btn.hasChoices = data.hasChoices

            if data.actionType == "TURNIN" then
                btn:SetText("Turn In")
            else
                btn:SetText("Pick Up")
            end

            row:Show()
            currentTop = currentTop + 22
        else
            row.label:SetText("")
            row.actionBtn.botName = nil
            row.actionBtn.questId = nil
            row.actionBtn.questTitle = nil
            row.actionBtn.actionType = nil
            row.actionBtn.hasChoices = nil
            row.actionBtn:SetText("")
            row:Hide()
        end
    end

    -- Position reward rows below quest action rows
    local actionsBottom = currentTop

    if LWCPQuestRewardsHeader then
        LWCPQuestRewardsHeader:ClearAllPoints()
        LWCPQuestRewardsHeader:SetPoint("TOP", LWCPPageNPC, "TOP", 0, -actionsBottom)
        if #rewards > 0 then
            LWCPQuestRewardsHeader:Show()
        else
            LWCPQuestRewardsHeader:Hide()
        end
    end

    local rewardsStartY = actionsBottom + 16

    -- Empty text shown only when both sections are empty
    if LWCPQuestsEmptyText then
        if #trainActions == 0 and #actions == 0 and #rewards == 0 then
            LWCPQuestsEmptyText:SetText(
                (LW_TrainActionsReady or LW_QuestActionsReady)
                    and "No NPC actions available.\nTarget a quest giver or trainer to see options."
                    or "No pending NPC actions.")
            LWCPQuestsEmptyText:Show()
        else
            LWCPQuestsEmptyText:Hide()
        end
    end

    -- Render reward choice rows
    for rowIdx, row in ipairs(LW_QuestRows) do
        row:ClearAllPoints()
        if rowIdx == 1 then
            row:SetPoint("TOP", LWCPPageNPC, "TOP", 0, -rewardsStartY)
        else
            row:SetPoint("TOP", LW_QuestRows[rowIdx - 1], "BOTTOM", 0, -6)
        end

        local data = rewards[rowIdx]
        if data then
            row:Show()
            row.botText:SetText(data.botName)
            row.questText:SetText("[" .. data.questId .. "] " .. data.questTitle)
            row.emptyText:SetText("")

            for choiceIdx, btn in ipairs(row.choiceButtons) do
                local choice = data.choices[choiceIdx]
                if choice then
                    btn.botName = data.botName
                    btn.questId = data.questId
                    btn.choiceNumber = choice.choiceNumber
                    btn.itemId = choice.itemId
                    btn.countValue = choice.count

                    local _, _, _, _, _, _, _, _, _, icon = GetItemInfo(choice.itemId)
                    btn.icon:SetTexture(icon or "Interface\\Icons\\INV_Misc_QuestionMark")
                    btn.icon:SetAlpha(icon and 1 or 0.6)
                    if btn.count then
                        if choice.count and choice.count > 1 then
                            btn.count:SetText(choice.count)
                        else
                            btn.count:SetText("")
                        end
                    end
                    btn:Show()
                else
                    btn.botName = nil
                    btn.questId = nil
                    btn.choiceNumber = nil
                    btn.itemId = nil
                    btn.countValue = nil
                    btn.icon:SetTexture(nil)
                    btn.icon:SetAlpha(0)
                    if btn.count then btn.count:SetText("") end
                    btn:Hide()
                end
            end
        else
            row:Hide()
            row.botText:SetText("")
            row.questText:SetText("")
            row.emptyText:SetText("")
            for _, btn in ipairs(row.choiceButtons) do
                btn.botName = nil
                btn.questId = nil
                btn.choiceNumber = nil
                btn.itemId = nil
                btn.countValue = nil
                btn.icon:SetTexture(nil)
                btn.icon:SetAlpha(0)
                if btn.count then btn.count:SetText("") end
                btn:Hide()
            end
        end
    end
end

-- -----------------------------------------------
-- Gear tab — inventory state
-- -----------------------------------------------
local LW_GearData      = {}
local LW_BagData       = {}
local LW_SelectedBagIdx = 0

-- Equipment slot display order (left-to-right, top-to-bottom across 3 rows of 7):
--   Row 1: Head Neck Shoulders Back Chest Shirt Tabard
--   Row 2: Wrists Hands Waist Legs Feet Ring1 Ring2
--   Row 3: Trinket1 Trinket2 MainHand OffHand Ranged
local GEAR_SLOT_ORDER = {
    0, 1, 2, 14, 4, 3, 18,
    8, 9, 5, 6, 7, 10, 11,
    12, 13, 15, 16, 17,
}

local GEAR_SLOT_ICON = {
    [0]  = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Head",
    [1]  = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Neck",
    [2]  = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Shoulder",
    [3]  = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Shirt",
    [4]  = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Chest",
    [5]  = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Waist",
    [6]  = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Legs",
    [7]  = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Feet",
    [8]  = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Wrist",
    [9]  = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Hands",
    [10] = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Finger",
    [11] = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Finger",
    [12] = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Trinket",
    [13] = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Trinket",
    [14] = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Back",
    [15] = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-MainHand",
    [16] = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-SecondaryHand",
    [17] = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Ranged",
    [18] = "Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-Tabard",
    [19] = "Interface\\Buttons\\Button-Backpack-Up",
    [20] = "Interface\\Buttons\\Button-Backpack-Up",
    [21] = "Interface\\Buttons\\Button-Backpack-Up",
    [22] = "Interface\\Buttons\\Button-Backpack-Up",
}

local GEAR_SLOT_NAMES = {
    [0]="Head", [1]="Neck", [2]="Shoulders", [3]="Shirt", [4]="Chest",
    [5]="Waist", [6]="Legs", [7]="Feet", [8]="Wrists", [9]="Hands",
    [10]="Ring 1", [11]="Ring 2", [12]="Trinket 1", [13]="Trinket 2",
    [14]="Back", [15]="Main Hand", [16]="Off Hand", [17]="Ranged/Wand",
    [18]="Tabard", [19]="Bag 1", [20]="Bag 2", [21]="Bag 3", [22]="Bag 4",
}

-- Pick-and-place state: left-click a bag item to pick it, then left-click a
-- gear slot to equip it on the bot. Right-click a bag item to retrieve it.
local LW_PickedGuidLow = nil
local LW_PickedBtn     = nil
local LW_RetrievePromptGuidLow = nil
local LW_RetrievePromptMax     = nil
local LW_HoveredBagItemBtn = nil

local function LWCP_ClearPick()
    if LW_PickedBtn and LW_PickedBtn.hl then
        LW_PickedBtn.hl:Hide()
    end
    LW_PickedGuidLow = nil
    LW_PickedBtn     = nil
end

local function LWCP_PickItem(btn)
    LWCP_ClearPick()
    LW_PickedGuidLow = btn.guidLow
    LW_PickedBtn     = btn
    btn.hl:Show()
end

local function LWCP_HideComparisonTooltips()
    if ShoppingTooltip1 then ShoppingTooltip1:Hide() end
    if ShoppingTooltip2 then ShoppingTooltip2:Hide() end
end

local function LWCP_ShowTooltipComparisonLines(itemId)
    if not IsShiftKeyDown() or not itemId then
        return
    end

    local _, _, _, _, _, _, _, _, equipLoc = GetItemInfo(itemId)
    if not equipLoc or equipLoc == "" then
        return
    end

    local slots = nil
    if equipLoc == "INVTYPE_HEAD" then
        slots = { 0 }
    elseif equipLoc == "INVTYPE_NECK" then
        slots = { 1 }
    elseif equipLoc == "INVTYPE_SHOULDER" then
        slots = { 2 }
    elseif equipLoc == "INVTYPE_BODY" then
        slots = { 3 }
    elseif equipLoc == "INVTYPE_CHEST" or equipLoc == "INVTYPE_ROBE" then
        slots = { 4 }
    elseif equipLoc == "INVTYPE_WAIST" then
        slots = { 5 }
    elseif equipLoc == "INVTYPE_LEGS" then
        slots = { 6 }
    elseif equipLoc == "INVTYPE_FEET" then
        slots = { 7 }
    elseif equipLoc == "INVTYPE_WRIST" then
        slots = { 8 }
    elseif equipLoc == "INVTYPE_HAND" then
        slots = { 9 }
    elseif equipLoc == "INVTYPE_FINGER" then
        slots = { 10, 11 }
    elseif equipLoc == "INVTYPE_TRINKET" then
        slots = { 12, 13 }
    elseif equipLoc == "INVTYPE_CLOAK" then
        slots = { 14 }
    elseif equipLoc == "INVTYPE_WEAPONMAINHAND" then
        slots = { 15 }
    elseif equipLoc == "INVTYPE_SHIELD" or equipLoc == "INVTYPE_HOLDABLE" or equipLoc == "INVTYPE_WEAPONOFFHAND" then
        slots = { 16 }
    elseif equipLoc == "INVTYPE_RANGED" or equipLoc == "INVTYPE_THROWN" or equipLoc == "INVTYPE_RANGEDRIGHT" or equipLoc == "INVTYPE_RELIC" then
        slots = { 17 }
    elseif equipLoc == "INVTYPE_TABARD" then
        slots = { 18 }
    elseif equipLoc == "INVTYPE_2HWEAPON" or equipLoc == "INVTYPE_WEAPON" then
        slots = { 15, 16 }
    end

    if not slots then
        return
    end

    local addedHeader = false
    for _, slot in ipairs(slots) do
        local worn = LW_GearData[slot]
        if worn and worn.itemId then
            if not addedHeader then
                GameTooltip:AddLine(" ")
                GameTooltip:AddLine("Bot comparison", 0.0, 1.0, 0.6)
                addedHeader = true
            end
            GameTooltip:AddLine((GEAR_SLOT_NAMES[slot] or "Slot") .. ": |cffffffffitem:" .. worn.itemId .. "|h[" .. (GetItemInfo(worn.itemId) or ("Item " .. worn.itemId)) .. "]|h|r", 0.85, 0.85, 0.85, true)
        end
    end

    if addedHeader then
        GameTooltip:Show()
    end
end

local function LWCP_RefreshBagItemTooltip(btn)
    if not btn or not btn.itemId then
        return
    end

    GameTooltip:SetOwner(btn, "ANCHOR_RIGHT")
    GameTooltip:SetHyperlink("item:" .. btn.itemId .. ":0:0:0:0:0:0:0")
    LWCP_ShowTooltipComparisonLines(btn.itemId)
    GameTooltip:Show()
end

function LWCP_HandleModifierStateChanged(key)
    if key ~= "LSHIFT" and key ~= "RSHIFT" then
        return
    end

    if not LW_HoveredBagItemBtn or not LW_HoveredBagItemBtn.itemId then
        return
    end

    LWCP_HideComparisonTooltips()
    LWCP_RefreshBagItemTooltip(LW_HoveredBagItemBtn)
end

local function LWCP_SelectBagIndex(bagIdx)
    if bagIdx ~= 0 then
        local bag = LW_GearData[18 + bagIdx]
        if not bag or not bag.itemId then
            return
        end
    end

    LW_SelectedBagIdx = bagIdx or 0
    LWCP_RenderBagsTab()
end

function LWCP_ShowRetrievePrompt(guidLow, maxCount)
    if not guidLow or not maxCount or maxCount < 2 then
        return
    end

    LW_RetrievePromptGuidLow = guidLow
    LW_RetrievePromptMax = maxCount

    if LWCPRetrievePromptText then
        LWCPRetrievePromptText:SetText("Retrieve how many? (1-" .. maxCount .. ")")
    end

    if LWCPRetrievePromptEditBox then
        LWCPRetrievePromptEditBox:SetText(maxCount)
        LWCPRetrievePromptEditBox:SetFocus()
        LWCPRetrievePromptEditBox:HighlightText()
    end

    if LWCPRetrievePrompt then
        LWCPRetrievePrompt:Show()
    end
end

function LWCP_HideRetrievePrompt()
    if LWCPRetrievePromptEditBox then
        LWCPRetrievePromptEditBox:SetText("")
    end
    if LWCPRetrievePrompt then
        LWCPRetrievePrompt:Hide()
    end
    LW_RetrievePromptGuidLow = nil
    LW_RetrievePromptMax = nil
end

function LWCP_ConfirmRetrievePrompt()
    if not LW_RetrievePromptGuidLow or not LW_RetrievePromptMax then
        LWCP_HideRetrievePrompt()
        return
    end

    local text = LWCPRetrievePromptEditBox and LWCPRetrievePromptEditBox:GetText() or ""
    local count = tonumber(text or "")
    if not count or count < 1 or count > LW_RetrievePromptMax then
        DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Enter a value from 1 to " .. LW_RetrievePromptMax .. ".")
        return
    end

    LWCP_RetrieveItem(LW_RetrievePromptGuidLow, count)
    LWCP_HideRetrievePrompt()
end

-- -----------------------------------------------
-- Gear tab — slot button creation (called OnLoad)
-- -----------------------------------------------

-- BEGIN LWCP_BAGS_TAB_LAYOUT
local LW_BagsTabLayout = {
    LWCPBagContainerBtn0 = { x = -88, y = -54, w = 30, h = 30 },
    LWCPBagContainerBtn1 = { x = -52, y = -54, w = 30, h = 30 },
    LWCPBagContainerBtn2 = { x = -16, y = -54, w = 30, h = 30 },
    LWCPBagContainerBtn3 = { x = 20, y = -54, w = 30, h = 30 },
    LWCPBagContainerBtn4 = { x = 56, y = -54, w = 30, h = 30 },
    LWCPBagGearBtn = { x = 102, y = -50, w = 46, h = 22 },
    LWCPBagItemSlot1 = { x = -105, y = -112, w = 28, h = 28 },
    LWCPBagItemSlot2 = { x = -75, y = -112, w = 28, h = 28 },
    LWCPBagItemSlot3 = { x = -45, y = -112, w = 28, h = 28 },
    LWCPBagItemSlot4 = { x = -15, y = -112, w = 28, h = 28 },
    LWCPBagItemSlot5 = { x = 15, y = -112, w = 28, h = 28 },
    LWCPBagItemSlot6 = { x = 45, y = -112, w = 28, h = 28 },
    LWCPBagItemSlot7 = { x = 75, y = -112, w = 28, h = 28 },
    LWCPBagItemSlot8 = { x = 105, y = -112, w = 28, h = 28 },
    LWCPBagItemSlot9 = { x = -105, y = -142, w = 28, h = 28 },
    LWCPBagItemSlot10 = { x = -75, y = -142, w = 28, h = 28 },
    LWCPBagItemSlot11 = { x = -45, y = -142, w = 28, h = 28 },
    LWCPBagItemSlot12 = { x = -15, y = -142, w = 28, h = 28 },
    LWCPBagItemSlot13 = { x = 15, y = -142, w = 28, h = 28 },
    LWCPBagItemSlot14 = { x = 45, y = -142, w = 28, h = 28 },
    LWCPBagItemSlot15 = { x = 75, y = -142, w = 28, h = 28 },
    LWCPBagItemSlot16 = { x = 105, y = -142, w = 28, h = 28 },
    LWCPBagItemSlot17 = { x = -105, y = -172, w = 28, h = 28 },
    LWCPBagItemSlot18 = { x = -75, y = -172, w = 28, h = 28 },
    LWCPBagItemSlot19 = { x = -45, y = -172, w = 28, h = 28 },
    LWCPBagItemSlot20 = { x = -15, y = -172, w = 28, h = 28 },
    LWCPBagItemSlot21 = { x = 15, y = -172, w = 28, h = 28 },
    LWCPBagItemSlot22 = { x = 45, y = -172, w = 28, h = 28 },
    LWCPBagItemSlot23 = { x = 75, y = -172, w = 28, h = 28 },
    LWCPBagItemSlot24 = { x = 105, y = -172, w = 28, h = 28 },
    LWCPBagItemSlot25 = { x = -105, y = -202, w = 28, h = 28 },
    LWCPBagItemSlot26 = { x = -75, y = -202, w = 28, h = 28 },
    LWCPBagItemSlot27 = { x = -45, y = -202, w = 28, h = 28 },
    LWCPBagItemSlot28 = { x = -15, y = -202, w = 28, h = 28 },
    LWCPBagItemSlot29 = { x = 15, y = -202, w = 28, h = 28 },
    LWCPBagItemSlot30 = { x = 45, y = -202, w = 28, h = 28 },
    LWCPBagItemSlot31 = { x = 75, y = -202, w = 28, h = 28 },
    LWCPBagItemSlot32 = { x = 105, y = -202, w = 28, h = 28 },
}
-- END LWCP_BAGS_TAB_LAYOUT

-- BEGIN LWCP_GEAR_SLOT_LAYOUT
local LW_GearSlotLayout = {
    LWCPGearSlot1 = { x = -90, y = -66, w = 30, h = 30 },
    LWCPGearSlot2 = { x = -58, y = -66, w = 30, h = 30 },
    LWCPGearSlot3 = { x = -26, y = -66, w = 30, h = 30 },
    LWCPGearSlot4 = { x = 6, y = -66, w = 30, h = 30 },
    LWCPGearSlot5 = { x = 38, y = -66, w = 30, h = 30 },
    LWCPGearSlot6 = { x = 70, y = -66, w = 30, h = 30 },
    LWCPGearSlot7 = { x = 102, y = -66, w = 30, h = 30 },
    LWCPGearSlot8 = { x = -90, y = -99, w = 30, h = 30 },
    LWCPGearSlot9 = { x = -58, y = -99, w = 30, h = 30 },
    LWCPGearSlot10 = { x = -26, y = -99, w = 30, h = 30 },
    LWCPGearSlot11 = { x = 6, y = -99, w = 30, h = 30 },
    LWCPGearSlot12 = { x = 38, y = -99, w = 30, h = 30 },
    LWCPGearSlot13 = { x = 70, y = -99, w = 30, h = 30 },
    LWCPGearSlot14 = { x = 102, y = -99, w = 30, h = 30 },
    LWCPGearSlot15 = { x = -90, y = -132, w = 30, h = 30 },
    LWCPGearSlot16 = { x = -58, y = -132, w = 30, h = 30 },
    LWCPGearSlot17 = { x = -26, y = -132, w = 30, h = 30 },
    LWCPGearSlot18 = { x = 6, y = -132, w = 30, h = 30 },
    LWCPGearSlot19 = { x = 38, y = -132, w = 30, h = 30 },
}
-- END LWCP_GEAR_SLOT_LAYOUT

function LWCP_InitGearPage(frame)
    -- Gear slots — each maps to a fixed equipment slot for icon + tooltip
    for i = 1, 19 do
        local btn = _G["LWCPGearSlot" .. i]
        local eslot = GEAR_SLOT_ORDER[i]
        btn.icon = _G[btn:GetName() .. "Icon"]
        btn.count = _G[btn:GetName() .. "Count"]
        btn.hl = _G[btn:GetName() .. "Highlight"]
        btn:RegisterForClicks("LeftButtonUp", "RightButtonUp")
        btn.equipSlot = eslot
        btn.icon:SetTexture(GEAR_SLOT_ICON[eslot])
        btn.icon:SetAlpha(0.4)
        btn.guidLow = nil
        btn.itemId = nil
        btn.stackCount = nil

        btn:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(btn, "ANCHOR_RIGHT")
            if btn.itemId then
                GameTooltip:SetHyperlink("item:" .. btn.itemId .. ":0:0:0:0:0:0:0")
            else
                local label = (GEAR_SLOT_NAMES[btn.equipSlot] or "Slot")
                if LW_PickedGuidLow then
                    GameTooltip:SetText(label .. "\n|cffff0(Click to equip picked item)|r")
                else
                    GameTooltip:SetText(label .. " — empty")
                end
            end
            GameTooltip:Show()
        end)
        btn:SetScript("OnLeave", function() GameTooltip:Hide() end)
        btn:SetScript("OnClick", function(self, mouseButton)
            if mouseButton == "RightButton" then
                if btn.guidLow then
                    LWCP_ClearPick()
                    LWCP_UnequipItem(btn.guidLow)
                end
                return
            end
            if LW_PickedGuidLow then
                LWCP_EquipItem(LW_PickedGuidLow)
                LWCP_ClearPick()
            end
        end)
    end

end

function LWCP_InitBagsPage(frame)
    for i = 0, 4 do
        local btn = _G["LWCPBagSelect" .. i]
        btn.icon = _G[btn:GetName() .. "Icon"]
        btn.hl = _G[btn:GetName() .. "Highlight"]
        btn.bagIdx = i

        btn:SetScript("OnClick", function(self)
            LWCP_SelectBagIndex(self.bagIdx)
        end)
        btn:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            local label = (self.bagIdx == 0 and "Backpack") or ("Bag " .. self.bagIdx)
            if self.itemId then
                GameTooltip:SetHyperlink("item:" .. self.itemId .. ":0:0:0:0:0:0:0")
                GameTooltip:AddLine(" ")
                GameTooltip:AddLine(label, 0.0, 1.0, 0.6)
            else
                GameTooltip:SetText(label .. " — empty")
            end
            GameTooltip:Show()
        end)
        btn:SetScript("OnLeave", function()
            LWCP_HideComparisonTooltips()
            GameTooltip:Hide()
        end)
    end

    for i = 1, 32 do
        local btn = _G["LWCPBagsSlot" .. i]
        btn.icon = _G[btn:GetName() .. "Icon"]
        btn.count = _G[btn:GetName() .. "Count"]
        btn.hl = _G[btn:GetName() .. "Highlight"]
        btn.icon:SetTexture(nil)
        btn.icon:SetAlpha(0)
        btn.guidLow = nil
        btn.itemId = nil
        btn.stackCount = nil
        btn:RegisterForClicks("LeftButtonUp", "RightButtonUp")
        btn:SetScript("OnEnter", function(self)
            if btn.itemId then
                LW_HoveredBagItemBtn = btn
                LWCP_RefreshBagItemTooltip(btn)
            end
        end)
        btn:SetScript("OnLeave", function()
            if LW_HoveredBagItemBtn == btn then
                LW_HoveredBagItemBtn = nil
            end
            LWCP_HideComparisonTooltips()
            GameTooltip:Hide()
        end)
        btn:SetScript("OnClick", function(self, mouseButton)
            if not btn.guidLow then return end
            if mouseButton == "RightButton" then
                LWCP_ClearPick()
                if IsControlKeyDown() and btn.stackCount and btn.stackCount > 1 then
                    LWCP_ShowRetrievePrompt(btn.guidLow, btn.stackCount)
                else
                    LWCP_RetrieveItem(btn.guidLow)
                end
            else
                if LW_PickedGuidLow == btn.guidLow then
                    LWCP_ClearPick()
                else
                    LWCP_PickItem(btn)
                end
            end
        end)
    end

    local equip = LWCPBagsEquipSlot
    equip.icon = _G[equip:GetName() .. "Icon"]
    equip.count = _G[equip:GetName() .. "Count"]
    equip.hl = _G[equip:GetName() .. "Highlight"]
    equip:RegisterForClicks("LeftButtonUp")
    equip.icon:SetTexture("Interface\\PaperDollInfoFrame\\UI-PaperDoll-Slot-MainHand")
    equip.icon:SetAlpha(0.45)
    if equip.count then equip.count:SetText("") end

    equip:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        if LW_PickedGuidLow then
            GameTooltip:SetText("Equip\n|cffffd200Click to equip the selected item on the bot.|r")
        else
            GameTooltip:SetText("Equip")
        end
        GameTooltip:Show()
    end)
    equip:SetScript("OnLeave", function() GameTooltip:Hide() end)
    equip:SetScript("OnClick", function(self)
        if LW_PickedGuidLow then
            LWCP_EquipItem(LW_PickedGuidLow)
            LWCP_ClearPick()
        end
    end)
end

-- -----------------------------------------------
-- Gear tab — render current inventory data
-- -----------------------------------------------
function LWCP_RenderGearTab()
    -- Gear slots: each button maps to its fixed equipment slot
    for i = 1, 19 do
        local btn = _G["LWCPGearSlot" .. i]
        if not btn then break end
        local eslot = GEAR_SLOT_ORDER[i]
        local item = LW_GearData[eslot]
        if item then
            btn.itemId  = item.itemId
            btn.guidLow = item.guidLow
            btn.stackCount = nil
            local _, _, _, _, _, _, _, _, _, icon = GetItemInfo(item.itemId)
            if icon then
                btn.icon:SetTexture(icon)
                btn.icon:SetAlpha(1)
            else
                -- Item not yet cached — show slot placeholder until hover loads it
                btn.icon:SetTexture(GEAR_SLOT_ICON[eslot])
                btn.icon:SetAlpha(0.7)
            end
        else
            btn.itemId  = nil
            btn.guidLow = nil
            btn.stackCount = nil
            btn.icon:SetTexture(GEAR_SLOT_ICON[eslot])
            btn.icon:SetAlpha(0.4)
        end
        if btn.count then
            btn.count:SetText("")
        end
    end
end

function LWCP_RenderBagsTab()
    if LWCPBagsBotName then
        local name = LWCPGearBotName and LWCPGearBotName:GetText()
        LWCPBagsBotName:SetText(name or "-- No bot selected --")
    end

    for bagIdx = 0, 4 do
        local btn = _G["LWCPBagSelect" .. bagIdx]
        if btn then
            local item = nil
            if bagIdx > 0 then
                item = LW_GearData[18 + bagIdx]
            end

            if bagIdx == 0 then
                btn.itemId = nil
                btn.icon:SetTexture("Interface\\Buttons\\Button-Backpack-Up")
                btn.icon:SetAlpha(1)
            elseif item and item.itemId then
                btn.itemId = item.itemId
                local _, _, _, _, _, _, _, _, _, icon = GetItemInfo(item.itemId)
                btn.icon:SetTexture(icon or "Interface\\Buttons\\Button-Backpack-Up")
                btn.icon:SetAlpha(icon and 1 or 0.6)
            else
                btn.itemId = nil
                btn.icon:SetTexture("Interface\\Buttons\\Button-Backpack-Up")
                btn.icon:SetAlpha(0.18)
            end

            if bagIdx == LW_SelectedBagIdx then
                btn.hl:Show()
            else
                btn.hl:Hide()
            end
        end
    end

    local visible = {}
    for _, item in ipairs(LW_BagData) do
        if item.bagIdx == LW_SelectedBagIdx then
            visible[#visible + 1] = item
        end
    end
    table.sort(visible, function(a, b)
        return (a.slotIdx or 0) < (b.slotIdx or 0)
    end)

    for i = 1, 32 do
        local btn = _G["LWCPBagsSlot" .. i]
        if not btn then break end
        local item = visible[i]
        if item then
            btn.itemId = item.itemId
            btn.guidLow = item.guidLow
            btn.stackCount = item.count
            local _, _, _, _, _, _, _, _, _, icon = GetItemInfo(item.itemId)
            btn.icon:SetTexture(icon or "Interface\\Icons\\INV_Misc_QuestionMark")
            btn.icon:SetAlpha(icon and 1 or 0.6)
            if btn.count then
                if item.count and item.count > 1 then
                    btn.count:SetText(item.count)
                else
                    btn.count:SetText("")
                end
            end
        else
            btn.itemId = nil
            btn.guidLow = nil
            btn.stackCount = nil
            btn.icon:SetTexture(nil)
            btn.icon:SetAlpha(0)
            if btn.count then btn.count:SetText("") end
        end
    end
end

-- -----------------------------------------------
-- Gear tab — addon message handler
-- -----------------------------------------------
-- Called when the server sends: LWBOT\tINV;botName;G:slot:fields;B:bi:si:fields
-- G fields: itemId:enchId:g1:g2:g3:guidLow
-- B fields: itemId:enchId:g1:g2:g3:count:guidLow
function LWCP_HandleAddonMsg(prefix, payload, channel, sender)
    if prefix ~= "LWBOT" then return end
    if not payload then return end

    local parts = {}
    for part in string.gmatch(payload .. ";", "([^;]*);") do
        parts[#parts + 1] = part
    end

    if parts[1] == "QCLR" then
        LW_QuestRewards = {}
        if LWCPPageNPC and LWCPPageNPC:IsVisible() then
            LWCP_RenderQuestsTab()
        end
        return
    end

    if parts[1] == "QMODE" then
        LW_QuestMode = string.upper(parts[2] or "SMART")
        LWCP_UpdateQuestModeButtons()
        if LWCPPageNPC and LWCPPageNPC:IsVisible() then
            LWCP_RenderQuestsTab()
        end
        return
    end

    if parts[1] == "QST" then
        local reward = {
            botName = parts[2] or "Unknown",
            questId = tonumber(parts[3]) or 0,
            questTitle = parts[4] or "Quest",
            choices = {}
        }

        for i = 5, #parts do
            local choiceNum, itemId, count = string.match(parts[i], "^(%d+):(%d+):(%d+)$")
            if choiceNum and itemId and count then
                reward.choices[#reward.choices + 1] = {
                    choiceNumber = tonumber(choiceNum),
                    itemId = tonumber(itemId),
                    count = tonumber(count),
                }
            end
        end

        LW_QuestRewards[#LW_QuestRewards + 1] = reward
        if LWCPPageNPC and LWCPPageNPC:IsVisible() then
            LWCP_RenderQuestsTab()
        end
        return
    end

    if parts[1] == "QEND" then
        if LWCPPageNPC and LWCPPageNPC:IsVisible() then
            LWCP_RenderQuestsTab()
        end
        return
    end

    if parts[1] == "QACLR" then
        LW_QuestActions = {}
        LW_QuestActionsReady = false
        return
    end

    if parts[1] == "TACLR" then
        LW_TrainPendingActions = {}
        LW_TrainPendingBotGold = {}
        LW_TrainPendingOwnerGold = 0
        LW_TrainActionsReady = false
        LWCP_UpdateNPCDebugText()
        return
    end

    if parts[1] == "TABOT" then
        local botName = parts[2] or "Unknown"
        LW_TrainPendingBotGold[botName] = tonumber(parts[3]) or 0
        LWCP_UpdateNPCDebugText()
        return
    end

    if parts[1] == "TA" then
        LW_TrainPendingActions[#LW_TrainPendingActions + 1] = {
            botName = parts[2] or "Unknown",
            trainerSpellId = tonumber(parts[3]) or 0,
            spellLabel = parts[4] or "Spell",
            cost = tonumber(parts[5]) or 0,
        }
        LWCP_UpdateNPCDebugText()
        return
    end

    if parts[1] == "TAEND" then
        LW_TrainPendingOwnerGold = tonumber(parts[2]) or 0
        if #LW_TrainPendingActions > 0 or #LW_TrainActions == 0 then
            LW_TrainActions = LW_TrainPendingActions
            LW_TrainBotGold = LW_TrainPendingBotGold
            LW_TrainOwnerGold = LW_TrainPendingOwnerGold
        end
        LW_TrainActionsReady = true
        if LW_PendingTrainerAlert then
            if #LW_TrainPendingActions > 0 then
                local bots = {}
                local seen = {}
                for _, action in ipairs(LW_TrainPendingActions) do
                    if action.botName and not seen[action.botName] then
                        seen[action.botName] = true
                        bots[#bots + 1] = action.botName
                    end
                end
                DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Trainer spells available for " .. table.concat(bots, ", ") .. ". Open the NPC tab or use Train All.")
            end
            LW_PendingTrainerAlert = false
        end
        LWCP_UpdateNPCDebugText()
        if LWCPPageNPC and LWCPPageNPC:IsVisible() then
            LWCP_RenderQuestsTab()
        end
        if LW_TrainerDebugFrame and LW_TrainerDebugFrame:IsVisible() then
            LWCP_RenderTrainerDebugWindow()
        elseif #LW_TrainActions > 0 then
            LWCP_ShowTrainerWindow()
        end
        return
    end

    if parts[1] == "QA" then
        local action = {
            botName = parts[2] or "Unknown",
            questId = tonumber(parts[3]) or 0,
            questTitle = parts[4] or "Quest",
            actionType = parts[5] or "PICKUP",
            hasChoices = (parts[6] == "CHOICES"),
        }
        LW_QuestActions[#LW_QuestActions + 1] = action
        return
    end

    if parts[1] == "QAEND" then
        LW_QuestActionsReady = true
        if LWCPPageNPC and LWCPPageNPC:IsVisible() then
            LWCP_RenderQuestsTab()
        end
        return
    end

    if parts[1] ~= "INV" then return end

    local botName = parts[2] or "Unknown"
    LW_GearData = {}
    LW_BagData  = {}

    for i = 3, #parts do
        local entry = parts[i]
        if entry == "" then break end

        -- Equipped gear: G:slot:itemId:enchId:g1:g2:g3:guidLow
        local slotStr, rest = string.match(entry, "^G:(%d+):(.+)$")
        if slotStr then
            local fields = {}
            for f in string.gmatch(rest .. ":", "([^:]*):") do
                fields[#fields + 1] = f
            end
            -- fields[1]=itemId, fields[6]=guidLow
            LW_GearData[tonumber(slotStr)] = {
                itemId  = tonumber(fields[1]),
                guidLow = tonumber(fields[6]),
            }
        end

        -- Bag item: B:bagIdx:slotIdx:itemId:enchId:g1:g2:g3:count:guidLow
        local bagStr, slotIdxStr, brest = string.match(entry, "^B:(%d+):(%d+):(.+)$")
        if bagStr then
            local fields = {}
            for f in string.gmatch(brest .. ":", "([^:]*):") do
                fields[#fields + 1] = f
            end
            -- fields[1]=itemId, fields[6]=count, fields[7]=guidLow
            LW_BagData[#LW_BagData + 1] = {
                bagIdx  = tonumber(bagStr),
                slotIdx = tonumber(slotIdxStr),
                itemId  = tonumber(fields[1]),
                count   = tonumber(fields[6]),
                guidLow = tonumber(fields[7]),
            }
        end
    end

    if LWCPGearBotName then
        LWCPGearBotName:SetText("-- " .. botName .. " --")
    end

    if LWCPBagsBotName then
        LWCPBagsBotName:SetText("-- " .. botName .. " --")
    end

    if LWCPPageGear and LWCPPageGear:IsVisible() then
        LWCP_RenderGearTab()
    end

    if LWCPPageBags and LWCPPageBags:IsVisible() then
        LWCP_RenderBagsTab()
    end
end

-- -----------------------------------------------
-- Gear tab — commands
-- -----------------------------------------------
function LWCP_RequestBotBags(silent)
    if IsPartySelected() then
        if not silent then
            DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Select a specific bot on the Bots tab first.")
        end
        return
    end
    SendChatMessage(CMD_LWBOT .. GetBotRef() .. " bags")
end

function LWCP_RetrieveItem(guidLow, count)
    if not guidLow then return end
    if IsPartySelected() then
        DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Select a specific bot on the Bots tab first.")
        return
    end
    if count and count > 0 then
        SendChatMessage(CMD_LWBOT .. GetBotRef() .. " retrieve " .. guidLow .. " " .. count)
    else
        SendChatMessage(CMD_LWBOT .. GetBotRef() .. " retrieve " .. guidLow)
    end
end

function LWCP_EquipItem(guidLow)
    if not guidLow then return end
    if IsPartySelected() then
        DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Select a specific bot on the Bots tab first.")
        return
    end
    SendChatMessage(CMD_LWBOT .. GetBotRef() .. " equip " .. guidLow)
end

function LWCP_UnequipItem(guidLow)
    if not guidLow then return end
    if IsPartySelected() then
        DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Select a specific bot on the Bots tab first.")
        return
    end
    SendChatMessage(CMD_LWBOT .. GetBotRef() .. " unequip " .. guidLow)
end

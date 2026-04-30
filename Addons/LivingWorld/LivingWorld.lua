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
-- Tab switching
-- -----------------------------------------------
local LW_Tabs = { "Bots", "Combat", "Gear", "Settings" }

function LWCP_ShowTab(name)
    for _, t in ipairs(LW_Tabs) do
        local page = _G["LWCPPage" .. t]
        local btn  = _G["LWCPTab"  .. t]
        if t == name then
            page:Show()
            btn:Disable()
        else
            page:Hide()
            btn:Enable()
        end
    end
    if name == "Gear" then
        LWCP_RenderGearTab()
    end
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

-- -----------------------------------------------
-- Gear tab — inventory state
-- -----------------------------------------------
local LW_GearData    = {}
local LW_BagData     = {}
local LW_BagPage     = 1
local LW_BagPageSize = 14

-- Equipment slot display order (left-to-right, top-to-bottom across 3 rows of 7):
--   Row 1: Head Neck Shoulders Back Chest Shirt Tabard
--   Row 2: Wrists Hands Waist Legs Feet Ring1 Ring2
--   Row 3: Trinket1 Trinket2 MainHand OffHand Ranged
local GEAR_SLOT_ORDER = {
    0, 1, 2, 14, 4, 3, 18,
    8, 9, 5, 6, 7, 10, 11,
    12, 13, 15, 16, 17,
}

local EQUIPPED_BAG_SLOT_ORDER = {
    19, 20, 21, 22,
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
function LWCP_InitGearPage(frame)
    local function MakeSlot(name, parent, col, row, yBase)
        local btn = CreateFrame("Button", name, parent)
        btn:SetWidth(30)
        btn:SetHeight(30)
        btn:SetPoint("TOP", parent, "TOP", -90 + col * 32, yBase - row * 33)
        btn:EnableMouse(true)

        local bg = btn:CreateTexture(nil, "BACKGROUND")
        bg:SetAllPoints()
        bg:SetTexture("Interface\\Buttons\\UI-Slot-Background")
        bg:SetTexCoord(0.1, 0.9, 0.1, 0.9)

        local icon = btn:CreateTexture(nil, "ARTWORK")
        icon:SetAllPoints()
        btn.icon = icon

        local count = btn:CreateFontString(nil, "OVERLAY", "NumberFontNormalSmall")
        count:SetPoint("BOTTOMRIGHT", btn, "BOTTOMRIGHT", -2, 2)
        count:SetJustifyH("RIGHT")
        count:SetText("")
        btn.count = count

        -- Yellow highlight shown when this bag slot is "picked"
        local hl = btn:CreateTexture(nil, "OVERLAY")
        hl:SetTexture("Interface\\Buttons\\ButtonHilight-Square")
        hl:SetAllPoints()
        hl:SetBlendMode("ADD")
        hl:Hide()
        btn.hl = hl

        btn.guidLow   = nil
        btn.itemId    = nil
        btn.equipSlot = nil
        btn.stackCount = nil
        return btn
    end

    -- Gear slots — each maps to a fixed equipment slot for icon + tooltip
    for i = 1, 19 do
        local col = (i - 1) % 7
        local row = math.floor((i - 1) / 7)
        local btn = MakeSlot("LWCPGearSlot" .. i, frame, col, row, -66)
        local eslot = GEAR_SLOT_ORDER[i]
        btn:RegisterForClicks("LeftButtonUp", "RightButtonUp")
        btn.equipSlot = eslot
        btn.icon:SetTexture(GEAR_SLOT_ICON[eslot])
        btn.icon:SetAlpha(0.4)

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

    -- Equipped bag slots — treated like gear targets for bag equipping/swapping
    for i = 1, 4 do
        local btn = MakeSlot("LWCPBagEquipSlot" .. i, frame, i - 1, 0, -180)
        local eslot = EQUIPPED_BAG_SLOT_ORDER[i]
        btn:RegisterForClicks("LeftButtonUp", "RightButtonUp")
        btn.equipSlot = eslot
        btn.icon:SetTexture(GEAR_SLOT_ICON[eslot])
        btn.icon:SetAlpha(0.35)

        btn:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(btn, "ANCHOR_RIGHT")
            if btn.itemId then
                GameTooltip:SetHyperlink("item:" .. btn.itemId .. ":0:0:0:0:0:0:0")
            else
                local label = (GEAR_SLOT_NAMES[btn.equipSlot] or "Bag Slot")
                if LW_PickedGuidLow then
                    GameTooltip:SetText(label .. "\n|cffff0(Click to equip picked bag)|r")
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

    -- Bag slots — left-click picks for equipping, right-click retrieves
    for i = 1, 14 do
        local col = (i - 1) % 7
        local row = math.floor((i - 1) / 7)
        local btn = MakeSlot("LWCPBagSlot" .. i, frame, col, row, -228)
        btn.icon:SetTexture(nil)
        btn.icon:SetAlpha(0)

        btn:RegisterForClicks("LeftButtonUp", "RightButtonUp")
        btn:SetScript("OnEnter", function(self)
            if btn.itemId then
                GameTooltip:SetOwner(btn, "ANCHOR_RIGHT")
                GameTooltip:SetHyperlink("item:" .. btn.itemId .. ":0:0:0:0:0:0:0")
                GameTooltip:Show()
            end
        end)
        btn:SetScript("OnLeave", function() GameTooltip:Hide() end)
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

    for i = 1, 4 do
        local btn = _G["LWCPBagEquipSlot" .. i]
        if not btn then break end
        local eslot = EQUIPPED_BAG_SLOT_ORDER[i]
        local item = LW_GearData[eslot]
        if item then
            btn.itemId = item.itemId
            btn.guidLow = item.guidLow
            btn.stackCount = nil
            local _, _, _, _, _, _, _, _, _, icon = GetItemInfo(item.itemId)
            if icon then
                btn.icon:SetTexture(icon)
                btn.icon:SetAlpha(1)
            else
                btn.icon:SetTexture(GEAR_SLOT_ICON[eslot])
                btn.icon:SetAlpha(0.7)
            end
        else
            btn.itemId = nil
            btn.guidLow = nil
            btn.stackCount = nil
            btn.icon:SetTexture(GEAR_SLOT_ICON[eslot])
            btn.icon:SetAlpha(0.35)
        end
        if btn.count then
            btn.count:SetText("")
        end
    end

    -- Bag slots: paginated
    local totalPages = math.max(1, math.ceil(#LW_BagData / LW_BagPageSize))
    LW_BagPage = math.min(LW_BagPage, totalPages)
    local pageStart = (LW_BagPage - 1) * LW_BagPageSize + 1

    if LWCPGearPageLabel then
        LWCPGearPageLabel:SetText(LW_BagPage .. "/" .. totalPages)
    end

    for i = 1, 14 do
        local btn = _G["LWCPBagSlot" .. i]
        if not btn then break end
        local item = LW_BagData[pageStart + i - 1]
        if item then
            btn.itemId  = item.itemId
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
            btn.itemId  = nil
            btn.guidLow = nil
            btn.stackCount = nil
            btn.icon:SetTexture(nil)
            btn.icon:SetAlpha(0)
            if btn.count then
                btn.count:SetText("")
            end
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

    if LWCPPageGear and LWCPPageGear:IsVisible() then
        LW_BagPage = 1
        LWCP_RenderGearTab()
    end
end

-- -----------------------------------------------
-- Gear tab — commands
-- -----------------------------------------------
function LWCP_RequestBotBags()
    if IsPartySelected() then
        DEFAULT_CHAT_FRAME:AddMessage("|cff00cc44LWCP:|r Select a specific bot on the Bots tab first.")
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

function LWCP_BagPageDec()
    if LW_BagPage > 1 then
        LW_BagPage = LW_BagPage - 1
        LWCP_RenderGearTab()
    end
end

function LWCP_BagPageInc()
    local totalPages = math.max(1, math.ceil(#LW_BagData / LW_BagPageSize))
    if LW_BagPage < totalPages then
        LW_BagPage = LW_BagPage + 1
        LWCP_RenderGearTab()
    end
end

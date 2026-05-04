# LivingWorld Bot Editor - Threat Awareness Update

## Summary

Updated the LivingWorld Bot Editor (`lw_bot_editor.py`) to support new threat-aware features that were recently implemented in the bot combat system. This brings the editor UI in sync with the C++ backend capabilities.

## Changes Made

### 1. Updated Condition Stats (STAT_KEYS)

Added three new stat keys that bots can use in rotation conditions:

- **`threat_pct`**: Returns the subject's threat as a percentage of the top threat holder (0-100+)
  - Used for: Threat dumps, conditional DPS, tank threat monitoring
  - Example: `bot.threat_pct >= 90` means bot has 90% or more of top threat

- **`is_aggro_holder`**: Boolean indicating whether the subject currently holds aggro
  - Used for: Emergency taunts, threat drops, conditional abilities
  - Example: `owner.is_aggro_holder == 0` means tank doesn't have aggro

- **`aura_stacks`**: Returns the number of stacks of a specific aura
  - Used for: Stacking debuff management, buff tracking
  - Example: Check if a DoT has reached 5 stacks before using a finisher

### 2. Updated Target Keys (TARGET_KEYS)

Added two new target selection options:

- **`lowest_hp_party`**: Targets the party member with the lowest HP percentage
  - Replaces the deprecated `ally_lowest_hp`
  - Better naming consistency with other keys

- **`enemy_primary_victim`**: Targets whoever the primary enemy is currently attacking
  - Useful for off-tanks and protection abilities
  - Example: Shield the person taking damage from the boss

### 3. UI Enhancements

**Condition Value Editor:**
- Added special handling for `aura_stacks` stat
  - Shows both spell picker (for selecting which aura) and numeric value (for stack count)
  - Stores spell ID in `string_value` field (matching C++ implementation)
  - Displays as: "Spell Name [ID] stacks=5"

**Boolean Stats:**
- Added `is_aggro_holder` to the boolean stats set
  - Displays as True/False dropdown instead of numeric input
  - Stored as 1/0 in database

**Condition Descriptions:**
- Updated display logic to properly show `aura_stacks` conditions
- Format: "subject.aura_stacks >= Spell Name [ID] stacks=5"

### 4. Documentation

Created comprehensive documentation:

**README.md:**
- Complete feature overview
- All condition stats with descriptions
- All target keys with use cases
- Comparison operators reference
- Database schema information
- Configuration tips

**THREAT_EXAMPLES.md:**
- Practical threat-aware rotation examples for all roles
- Tank rotations (taunt macros, threat generation)
- DPS rotations (threat dumps, conditional bursts)
- Healer rotations (fade, hand of salvation)
- Hybrid rotations (threat-aware healing/DPS switching)
- Advanced multi-condition examples
- Debugging and testing tips

## Technical Details

### Code Changes

1. **Constants (lines 61-68):**
   ```python
   TARGET_KEYS = [..., "enemy_primary_victim", "lowest_hp_party", ...]
   STAT_KEYS = [..., "threat_pct", "is_aggro_holder", "aura_stacks", ...]
   BOOL_STAT_KEYS = {..., "is_aggro_holder"}
   ```

2. **Condition Display (lines 1236-1242):**
   - Added `aura_stacks` case to show spell name and stack count

3. **UI Sync (lines 1255-1294):**
   - Added `aura_stacks_mode` handling
   - Shows spell combo + value entry for stack count

4. **Stat Change Handler (lines 1296-1308):**
   - Added `aura_stacks` case to populate spell picker from `string_value`

5. **Spell Picker Handlers (lines 1314-1330):**
   - Modified to store `aura_stacks` spell ID in `string_value` instead of `numeric_value`
   - Matches C++ implementation expectation

### Backend Compatibility

All changes are fully compatible with the existing C++ implementation in:
- `modules/mod-living-world/src/service/BotCombatRuntimeEvaluator.cpp`
- Lines 337-378 implement the new stat evaluations

The editor now exposes all functionality that the bot combat system can evaluate at runtime.

## Testing Recommendations

1. **Basic Functionality:**
   - Create a condition with `threat_pct` and verify it saves/loads correctly
   - Create a condition with `is_aggro_holder` and verify boolean display
   - Create a condition with `aura_stacks` and verify spell picker + value entry

2. **Target Selection:**
   - Verify `lowest_hp_party` appears in target dropdowns
   - Verify `enemy_primary_victim` appears in target dropdowns

3. **Integration:**
   - Create a full rotation using threat conditions
   - Load on a bot and verify it executes in-game
   - Check server logs for `[LivingWorldDebug]` entries showing condition evaluations

## Migration Notes

**No database migration required** - The new features use existing table structure:
- `threat_pct` and `is_aggro_holder` use existing `stat_key` and `numeric_value` columns
- `aura_stacks` uses existing `stat_key`, `string_value`, and `numeric_value` columns
- New target keys use existing `target_key` column

**Backward Compatibility:**
- Existing rotations continue to work unchanged
- New features are additive only
- No breaking changes to API or database schema

## Future Enhancements

Potential additions for future updates:

1. **Threat Visualization**: Add a threat meter widget to the editor
2. **Rotation Validation**: Warn when threat conditions might conflict
3. **Templates**: Pre-built rotation templates for common tank/DPS/heal scenarios
4. **Simulation**: Test mode to simulate threat scenarios
5. **Import/Export**: Share rotations between profiles or servers

## Related Files

- `tools/lw-editor/lw_bot_editor.py` - Main editor (updated)
- `tools/lw-editor/README.md` - User documentation (new)
- `tools/lw-editor/THREAT_EXAMPLES.md` - Practical examples (new)
- `modules/mod-living-world/src/service/BotCombatRuntimeEvaluator.cpp` - Backend implementation (reference)
- `modules/mod-living-world/src/ai/CompanionAI.cpp` - Threat message push (reference)

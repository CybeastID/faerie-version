#include "global.h"
#include "battle.h"
#include "battle_bug_space.h"
#include "battle_scripts.h"
#include "battle_util.h"
#include "event_data.h"

bool32 BugSpaceHasDoomClock(void)
{
    return gBattleStruct != NULL && gBattleStruct->bugSpace.active
        && gBattleStruct->bugSpace.mode != BUGSPACE_MODE_KOANKO_CRUSH_ONLY;
}

bool32 BugSpaceIsKazuradrop(void)
{
    return gBattleStruct != NULL && gBattleStruct->bugSpace.active
        && gBattleStruct->bugSpace.mode == BUGSPACE_MODE_KAZURADROP;
}

// Called at the existing decay scheduler position. No stage-change abilities,
// items, healing or generic stat-loss callbacks belong to this crushing effect.
bool32 BugSpaceCrushBoosts(void)
{
    if (!gBattleStruct->bugSpace.active)
        return FALSE;
    u32 firstAffected = MAX_BATTLERS_COUNT;
    for (u32 i = 0; i < gBattlersCount; i++)
    {
        if (GetBattlerSide(i) == GetBattlerSide(gBattleStruct->bugSpace.sourceBattler))
            continue;
        for (u32 j = 0; j < NUM_BATTLE_STATS; j++)
        {
            if (gBattleMons[i].statStages[j] > DEFAULT_STAT_STAGE)
            {
                u32 boost = gBattleMons[i].statStages[j] - DEFAULT_STAT_STAGE;
                gBattleMons[i].statStages[j] = DEFAULT_STAT_STAGE + boost / 2;
                if (firstAffected == MAX_BATTLERS_COUNT)
                    firstAffected = i;
            }
        }
    }
    if (firstAffected == MAX_BATTLERS_COUNT)
        return FALSE;

    gBattlerTarget = firstAffected;
    gBattleScripting.battler = firstAffected;
    if (!BugSpaceIsKazuradrop())
        BattleScriptCall(BattleScript_KoankoArceusStatDecay);
    else if (!FlagGet(FLAG_TEMP_1))
    {
        BattleScriptCall(BattleScript_BugSpaceStatDecayFirst);
        FlagSet(FLAG_TEMP_1);
    }
    else
        BattleScriptCall(BattleScript_BugSpaceStatDecay);
    return TRUE;
}

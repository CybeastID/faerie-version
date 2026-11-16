#include "global.h"
#include "battle.h"
#include "battle_ai_main.h"
#include "battle_temp_ally.h"
#include "pokemon.h"

enum TempAllyState TempAllyGetState(void)
{
    return gBattleStruct != NULL ? gBattleStruct->tempAllyState : TEMP_ALLY_NONE;
}

void TempAllySetState(enum TempAllyState state)
{
    if (gBattleStruct != NULL)
        gBattleStruct->tempAllyState = state;
}

bool32 TempAllyOwnsBattler(u32 battler)
{
    return battler == TEMP_ALLY_BATTLER && TempAllyGetState() != TEMP_ALLY_NONE;
}

bool32 TempAllyOwnsTrainer(enum BattleTrainer trainer)
{
    return trainer == B_TRAINER_PARTNER && TempAllyGetState() != TEMP_ALLY_NONE;
}

void TempAllyClearStorage(void)
{
    if (gBattleStruct == NULL)
        return;

    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        ZeroMonData(&gParties[B_TRAINER_PARTNER][i]);
        memset(&gBattleStruct->partyState[B_TRAINER_PARTNER][i], 0, sizeof(gBattleStruct->partyState[B_TRAINER_PARTNER][i]));
        gBattleStruct->gimmick.activeGimmick[B_TRAINER_PARTNER][i] = GIMMICK_NONE;
    }
    gBattleStruct->faintCounter[B_TRAINER_PARTNER] = 0;

    if (gAiPartyData != NULL)
    {
        memset(&gAiPartyData->mons[B_TRAINER_PARTNER], 0, sizeof(gAiPartyData->mons[B_TRAINER_PARTNER]));
        gAiPartyData->count[B_TRAINER_PARTNER] = 0;
    }
}

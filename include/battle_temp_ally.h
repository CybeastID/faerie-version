#ifndef GUARD_BATTLE_TEMP_ALLY_H
#define GUARD_BATTLE_TEMP_ALLY_H

#include "constants/battle.h"

// Koanko's temporary AI reinforcement (Zacian-Crowned), always battler 2 (player-right).
// It is owned by B_TRAINER_PARTNER: GetBattlerTrainer() routes party, PartyState, AI party data, gimmick
// state and faint counters to the partner row, so nothing aliases the real player's party. It never
// enables BATTLE_TYPE_MULTI/INGAME_PARTNER, so BattleSideHasTwoTrainers(B_SIDE_PLAYER) stays FALSE and
// loss detection keeps counting only the real player party.
enum TempAllyState
{
    TEMP_ALLY_NONE,
    TEMP_ALLY_ENTERING,
    TEMP_ALLY_ACTIVE,
    TEMP_ALLY_FAINTED,
    TEMP_ALLY_DEPARTING,
};

#define TEMP_ALLY_BATTLER 2 // B_POSITION_PLAYER_RIGHT in a doubles layout.

enum TempAllyState TempAllyGetState(void);
void TempAllySetState(enum TempAllyState state);
bool32 TempAllyOwnsBattler(u32 battler);
bool32 TempAllyOwnsTrainer(enum BattleTrainer trainer);
bool32 TempAllyCanEnter(void);
void BS_TempAllyEnter(void);
void BS_TempAllyInitialize(void);
void BS_TempAllySlideBattlers(void);
void BS_TempAllyFinishEntry(void);
// Ball-less entrance, same send-out contract as the Kazuradrop entrance (see kazuradrop_entrance.c).
u8 DoTempAllyZipInAnimation(u32 battler);
// Zero every partner-row structure the ally uses. Called before it enters and after it leaves.
void TempAllyClearStorage(void);

#endif // GUARD_BATTLE_TEMP_ALLY_H

// Sparse trainer/difficulty defaults. Unspecified difficulties inherit Normal.
// Use &gEmptyTrainerBattleScripts to explicitly disable that inheritance.
// See docs/mechanics/entry_scripts.md for complete authoring examples.
static const struct TrainerBattleScripts *const sTrainerBattleScripts[TRAINERS_COUNT][DIFFICULTY_COUNT] =
{
    [TRAINER_NONE] = {NULL},
};

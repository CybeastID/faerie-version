// These profiles grant one through five extra full HP bars. Custom profiles can
// supply onBreakScript/afterBreakScript using the existing BreakBarConfig API.
const struct BreakBarConfig gTrainerBreakBarProfiles[BREAK_BAR_MAX_EXTRA_BARS] =
{
    {.extraBars = 1},
    {.extraBars = 2},
    {.extraBars = 3},
    {.extraBars = 4},
    {.extraBars = 5},
};

// Activated by Bug Space, rather than assigned during trainer party creation.
const struct BreakBarConfig gKazuradropBreakBarConfig =
{
    .phases[BREAK_BAR_PURPLE - 1] =
    {
        .onBreakScript = BattleScript_KazuradropTransform,
        .afterBreakScript = BattleScript_KazuradropTransformReturn,
    },
    .extraBars = 1,
    .flags = BREAK_BAR_IGNORE_WHILE_TRANSFORMED,
};

const struct TrainerBreakBarRules gEmptyTrainerBreakBars = {0};

// Koanko's independently exported initial and conjured team assignments.
const struct TrainerBreakBarRules gKoankoInitialBreakBars = {0};
// Conjured team: only Calyrex and Eternatus get an original bar. Each one's single extra bar breaks
// into its boss form (one first-break transition, one final form bar); solo TX and the rest stay barless.
static const struct BreakBarConfig sKoankoBossFormBar =
{
    .phases[BREAK_BAR_PURPLE - 1] = {.onBreakScript = BattleScript_KoankoBossFormBreak},
    .extraBars = 1,
};

static const struct TrainerBreakBarRule sKoankoConjuredRules[] =
{
    {TRAINER_BREAK_BAR_SPECIES, SPECIES_CALYREX_ICE, &sKoankoBossFormBar},
    {TRAINER_BREAK_BAR_SPECIES, SPECIES_ETERNATUS, &sKoankoBossFormBar},
};

const struct TrainerBreakBarRules gKoankoConjuredBreakBars = TRAINER_BREAK_BAR_RULES(sKoankoConjuredRules);

// Only explicitly configured trainers need a block. Unspecified entries are NULL.
// Missing difficulty entries inherit Normal; an explicit empty ruleset opts out.
// Add each trainer's rules and any inline BreakBarConfig objects directly here
// with INLINE_BREAK_BAR_RULES(...); named profiles and rulesets also work.
// See docs/mechanics/trainer_break_bars.md for complete authoring examples.
static const struct TrainerBreakBarRules *const sTrainerBreakBars[TRAINERS_COUNT][DIFFICULTY_COUNT] =
{
    [TRAINER_KOANKO] =
    {
        [DIFFICULTY_NORMAL] = &gKoankoInitialBreakBars,
    },
};

#include "global.h"
#include "test/battle.h"
#include "battle_koanko.h"
#include "event_data.h"

ASSUMPTIONS
{
    ASSUME(GetSpeciesFormTable(SPECIES_ARCEUS_NORMAL) != NULL);
    ASSUME(gSpeciesInfo[SPECIES_ARCEUS_NORMAL].abilities[0] == ABILITY_MULTITYPE);
}

// Independent oracle: enumerate the engine's actual Arceus form family, rather
// than copying the production mapping or maintaining a second matchup list.
static u32 ArceusResistanceMask(enum Type attackType, u32 *count)
{
    const u16 *forms = GetSpeciesFormTable(SPECIES_ARCEUS_NORMAL);
    u32 mask = 0;
    *count = 0;
    if (attackType == TYPE_NONE || attackType == TYPE_MYSTERY || attackType >= NUMBER_OF_MON_TYPES)
        return 0;
    for (u32 i = 0; forms[i] != FORM_SPECIES_END; i++)
    {
        enum Type type = GetSpeciesType(forms[i], 0);
        if (GetTypeModifier(attackType, type) < UQ_4_12(1.0))
        {
            mask |= 1u << type;
            (*count)++;
        }
    }
    return mask;
}

static uq4_12_t ExpectedJudgmentMatchup(enum Type attackType, const enum Type foeTypes[3])
{
    uq4_12_t result = UQ_4_12(1.0);
    u32 seen = 0;
    if (foeTypes == NULL)
        return result;
    for (u32 slot = 0; slot < 3; slot++)
    {
        enum Type type = foeTypes[slot];
        if (type == TYPE_NONE || type == TYPE_MYSTERY || type >= NUMBER_OF_MON_TYPES || (seen & (1u << type)))
            continue;
        seen |= 1u << type;
        result = uq4_12_multiply(result, GetTypeModifier(attackType, type));
    }
    return result;
}

// Filter the independently enumerated family by defensive tier first, then
// offensive tier. Hand-calculated cases below also check this oracle's policy.
static u32 ArceusRankedMask(enum Type attackType, const enum Type foeTypes[3], u32 *count)
{
    u32 candidates = ArceusResistanceMask(attackType, count);
    uq4_12_t defense = UQ_4_12(1.0), offense = UQ_4_12(0.0);
    for (u32 type = 0; type < NUMBER_OF_MON_TYPES; type++)
        if (candidates & (1u << type))
            defense = min(defense, GetTypeModifier(attackType, type));
    for (u32 type = 0; type < NUMBER_OF_MON_TYPES; type++)
        if ((candidates & (1u << type)) && GetTypeModifier(attackType, type) != defense)
            candidates &= ~(1u << type);
    for (u32 type = 0; type < NUMBER_OF_MON_TYPES; type++)
        if (candidates & (1u << type))
            offense = max(offense, ExpectedJudgmentMatchup(type, foeTypes));
    *count = 0;
    for (u32 type = 0; type < NUMBER_OF_MON_TYPES; type++)
    {
        if (!(candidates & (1u << type)))
            continue;
        if (ExpectedJudgmentMatchup(type, foeTypes) != offense)
            candidates &= ~(1u << type);
        else
            (*count)++;
    }
    return candidates;
}

static void ExpectOrdinaryArceusForm(enum Species species)
{
    EXPECT_EQ(GET_BASE_SPECIES_ID(species), SPECIES_ARCEUS_NORMAL);
    enum Type type = GetSpeciesType(species, 0);
    EXPECT_EQ(GetSpeciesType(species, 1), type);
    EXPECT_EQ(gSpeciesInfo[species].abilities[0], ABILITY_MULTITYPE);
    EXPECT_NE(type, TYPE_DIVINE);
    EXPECT_NE(type, TYPE_ALTER_EGO);
    EXPECT_NE(type, TYPE_CASTER_CLASS);
    EXPECT_NE(type, TYPE_STELLAR);
}

TEST("Koanko Command Seal tagged choices enumerate equally best-ranked ordinary Arceus forms exactly once")
{
    enum Type attackType = TYPE_NONE;
    enum Type foeType = TYPE_NONE;
    // battle.h overrides PARAMETRIZE for battle fixtures. PARAMETRIZE_LABEL
    // retains the function-runner implementation needed by these TEST cases.
    for (u32 type = 0; type < NUMBER_OF_MON_TYPES; type++)
        for (u32 foe = 0; foe < NUMBER_OF_MON_TYPES; foe++)
            PARAMETRIZE_LABEL("breaking type %d, current foe type %d", type, foe) {
                attackType = type; foeType = foe;
            }

    const enum Type types[3] = {foeType, foeType, TYPE_MYSTERY};
    const enum Type *foeTypes = foeType == TYPE_NONE ? NULL : types;
    u32 count, seen = 0;
    u32 expected = ArceusRankedMask(attackType, foeTypes, &count);
    enum Species chosen;
    if (count == 0)
    {
        EXPECT_EQ(KoankoChooseArceusResistantForm(attackType, SPECIES_ARCEUS_NORMAL, foeTypes, &chosen), FALSE);
        EXPECT_EQ(chosen, SPECIES_ARCEUS_NORMAL);
    }
    for (u32 rank = 0; rank < count; rank++)
    {
        SET_RNG(RNG_KOANKO_ARCEUS_FORM, rank);
        EXPECT(KoankoChooseArceusResistantForm(attackType, SPECIES_ARCEUS_NORMAL, foeTypes, &chosen));
        ExpectOrdinaryArceusForm(chosen);
        enum Type type = GetSpeciesType(chosen, 0);
        EXPECT_LT(GetTypeModifier(attackType, type), UQ_4_12(1.0));
        EXPECT_EQ(seen & (1u << type), 0);
        seen |= 1u << type;
    }
    EXPECT_EQ(seen, expected);
}

TEST("Koanko Command Seal keeps every current Arceus form for untyped, unknown and empty-set attacks")
{
    const u16 *forms = GetSpeciesFormTable(SPECIES_ARCEUS_NORMAL);
    enum Species current = SPECIES_ARCEUS_NORMAL;
    enum Type attackType = TYPE_NONE;
    static const enum Type fallbackTypes[] = {
        TYPE_NONE, TYPE_MYSTERY, TYPE_STELLAR, TYPE_DIVINE,
        NUMBER_OF_MON_TYPES, (enum Type)0xFF,
    };
    for (u32 i = 0; forms[i] != FORM_SPECIES_END; i++)
        for (u32 j = 0; j < ARRAY_COUNT(fallbackTypes); j++)
            PARAMETRIZE_LABEL("form %d, attack type %d", forms[i], fallbackTypes[j]) {
                current = forms[i]; attackType = fallbackTypes[j];
            }

    u32 count;
    EXPECT_EQ(ArceusResistanceMask(attackType, &count), 0);
    enum Species chosen = SPECIES_NONE;
    EXPECT_EQ(KoankoChooseArceusResistantForm(attackType, current, NULL, &chosen), FALSE);
    EXPECT_EQ(chosen, current);
}

TEST("Koanko Command Seal may retain an already-resistant current form")
{
    // Normal Arceus's genuine Ghost immunity is one of the eligible defenses.
    u32 count;
    ArceusRankedMask(TYPE_GHOST, NULL, &count);
    EXPECT_GT(count, 0);
    bool32 foundCurrent = FALSE;
    for (u32 rank = 0; rank < count; rank++)
    {
        SET_RNG(RNG_KOANKO_ARCEUS_FORM, rank);
        enum Species chosen;
        EXPECT(KoankoChooseArceusResistantForm(TYPE_GHOST, SPECIES_ARCEUS_NORMAL, NULL, &chosen));
        foundCurrent |= chosen == SPECIES_ARCEUS_NORMAL;
    }
    EXPECT(foundCurrent);
}

TEST("Koanko Command Seal form selection rejects a non-Arceus input without changing it")
{
    enum Species chosen = SPECIES_NONE;
    EXPECT_EQ(KoankoChooseArceusResistantForm(TYPE_FIRE, SPECIES_WOBBUFFET, NULL, &chosen), FALSE);
    EXPECT_EQ(chosen, SPECIES_WOBBUFFET);
}

TEST("Koanko Command Seal prioritizes immunity then Judgment effectiveness then random ties")
{
    static const struct
    {
        enum Type breakingType;
        enum Type foeTypes[3];
        u32 expectedTypes;
    } cases[] =
    {
        // A Ghost Judgment cannot hit Normal, but immunity to the captured
        // Normal attack still outranks Rock/Steel's ordinary resistance.
        {TYPE_NORMAL, {TYPE_NORMAL, TYPE_NORMAL, TYPE_MYSTERY}, 1u << TYPE_GHOST},
        // Dark would hit Psychic harder; Normal's Ghost immunity wins first.
        {TYPE_GHOST, {TYPE_PSYCHIC, TYPE_PSYCHIC, TYPE_MYSTERY}, 1u << TYPE_NORMAL},
        // Both Water and Rock resist Fire. Water hits Fire/Rock for 4x, Rock 2x.
        {TYPE_FIRE, {TYPE_FIRE, TYPE_ROCK, TYPE_MYSTERY}, 1u << TYPE_WATER},
        {TYPE_FIRE, {TYPE_GRASS, TYPE_STEEL, TYPE_MYSTERY}, 1u << TYPE_FIRE},
        // All four Fire resistances tie offensively against Dark/Ghost.
        {TYPE_FIRE, {TYPE_DARK, TYPE_GHOST, TYPE_MYSTERY},
            (1u << TYPE_ROCK) | (1u << TYPE_FIRE) | (1u << TYPE_WATER) | (1u << TYPE_DRAGON)},
        // Duplicated Grass is counted once. Doubling it would incorrectly
        // promote Fire above the Rock/Fire/Dragon tie at neutral damage.
        {TYPE_FIRE, {TYPE_WATER, TYPE_GRASS, TYPE_GRASS},
            (1u << TYPE_ROCK) | (1u << TYPE_FIRE) | (1u << TYPE_DRAGON)},
        // A distinct third type does count: Flying promotes Rock to 2x.
        {TYPE_FIRE, {TYPE_WATER, TYPE_GRASS, TYPE_FLYING}, 1u << TYPE_ROCK},
        // Grass would hit Ground/Water for 4x but does not resist Fire.
        {TYPE_FIRE, {TYPE_GROUND, TYPE_WATER, TYPE_MYSTERY},
            (1u << TYPE_WATER) | (1u << TYPE_DRAGON)},
        // No super-effective option: neutral Fire/Dragon beats resisted ones.
        {TYPE_FIRE, {TYPE_DRAGON, TYPE_STEEL, TYPE_MYSTERY},
            (1u << TYPE_FIRE) | (1u << TYPE_DRAGON)},
        // Fairy's immunity zeroes Dragon's otherwise super-effective matchup.
        {TYPE_FIRE, {TYPE_FAIRY, TYPE_DRAGON, TYPE_MYSTERY}, 1u << TYPE_ROCK},
    };
    u32 caseId = 0;
    for (u32 param = 0; param < ARRAY_COUNT(cases); param++)
        PARAMETRIZE_LABEL("rank case %d", param) { caseId = param; }

    u32 count, seen = 0;
    EXPECT_EQ(ArceusRankedMask(cases[caseId].breakingType, cases[caseId].foeTypes, &count), cases[caseId].expectedTypes);
    EXPECT_GT(count, 0);
    for (u32 rank = 0; rank < count; rank++)
    {
        SET_RNG(RNG_KOANKO_ARCEUS_FORM, rank);
        enum Species chosen;
        EXPECT(KoankoChooseArceusResistantForm(cases[caseId].breakingType, SPECIES_ARCEUS_NORMAL, cases[caseId].foeTypes, &chosen));
        ExpectOrdinaryArceusForm(chosen);
        u32 bit = 1u << GetSpeciesType(chosen, 0);
        EXPECT_EQ(seen & bit, 0);
        seen |= bit;
    }
    EXPECT_EQ(seen, cases[caseId].expectedTypes);
}

SINGLE_BATTLE_TEST("Koanko Command Seal offense uses the current switched opponent independently of the breaking type")
{
    GIVEN {
        ASSUME(GetMoveType(MOVE_EMBER) == TYPE_FIRE);
        PLAYER(SPECIES_CHARIZARD) { Speed(2); SpAttack(1); Moves(MOVE_EMBER); }
        PLAYER(SPECIES_VENUSAUR) { Speed(2); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { Speed(1); HP(500); MaxHP(500); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_EMBER); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { SWITCH(player, 1); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        // Fire is the independently supplied captured attack type. Selection
        // now sees Venusaur, not the original Fire/Flying attacker (which would
        // favor Rock). Full break-event capture is a later lifecycle milestone.
        enum Type foeTypes[3];
        GetBattlerTypes(0, FALSE, foeTypes);
        enum Species chosen;
        EXPECT(KoankoChooseArceusResistantForm(TYPE_FIRE, opponent->species, foeTypes, &chosen));
        EXPECT_EQ(player->species, SPECIES_VENUSAUR);
        EXPECT_EQ(chosen, SPECIES_ARCEUS_FIRE);
        EXPECT_EQ(opponent->species, SPECIES_ARCEUS_NORMAL);
        EXPECT_EQ(opponent->ability, ABILITY_MULTITYPE);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal offense uses effective Tera types and Stellar retains native defense")
{
    enum Type teraType;
    PARAMETRIZE { teraType = TYPE_GRASS; }
    PARAMETRIZE { teraType = TYPE_STELLAR; }
    GIVEN {
        PLAYER(SPECIES_CHARIZARD) { TeraType(teraType); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE, gimmick: GIMMICK_TERA); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        enum Type foeTypes[3];
        GetBattlerTypes(0, FALSE, foeTypes);
        enum Species chosen;
        EXPECT(KoankoChooseArceusResistantForm(TYPE_FIRE, opponent->species, foeTypes, &chosen));
        EXPECT_EQ(chosen, teraType == TYPE_GRASS ? SPECIES_ARCEUS_FIRE : SPECIES_ARCEUS_ROCK);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal form selection respects inverse matchups without mutating battlers")
{
    GIVEN {
        FLAG_SET(B_FLAG_INVERSE_BATTLE);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { HP(123); MaxHP(500); Ability(ABILITY_MULTITYPE); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT(FlagGet(B_FLAG_INVERSE_BATTLE));
        enum Type foeTypes[3];
        GetBattlerTypes(0, FALSE, foeTypes);
        for (u32 attackType = 0; attackType < NUMBER_OF_MON_TYPES; attackType++)
        {
            u32 count;
            u32 mask = ArceusRankedMask(attackType, foeTypes, &count);
            enum Species chosen;
            bool32 resisted = KoankoChooseArceusResistantForm(attackType, SPECIES_ARCEUS_NORMAL, foeTypes, &chosen);
            EXPECT_EQ(resisted, count != 0);
            if (resisted)
                EXPECT(mask & (1u << GetSpeciesType(chosen, 0)));
            else
                EXPECT_EQ(chosen, SPECIES_ARCEUS_NORMAL);
        }
        EXPECT_EQ(opponent->species, SPECIES_ARCEUS_NORMAL);
        EXPECT_EQ(opponent->ability, ABILITY_MULTITYPE);
        EXPECT_EQ(opponent->hp, 123);
        EXPECT_EQ(opponent->maxHP, 500);
        EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_SPECIES), SPECIES_ARCEUS_NORMAL);
    }
}

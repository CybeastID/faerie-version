#include "global.h"
#include "test/battle.h"
#include "battle_message.h"
#include "kazuradrop_battle_ui.h"
#include "malloc.h"
#include "text.h"
#include "window.h"
#include "util.h"

extern const u8 *Test_GetKazuraDialogueText(void);
extern u32 Test_GetKazuraDialogueWindow(void);

// Exercise the lifetime beyond the caller's return, unlike the headless script
// shortcut which records the ROM string without starting a real text printer.
static NOINLINE void DialogueStackChurn(u8 value)
{
    volatile u8 scratch[600];
    for (u32 i = 0; i < sizeof(scratch); i++) scratch[i] = value;
}

SINGLE_BATTLE_TEST("Kazuradrop dialogue real asynchronous printer retains text through stack reuse and dismissal")
{
    u32 stringId;
    PARAMETRIZE { stringId = STRINGID_KAZURA_LEGEND_TIER1; }
    PARAMETRIZE { stringId = STRINGID_KAZURA_LEGEND_TIER2; }
    PARAMETRIZE { stringId = STRINGID_TAUNTEDBYKAZURA; }
    PARAMETRIZE { stringId = STRINGID_BUGSPACEKAZURABLOCKEDFIRST; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        TextFlags savedFlags = gTextFlags;
        gTextFlags.autoScroll = TRUE;
        u32 crc = 0;
        for (u32 run = 0; run < 2; run++)
        {
            ShowKazuraDialogueBox(gBattleStringsTable[stringId]);
            u32 window = Test_GetKazuraDialogueWindow();
            const u8 *text = Test_GetKazuraDialogueText();
            EXPECT_NE(window, WINDOW_NONE);
            EXPECT(text != NULL);
            EXPECT_GE((uintptr_t)text, EWRAM_START);
            EXPECT_LT((uintptr_t)text, EWRAM_START + 0x40000);
            EXPECT(IsTextPrinterActiveOnWindow(window));
            DialogueStackChurn(run ? 0xAA : 0x55);
            u32 frames = 0;
            while (IsTextPrinterActiveOnWindow(window) && frames++ < 3000)
                RunTextPrinters();
            EXPECT_LT(frames, 3000);
            u32 rendered = CalcCRC16((const u8 *)GetWindowAttribute(window, WINDOW_TILE_DATA), 28 * 4 * TILE_SIZE_4BPP);
            if (run == 0) crc = rendered;
            else EXPECT_EQ(rendered, crc);
            HideKazuraDialogueBox();
            EXPECT_EQ(Test_GetKazuraDialogueText(), NULL);
            EXPECT_EQ(Test_GetKazuraDialogueWindow(), WINDOW_NONE);
            EXPECT_EQ(IsTextPrinterActiveOnWindow(window), FALSE);
        }
        gTextFlags = savedFlags;
        HideKazuraDialogueBox(); // Repeatable exit/reshow cleanup.
    } AFTER_CLEANUP { HideKazuraDialogueBox(); }
}

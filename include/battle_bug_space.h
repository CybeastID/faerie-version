#ifndef GUARD_BATTLE_BUG_SPACE_H
#define GUARD_BATTLE_BUG_SPACE_H

enum BugSpaceMode
{
    BUGSPACE_MODE_KAZURADROP, // Zero preserves ordinary activation/fixture defaults.
    BUGSPACE_MODE_KOANKO_OPENING,
    BUGSPACE_MODE_KOANKO_CRUSH_ONLY,
};

bool32 BugSpaceHasDoomClock(void);
bool32 BugSpaceIsKazuradrop(void);
bool32 BugSpaceCrushBoosts(void);

#endif

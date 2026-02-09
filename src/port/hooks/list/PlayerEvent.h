#ifndef __LUA__
#include "port/hooks/impl/EventSystem.h"
#endif

DEFINE_EVENT(PlayerHealthChange,
    struct MarioState* m;
    s32 health;
);

DEFINE_EVENT(PlayerLivesChange,
    struct MarioState* m;
    s32 lives;
);
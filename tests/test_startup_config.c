#include <assert.h>

#include "../src/game_core/startup_config.h"

void test_startup_config(void)
{
    assert(STARTUP_SCREEN_WIDTH == 800);
    assert(STARTUP_SCREEN_HEIGHT == 450);
    assert(STARTUP_TARGET_FPS == 60);
    assert(STARTUP_DEFAULT_COLUMN_COUNT == 20);
}

#include <assert.h>

#include "../src/app_config.h"

void test_startup_config(void)
{
    assert(APP_SCREEN_WIDTH == 800);
    assert(APP_SCREEN_HEIGHT == 450);
    assert(APP_TARGET_FPS == 60);
    assert(APP_DEFAULT_BLOCK_COUNT == 20);
}

#include <stdlib.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

void test_gameplay_tick(void);
void test_player_motion(void);
void test_startup_config(void);
void test_world_gen(void);

int main(void)
{
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

    test_startup_config();
    test_player_motion();
    test_world_gen();
    test_gameplay_tick();

    return 0;
}

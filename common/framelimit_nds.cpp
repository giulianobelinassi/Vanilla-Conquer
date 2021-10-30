#include "framelimit.h"
#include "wwmouse.h"
#include "settings.h"

#include <nds.h>

extern WWMouseClass* WWMouse;

void Video_Render_Frame();

void Update_HWCursor();

void Frame_Limiter(bool force_render)
{
    static unsigned ticks = 0;
    static unsigned ticks_start;

    if (ticks == 0) {
        timerStart(1, ClockDivider_1024, 0, NULL);
        ticks += timerElapsed(1);
        ticks_start = ticks;
    }

    ticks += timerElapsed(1);
    unsigned now = ticks;
    unsigned ticks_dt = now - ticks_start;

    if (ticks_dt < 30) {
        // If FPS is too high the mouse glitches out, so we force
        // VSync in this case.
        swiWaitForVBlank();
    }
    Update_HWCursor();
    ticks_start = ticks;
}

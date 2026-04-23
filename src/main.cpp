#include <multigauge/App.h>

#include "platform/FileSystemEmscripten.h"
#include "platform/LoggerWebConsole.h"
#include "platform/TimeWeb.h"

#include <emscripten.h>
#include <cstdint>

namespace mg {
    LoggerWebConsole logger;
    FileSystemEmscripten fileSystem;
    TimeWeb clock;
}

Platform plat{ mg::fileSystem, mg::clock, &mg::logger };

static uint64_t lastMs = 0;

static void tick() {
    uint64_t now = mg::clock.getMillis();
    uint64_t dt = (lastMs == 0) ? 0 : (now - lastMs);
    lastMs = now;
    (void)dt;

    mg::frame();
}

int main() {
    if (!mg::init(plat)) return 1;

    emscripten_set_main_loop(tick, 0, 1);
    return 0;
}

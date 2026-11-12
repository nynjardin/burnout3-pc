/*
 * Screens that only ask for a press to go on, gone by themselves: the port
 * presses Cross for the player (ps2host::ScriptPress, as --press does) a
 * moment after one shows. Never in a scripted run (--virtual-time: its own
 * presses land on their frames, the pictures checked against a record).
 *
 *   - "Load successful" (string 0xCD) after a profile is loaded, a driver
 *     details message box with one row, CONTINUE (0x00449F10(box, ?, text)).
 *
 * The LAN's own (the EA buddy server's failure, SELECT EA PERSONA with one
 * persona) are in lan.cpp.
 */

#include "ps2host/host.h"
#include "ps2host/run.h"

void F_00449F10(lifted::Context& c);

namespace {

constexpr u32 kLoadSuccessful = 0xCDu;

void DriverDetailsBox(lifted::Context& c) {
    const u32 text = u32(c.r[6]);
    F_00449F10(c);
    if (text == kLoadSuccessful && !ps2host::VirtualClock()) {
        const u64 now = ps2host::FrameNumber();
        ps2host::ScriptPress("cross", now + 20, now + 26);
    }
}

}  // namespace

/* A button (--press's names: cross, down, ...) pressed for the player
   `after` frames from now (6 frames held); nothing in a scripted run. */
void PressSoon(const char* button, u64 after) {
    if (ps2host::VirtualClock()) return;
    const u64 now = ps2host::FrameNumber();
    ps2host::ScriptPress(button, now + after, now + after + 6);
}

void PressCrossSoon(u64 after) { PressSoon("cross", after); }

void RegisterSkipScreens() {
    ps2host::Override(0x00449F10u, DriverDetailsBox, "a driver details message box: \"Load successful\" goes by itself");
}

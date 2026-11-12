/*
 * ELIMINATOR as an online (LAN) game mode in CREATE GAME's GAME MODE - a
 * Single Race whose rules are the
 * World Tour's Eliminator (the last car of each lap out), as SINGLE EVENT's
 * ELIMINATOR (single_event.cpp) makes it offline.
 *
 * What the game has (2026-11-13, read in the code; the probe's log):
 *   - CREATE GAME's mode page (made by 0x0039D900, 0x0039DE20 or 0x0039E5F0
 *     as the mode asks): its GAME MODE row's strings at 0x004C9B30 (Single
 *     Race 0x2DA, Road Rage 0x2DD, Team Crush 0x2DF, Double Impact 0x2DB,
 *     Party Crash 0x2DE; slots 5-7 zero), the row's count at page+0x14 (5) and last index at +0x15 (4: left and right
 *     wrap on it),
 *     a greyed byte for each at page+0x98 (the team modes greyed), the mode's
 *     index at page+0x6E4 - which the page's other rows follow.
 *   - The game made (gcre, 0x00339F50(online, request); 0x00337D60 is gset, the same words): PARAMS are the
 *     request's words +0xC..+0x28 as signed decimals ("%d,"); +0x14 the mode (PARAMS[2]),
 *     +0x28 (PARAMS[7]) zero.
 *   - A game's record as the lobby sends it (parser 0x004090E8): PARAMS as
 *     text at +0x8C; the started game's (+ses) at 0x01EA32B0.
 *   - The online event's setup (0x0030BB70, 0x0030D1B0's kind 3): its rules
 *     in the holder 0x0051B088 (+0x1B4): Single Race's 0x0051B62C.
 *
 * Here:
 *   - Double Impact's slot (index 3), greyed on a LAN, shows ELIMINATOR (the
 *     game's 0x111) and can be chosen; chosen, the page is made as a Single
 *     Race (index 0: REGION, TRACK, LAPS, SERIES LOCK) - its index turned
 *     to 0 before the page's maker is picked (0x0039F090), Single Race's
 *     string then ELIMINATOR (the row shows the string of index 0);
 *   - the game made as a Single Race (mode 0) with PARAMS[7] = 'ELIM';
 *   - every player's setup of the race (0x0030BB70): a started game whose
 *     PARAMS[7] is 'ELIM' gets the Eliminator's rules (0x0051B2F0, +0xA4 5)
 *     in place of Single Race's.
 *
 * What it gives (2026-11-13, two instances, the race started): the page,
 * the game made, PARAMS[7] and both setups as above - but the race breaks:
 * "Lost connection to other players" on both PCs, each alone in first
 * place, one lap. The Eliminator's rules are the offline game's; the online
 * race's (0x0051B62C) carry its network side. Next: keep the online race's
 * rules and put the elimination in them (how the Eliminator's object takes
 * a car out at a lap's end), or find an online kind that has it. So the
 * whole mode is off unless B3_LAN_ELIMINATOR=1.
 */

#include <cstdio>
#include <cstdlib>
#include <string>

#include "ps2host/host.h"
#include "ps2host/run.h"

void F_0039D900(lifted::Context& c);
void F_0039DE20(lifted::Context& c);
void F_0039E5F0(lifted::Context& c);
void F_003A03B0(lifted::Context& c);
void F_0039F090(lifted::Context& c);

namespace {

constexpr u32 kModeStrings = 0x004C9B30u;       // GAME MODE's strings, by index
constexpr u32 kSingleRaceString = 0x2DAu, kDoubleImpactString = 0x2DBu, kEliminatorString = 0x111u;
constexpr u32 kSlot = 3;                        // Double Impact's index: ELIMINATOR's on a LAN
constexpr u32 kMagic = 0x454C494Du;             // 'ELIM', PARAMS[7]
constexpr u32 kHolder = 0x0051B088u;            // the online event's rules holder
constexpr u32 kSingleRaceRules = 0x0051B62Cu, kEliminatorRules = 0x0051B2F0u;
constexpr u32 kStartedGame = 0x01EA32B0u;       // the started game's record (+ses)

u32 g_taken_page = 0;           // the mode page whose Double Impact slot is ELIMINATOR's
bool g_eliminator = false;      // ELIMINATOR chosen: the page made as a Single Race

void Log(const std::string& text) { std::fprintf(stderr, "[eliminator] %s\n", text.c_str()); }

/* CREATE GAME's pages set up (0x003A03B0(page, ...): as it opens, and again
   from 0x0030F2A4 once its pages are done): Double Impact's slot taken
   afresh - one the game offers this time stays. ELIMINATOR, if chosen, stays
   chosen (the game keeps its mode the same way). */
void PagesSetUp(lifted::Context& c) {
    g_taken_page = 0;
    F_003A03B0(c);
}

/* ELIMINATOR's slot chosen (index 3 on a page whose slot is taken): the page
   made as a Single Race from now on (index 0), the port remembering it. */
void TakeSlot(u32 page, const char* by) {
    const s32 index = s32(generated::ReadU32(page + 0x6E4));
    const bool was = g_eliminator;
    if (g_taken_page == page && index == s32(kSlot)) {
        g_eliminator = true;
        generated::Write32(page + 0x6E4, 0);
    } else if (index != 0) {
        g_eliminator = false;
    }
    if (was != g_eliminator) {
        char text[120];
        std::snprintf(text, sizeof text, "%s: page %08X (page %u) index %d: ELIMINATOR %s", by, page,
                      unsigned(generated::ReadU16(page + 0x4)), int(index), g_eliminator ? "on" : "off");
        Log(text);
    }
    generated::Write32(kModeStrings, g_eliminator ? kEliminatorString : kSingleRaceString);
}

/* 0x0039F090(page): CREATE GAME's pages, each frame - the mode page's maker
   picked by the mode's index (page+0x6E4) before it is made. */
void Pages(lifted::Context& c) {
    TakeSlot(u32(c.r[4]), "pages");
    F_0039F090(c);
}

/* The mode page made (first shown, or its mode changed). Double Impact's
   slot (3), greyed on a LAN (a team mode), becomes ELIMINATOR for as long
   as CREATE GAME is open (the game does not grey it again when the page is
   made again). The selector's own bounds stay five (setting its count after
   the page is made does not reach it: tried). ELIMINATOR chosen: the page
   made with index 0, a Single Race's rows (Double Impact's are a crash's,
   JUNCTION and ROUNDS), the selector still on ELIMINATOR. Left and right
   (0x0039F490) change the index and call the maker for it at once: the
   crash's maker asked for ELIMINATOR, the race's (0x0039E5F0) makes it. */
template <void (*Original)(lifted::Context&)>
void ModePage(lifted::Context& c) {
    const u32 page = u32(c.r[4]);
    const s32 index = s32(generated::ReadU32(page + 0x6E4));
    TakeSlot(page, "mode page");
    if (g_eliminator) F_0039E5F0(c);
    else Original(c);
    if (generated::ReadU32(page + 0x10) == kModeStrings &&
        (g_taken_page == page || generated::ReadU8(page + 0x98 + kSlot) != 0)) {
        g_taken_page = page;
        generated::Write8(page + 0x98 + kSlot, 0);
        generated::Write32(kModeStrings + 4 * kSlot, kEliminatorString);
    } else if (g_taken_page != page) {
        generated::Write32(kModeStrings + 4 * kSlot, kDoubleImpactString);
    }
    static int told = 0;
    if (told++ < 40)
        Log("mode page: index " + std::to_string(index) + " -> " +
            std::to_string(s32(generated::ReadU32(page + 0x6E4))) + (g_eliminator ? ", ELIMINATOR" : "") +
            (g_taken_page == page ? " (slot 3 ELIMINATOR's)" : ""));
}

bool g_enabled = false;         // B3_LAN_ELIMINATOR=1

}  // namespace

/* gcre (stay_together.cpp's hook on 0x00339F50, before the game's): ELIMINATOR
   made as a Single Race carrying 'ELIM' in PARAMS[7]. */
void EliminatorMakeGame(u32 request) {
    if (!g_enabled) return;
    Log("gcre: mode " + std::to_string(generated::ReadU32(request + 0x14)) + (g_eliminator ? ", ELIMINATOR" : ""));
    if (g_eliminator && generated::ReadU32(request + 0x14) == 0) {
        generated::Write32(request + 0x28, kMagic);
        Log("game made as ELIMINATOR: a Single Race, PARAMS[7] 'ELIM'");
    }
}

namespace {

/* PARAMS[n] of a record's PARAMS text ("a,b,c,..." signed decimals). */
u32 Param(u32 record, int n) {
    std::string text;
    for (u32 i = 0; i < 0x84; ++i) {
        const u8 ch = generated::ReadU8(record + 0x8C + i);
        if (!ch) break;
        text += char(ch);
    }
    std::size_t at = 0;
    for (int i = 0; i < n && at != std::string::npos; ++i) {
        at = text.find(',', at);
        if (at != std::string::npos) ++at;
    }
    if (at == std::string::npos || at >= text.size()) return 0;
    return u32(std::strtol(text.c_str() + at, nullptr, 10));
}

}  // namespace

/* The online race set up (stay_together.cpp's hook on 0x0030BB70, after the
   game's): an ELIMINATOR game's Single Race gets the Eliminator's rules. */
void EliminatorRaceSetUp() {
    if (!g_enabled) return;
    const u32 rules = generated::ReadU32(kHolder + 0x1B4);
    const u32 flag = Param(kStartedGame, 7);
    char text[160];
    std::snprintf(text, sizeof text, "online race set up: rules %08X, PARAMS[2] %X, PARAMS[7] %X", rules,
                  Param(kStartedGame, 2), flag);
    Log(text);
    if (flag != kMagic || rules != kSingleRaceRules) return;
    generated::Write32(kHolder + 0x1B4, kEliminatorRules);
    generated::Write32(kHolder + 0xA4, 5);
    Log("ELIMINATOR: the Eliminator's rules for this race");
}

/* EXPERIMENTAL, off unless B3_LAN_ELIMINATOR=1: the race does not hold with
   the Eliminator's rules swapped in (2026-11-13, two instances: "Lost
   connection to other players" on both, each alone in first place, one lap)
   - they are the offline game's, without the online race's network side. */
void RegisterOnlineEliminator() {
    const char* on = std::getenv("B3_LAN_ELIMINATOR");
    if (!on || !*on || *on == '0') return;
    g_enabled = true;
    ps2host::Override(0x0039D900u, ModePage<F_0039D900>, "CREATE GAME's mode page: ELIMINATOR in Double Impact's slot");
    ps2host::Override(0x0039DE20u, ModePage<F_0039DE20>, "CREATE GAME's mode page: ELIMINATOR in Double Impact's slot");
    ps2host::Override(0x0039E5F0u, ModePage<F_0039E5F0>, "CREATE GAME's mode page: ELIMINATOR in Double Impact's slot");
    ps2host::Override(0x0039F090u, Pages, "CREATE GAME's pages: ELIMINATOR's slot made as a Single Race");
    ps2host::Override(0x003A03B0u, PagesSetUp, "CREATE GAME's pages set up: ELIMINATOR's slot free again");
}

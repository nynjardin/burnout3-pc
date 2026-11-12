/*
 * The players of a LAN game stay together (the idea is Takedawn's): after a
 * race, once each player has gone through the results (Continue To Lobby),
 * the PC that made the game makes it again with the same settings, and the
 * others join it - the port presses for them. B3_LAN_STAY=0 turns it off.
 *
 * What the game has (2026-11-13, read in the code and seen on two instances):
 *   - after a race the game signs in again and comes back to the room's
 *     menu, CREATE GAME / JOIN GAME / CHAT / VIEW PLAYERS (made by
 *     0x00396E50(screen): the cursor at +0x2C on CREATE GAME);
 *   - CREATE GAME: three pages (name, mode, car), Cross on each; the game
 *     made by gcre (0x00339F50(online, request): PARAMS the request's words
 *     +0xC..+0x28). The settings are not kept: a Road Rage came back as a
 *     Single Race;
 *   - JOIN GAME: ACTIVE RACES (0x00329330(screen) each frame, the games'
 *     count at +0x1B08), Cross on a game, then its car page;
 *   - the online race set up by 0x0030BB70 (every player's).
 *
 * Here:
 *   - gcre: this PC made the game (its role, if the game starts: host); its
 *     request's words kept, and when the game is made again after a race
 *     they are the request's again (the same mode, track, laps...);
 *   - the race set up: the role taken (host if this PC made the game since
 *     it was last in the room, else guest);
 *   - the room's menu made after a race: the host presses Cross four times
 *     (CREATE GAME, name, mode, car), the guest Down and Cross (JOIN GAME),
 *     then, once ACTIVE RACES lists a game for a second, Cross on the first
 *     and on its car. The presses are counted in the game's frames, so a
 *     slow PC's pages, which come in frames too, keep up.
 *   - lan.cpp's ELIMINATOR (online_eliminator.cpp) is called from here: the
 *     two hooks are one.
 * Not handled: more than one open game (the guest takes the first listed).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "ps2host/host.h"
#include "ps2host/run.h"

void F_00339F50(lifted::Context& c);
void F_0030BB70(lifted::Context& c);
void F_00396E50(lifted::Context& c);
void F_00329330(lifted::Context& c);

bool LanEnabled();                          // iop_servers.cpp
void PressSoon(const char* button, u64 after);   // skip_screens.cpp
void EliminatorMakeGame(u32 request);       // online_eliminator.cpp
void EliminatorRaceSetUp();

namespace {

enum class Role { kNone, kHost, kGuest };

bool g_on = true;               // B3_LAN_STAY
bool g_made = false;            // this PC made a game since it was last in the room
bool g_raced = false;           // a race was set up since the room's menu was last made
Role g_role = Role::kNone;      // this PC's in the last race
bool g_again = false;           // the host makes the game again: the next gcre takes the kept words
u32 g_words[8] = {};            // the last game's request, +0xC..+0x28
bool g_have_words = false;
int g_waiting = -1;             // the guest on ACTIVE RACES: frames a game has been listed (-1: not waiting)

void Log(const std::string& text) { std::fprintf(stderr, "[stay] %s\n", text.c_str()); }

/* gcre: this PC's game. Made again after a race: the last game's words. */
void MakeGame(lifted::Context& c) {
    const u32 request = u32(c.r[5]);
    if (g_again && g_have_words) {
        for (u32 i = 0; i < 8; ++i) generated::Write32(request + 0xC + 4 * i, g_words[i]);
        Log("the game made again with the last one's settings");
    }
    g_again = false;
    EliminatorMakeGame(request);
    for (u32 i = 0; i < 8; ++i) g_words[i] = generated::ReadU32(request + 0xC + 4 * i);
    g_have_words = true;
    g_made = true;
    F_00339F50(c);
}

/* The online race set up: this PC's role in it. */
void RaceSetUp(lifted::Context& c) {
    F_0030BB70(c);
    EliminatorRaceSetUp();
    g_raced = true;
    g_role = g_made ? Role::kHost : Role::kGuest;
    Log(std::string("race: this PC is the ") + (g_role == Role::kHost ? "host" : "guest"));
}

/* The room's menu made: back from a race, the players go on together. */
void RoomMenu(lifted::Context& c) {
    F_00396E50(c);
    const bool after_race = g_raced;
    g_raced = false;
    g_made = false;
    g_waiting = -1;
    if (!after_race || !g_on || !LanEnabled()) return;
    if (g_role == Role::kHost) {
        Log("back from the race: making the game again");
        g_again = true;
        PressSoon("cross", 45);             // CREATE GAME
        PressSoon("cross", 45 + 300);       // the name page
        PressSoon("cross", 45 + 600);       // the mode page
        PressSoon("cross", 45 + 960);       // the car
    } else if (g_role == Role::kGuest) {
        Log("back from the race: joining the host's game when it is listed");
        PressSoon("down", 45);
        PressSoon("cross", 75);             // JOIN GAME
        g_waiting = 0;
    }
}

/* ACTIVE RACES, each frame: the guest takes the first game once one has been
   listed for a second, then its car. */
void ActiveRaces(lifted::Context& c) {
    const u32 screen = u32(c.r[4]);
    F_00329330(c);
    if (g_waiting < 0) return;
    if (generated::ReadU32(screen + 0x1B08) == 0) {
        g_waiting = 0;
        return;
    }
    if (++g_waiting < 60) return;
    g_waiting = -1;
    Log("the host's game listed: joining it");
    PressSoon("cross", 1);                  // the game
    PressSoon("cross", 1 + 360);            // the car
}

}  // namespace

void RegisterStayTogether() {
    const char* value = std::getenv("B3_LAN_STAY");
    g_on = !(value && *value == '0');
    ps2host::Override(0x00339F50u, MakeGame, "gcre: this PC's game (staying together; ELIMINATOR's PARAMS[7])");
    ps2host::Override(0x0030BB70u, RaceSetUp, "the online race's setup: this PC's role (staying together; ELIMINATOR)");
    ps2host::Override(0x00396E50u, RoomMenu, "the room's menu: back from a race, the players go on together");
    ps2host::Override(0x00329330u, ActiveRaces, "ACTIVE RACES: a guest staying together joins the host's game");
}

//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:  none
//


#include <stdlib.h>
#include <math.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_loop.h"

#include "deh_str.h"
#include "deh_misc.h"

#include "z_zone.h"
#include "f_finale.h"
#include "m_argv.h"
#include "m_controls.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"
#include "i_system.h"
#include "i_input.h"
#include "i_swap.h"

#include "p_setup.h"
#include "p_saveg.h"
#include "p_tick.h"

#include "d_main.h"

#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "statdump.h"

// Needs access to LFB.
#include "v_video.h"


#include "p_local.h" 

#include "s_sound.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

// SKY handling - still the wrong place.
#include "r_data.h"
#include "r_sky.h"



#include "g_game.h"


#define SAVEGAMESIZE	0x2c000

void G_ReadDemoTiccmd(ticcmd_t* cmd);
void G_WriteDemoTiccmd(ticcmd_t* cmd);
void G_PlayerReborn(int playernum);

void G_DoReborn(int playernum);

void G_DoLoadLevel(void);
void G_DoNewGame(void);
void G_DoPlayDemo(void);
void G_DoCompleted(void);
void G_DoWorldDone(void);
void G_DoSaveGame(void);

//
// Gamestate the last time G_Ticker was called.
//
static gamestate_t oldgamestate;

gameaction_t gameaction;
gamestate_t gamestate;
skill_t gameskill;
bool respawnmonsters;
int gameepisode;
int gamemap;

//
// If non-zero, exit the level after this number of minutes.
//
int timelimit;

bool paused;
//
// Send a pause event next tic.
//
bool sendpause;
//
// Send a save event next tic.
//
static bool sendsave;
//
// Ok to save/end game.
//
bool usergame;

//
// If true, exit with report on completion.
//
static bool timingdemo;
//
// For comparative timing purposes.
//
bool nodrawers;
//
// For comparative timing purposes.
//
static int starttime;

bool viewactive;

int deathmatch; // only if started as net death
bool netgame;   // only true if packets are broadcast
bool playeringame[MAXPLAYERS];
player_t players[MAXPLAYERS];

static bool turbodetected[MAXPLAYERS];

int consoleplayer;                       // player taking events and displaying
int displayplayer;                       // view being displayed
int totalkills; // for intermission
int totalitems; // for intermission
int totalsecret; // for intermission

static char* demoname;
bool demorecording;
static bool longtics;    // cph's doom 1.91 longtics hack
bool lowres_turn; // low resolution turning for longtics
bool demoplayback;
static bool netdemo;
static byte *demobuffer;
static byte *demo_p;
static byte *demoend;
bool singledemo; // quit after playing a demo from cmdline

bool precache = true; // if true, load all graphics at start

bool testcontrols = false; // Invoked by setup to test controls
int testcontrols_mousespeed;


wbstartstruct_t wminfo; // parms for world map / intermission

static byte consistancy[MAXPLAYERS][BACKUPTICS];

#define MAXPLMOVE (forwardmove[1])

#define TURBOTHRESHOLD 0x32

fixed_t forwardmove[2] = {0x19, 0x32};
fixed_t sidemove[2] = {0x18, 0x28};
static fixed_t angleturn[3] = {640, 1280, 320}; // + slow turn

static int* weapon_keys[] = {
    &key_weapon1,
    &key_weapon2,
    &key_weapon3,
    &key_weapon4,
    &key_weapon5,
    &key_weapon6,
    &key_weapon7,
    &key_weapon8
};

//
// Set to -1 or +1 to switch to the previous or next weapon.
//
static int next_weapon = 0;

//
// Used for prev/next weapon keys.
//
static const struct
{
    weapontype_t weapon;
    weapontype_t weapon_num;
} weapon_order_table[] = {
    {wp_fist, wp_fist},
    {wp_chainsaw, wp_fist},
    {wp_pistol, wp_pistol},
    {wp_shotgun, wp_shotgun},
    {wp_supershotgun, wp_shotgun},
    {wp_chaingun, wp_chaingun},
    {wp_missile, wp_missile},
    {wp_plasma, wp_plasma},
    {wp_bfg, wp_bfg}
};

#define WEAPON_TABLE_SIZE (arrlen(weapon_order_table))

#define SLOWTURNTICS 6

#define NUMKEYS         256
#define MAX_JOY_BUTTONS 20

static bool gamekeydown[NUMKEYS];
static int turnheld; // for accelerative turning

static bool mousearray[MAX_MOUSE_BUTTONS + 1];
static bool *mousebuttons = &mousearray[1]; // allow [-1]

// mouse values are used once
static int mousex;
static int mousey;

static int dclicktime;
static bool dclickstate;
static int dclicks;
static int dclicktime2;
static bool dclickstate2;
static int dclicks2;

// joystick values are repeated
static int joyxmove;
static int joyymove;
static int joystrafemove;
static bool joyarray[MAX_JOY_BUTTONS + 1];
static bool *joybuttons = &joyarray[1]; // allow [-1]

static int savegameslot;
static char savedescription[32];

#define BODYQUESIZE 32

mobj_t *bodyque[BODYQUESIZE];
int bodyqueslot;

int vanilla_savegame_limit = 1;
int vanilla_demo_limit = 1;

//
// If the player doesn't own the chainsaw, then we can select the fist.
// If the player does own the chainsaw, only select the fist if we also
// have the berserk pack.
//
static bool I_CanSelectFist() {
    const player_t* player = &players[consoleplayer];
    if (player->weaponowned[wp_chainsaw]) {
        return player->powers[pw_strength] != 0;
    }
    return true;
}

static bool I_PlayerOwnsWeapon(weapontype_t weapon) {
    return players[consoleplayer].weaponowned[weapon];
}

static bool I_WeaponAvailable(weapontype_t weapon) {
    if (logical_gamemission == doom && weapon == wp_supershotgun) {
        // Can't select the super shotgun in Doom 1.
        return false;
    }
    if (gamemission == doom && gamemode == shareware) {
        // These weapons aren't available in shareware.
        return weapon != wp_plasma && weapon != wp_bfg;
    }
    return true;
}

static bool I_WeaponSelectable(weapontype_t weapon) {
    return I_WeaponAvailable(weapon)
           && I_PlayerOwnsWeapon(weapon)
           && (weapon != wp_fist || I_CanSelectFist());
}

static int G_SwitchWeapon(int weapon_idx, int direction) {
    int next_weapon = weapon_idx + direction;
    next_weapon = (next_weapon + WEAPON_TABLE_SIZE) % WEAPON_TABLE_SIZE;
    return next_weapon;
}

static int G_FindPlayerWeaponIndex() {
    weapontype_t weapon;
    int i;

    if (players[consoleplayer].pendingweapon == wp_nochange) {
        weapon = players[consoleplayer].readyweapon;
    } else {
        weapon = players[consoleplayer].pendingweapon;
    }

    for (i = 0; i < WEAPON_TABLE_SIZE; ++i) {
        if (weapon_order_table[i].weapon == weapon) {
            break;
        }
    }

    // The current weapon must be present in weapon_order_table
    // otherwise something has gone terribly wrong
    if (i >= arrlen(weapon_order_table)) {
        I_Error("Internal error: weapon %d not present in weapon_order_table", weapon);
    }

    return i;
}

static int G_NextWeapon(int direction) {
    int current_weapon = G_FindPlayerWeaponIndex();
    int next_weapon = current_weapon;

    // Switch weapon. Don't loop forever.
    do {
        next_weapon = G_SwitchWeapon(next_weapon, direction);
        weapontype_t next_weapon_type = weapon_order_table[next_weapon].weapon;
        if (I_WeaponSelectable(next_weapon_type)) {
            break;
        }
    } while (next_weapon != current_weapon);

    return weapon_order_table[next_weapon].weapon_num;
}

static bool G_CheckStrafeDoubleClick() {
    bool bstrafe = mousebuttons[mousebstrafe] || joybuttons[joybstrafe];
    if (bstrafe == dclickstate2 || dclicktime2 <= 1) {
        dclicktime2 += ticdup;
        if (dclicktime2 > 20) {
            dclicks2 = 0;
            dclickstate2 = 0;
        }
        return false;
    }
    dclickstate2 = bstrafe;
    if (dclickstate2) {
        dclicks2++;
    }
    if (dclicks2 == 2) {
        dclicks2 = 0;
        return true;
    }
    dclicktime2 = 0;
    return false;
}

static bool G_CheckForwardDoubleClick() {
    if (mousebuttons[mousebforward] == dclickstate || dclicktime <= 1) {
        dclicktime += ticdup;
        if (dclicktime > 20) {
            dclicks = 0;
            dclickstate = false;
        }
        return false;
    }
    dclickstate = mousebuttons[mousebforward];
    if (dclickstate) {
        dclicks++;
    }
    if (dclicks == 2) {
        dclicks = 0;
        return true;
    }
    dclicktime = 0;
    return false;
}

static bool G_IsDoubleClicking(bool hit_use_button) {
    if (hit_use_button) {
        // clear double clicks if hit use button
        dclicks = 0;
    }
    // Avoid short-circuit behavior so that both functions run
    // even if one evaluates to false.
    bool forward_db_click = G_CheckForwardDoubleClick();
    bool strafe_db_click = G_CheckStrafeDoubleClick();
    return forward_db_click || strafe_db_click;
}

static bool G_IsHittingUseButton() {
    bool hit_use_button =
        gamekeydown[key_use] || joybuttons[joybuse] || mousebuttons[mousebuse];

    bool double_clicked = dclick_use && G_IsDoubleClicking(hit_use_button);

    return hit_use_button || double_clicked;
}

static bool G_IsFiringWeapon() {
    return gamekeydown[key_fire]
           || mousebuttons[mousebfire]
           || joybuttons[joybfire];
}

static bool G_IsTurning() {
    return joyxmove < 0
        || joyxmove > 0
        || gamekeydown[key_right]
        || gamekeydown[key_left]
        || mousebuttons[mousebturnright]
        || mousebuttons[mousebturnleft];
}

static bool G_IsRunning() {
    return key_speed >= NUMKEYS
           || joybspeed >= MAX_JOY_BUTTONS
           || gamekeydown[key_speed]
           || joybuttons[joybspeed]
           || mousebuttons[mousebspeed];
}

static bool G_IsStrafingRight() {
    return gamekeydown[key_straferight]
           || joybuttons[joybstraferight]
           || mousebuttons[mousebstraferight]
           || joystrafemove > 0;
}

static bool G_IsStrafingLeft() {
    return gamekeydown[key_strafeleft]
           || joybuttons[joybstrafeleft]
           || mousebuttons[mousebstrafeleft]
           || joystrafemove < 0;
}

static bool G_IsStrafing() {
    return gamekeydown[key_strafe]
           || mousebuttons[mousebstrafe]
           || joybuttons[joybstrafe];
}

static void G_SetSpecialButtons(ticcmd_t* cmd) {
    if (sendpause) {
        sendpause = false;
        cmd->buttons = BT_SPECIAL | BTS_PAUSE;
    }
    if (sendsave) {
        sendsave = false;
        cmd->buttons = BT_SPECIAL | BTS_SAVEGAME | (savegameslot<<BTS_SAVESHIFT);
    }
}

//
// If the previous or next weapon button is pressed, the next_weapon variable
// is set to change weapons when we generate a ticcmd. Choose a new weapon.
//
static void G_SetChangeWeaponButtons(ticcmd_t* cmd) {
    if (gamestate == GS_LEVEL && next_weapon != 0) {
        int i = G_NextWeapon(next_weapon);
        cmd->buttons |= BT_CHANGE;
        cmd->buttons |= i << BT_WEAPONSHIFT;
        return;
    }
    // Check weapon keys.
    for (int i = 0; i < arrlen(weapon_keys); ++i) {
        int key = *weapon_keys[i];
        if (gamekeydown[key]) {
            cmd->buttons |= BT_CHANGE;
            cmd->buttons |= i << BT_WEAPONSHIFT;
            break;
        }
    }
}

static void G_SetButtons(ticcmd_t* cmd) {
    if (G_IsFiringWeapon()) {
        cmd->buttons |= BT_ATTACK;
    }
    if (G_IsHittingUseButton()) {
        cmd->buttons |= BT_USE;
    }
    G_SetChangeWeaponButtons(cmd);
    G_SetSpecialButtons(cmd);
}

static void G_SetChatChar(ticcmd_t* cmd) {
    cmd->chatchar = HU_dequeueChatChar();
}

//
// low-res turning
//
static short G_DoLowResTurning(short turn) {
    static signed short carry = 0;

    signed short desired_angleturn = turn + carry;

    // round angleturn to the nearest 256 unit boundary
    // for recording demos with single byte values for turn
    short low_turn = (desired_angleturn + 128) & 0xff00;

    // Carry forward the error from the reduced resolution to the
    // next tic, so that successive small movements can accumulate.
    carry = desired_angleturn - low_turn;

    return low_turn;
}

//
// Use two stage accelerative turning on the keyboard and joystick.
//
static void G_UpdateTurnHeld() {
    if (G_IsTurning()) {
        turnheld += ticdup;
    } else {
        turnheld = 0;
    }
}

static int G_GetTurningSpeed() {
    if (turnheld < SLOWTURNTICS) {
        // slow turn
        return 2;
    }
    return G_IsRunning();
}

static void G_SetAngleTurn(ticcmd_t* cmd) {
    G_UpdateTurnHeld();

    if (G_IsStrafing()) {
        cmd->angleturn = 0;
        return;
    }

    int turn_speed = G_GetTurningSpeed();
    short turn = 0;
    if (gamekeydown[key_right] || mousebuttons[mousebturnright]) {
        turn -= angleturn[turn_speed];
    }
    if (gamekeydown[key_left] || mousebuttons[mousebturnleft]) {
        turn += angleturn[turn_speed];
    }
    if (joyxmove > 0) {
        turn -= angleturn[turn_speed];
    }
    if (joyxmove < 0) {
        turn += angleturn[turn_speed];
    }
    turn -= mousex * 0x8;

    if (lowres_turn) {
        cmd->angleturn = G_DoLowResTurning(turn);
    } else {
        cmd->angleturn = turn;
    }
}

static void G_SetForwardMove(ticcmd_t* cmd) {
    bool speed = G_IsRunning();
    int forward = 0;

    if (gamekeydown[key_up]) {
        forward += forwardmove[speed];
    }
    if (gamekeydown[key_down]) {
        forward -= forwardmove[speed];
    }
    if (joyymove < 0) {
        forward += forwardmove[speed];
    }
    if (joyymove > 0) {
        forward -= forwardmove[speed];
    }
    // mouse
    if (mousebuttons[mousebforward]) {
        forward += forwardmove[speed];
    }
    if (mousebuttons[mousebbackward]) {
        forward -= forwardmove[speed];
    }
    forward += mousey;

    if (forward > MAXPLMOVE) {
        forward = MAXPLMOVE;
    } else if (forward < -MAXPLMOVE) {
        forward = -MAXPLMOVE;
    }

    cmd->forwardmove += forward;
}

static void G_SetSideMove(ticcmd_t* cmd) {
    bool speed = G_IsRunning();
    int side = 0;

    // let movement keys cancel each other out
    if (G_IsStrafing()) {
        if (gamekeydown[key_right] || mousebuttons[mousebturnright]) {
            side += sidemove[speed];
        }
        if (gamekeydown[key_left] || mousebuttons[mousebturnleft]) {
            side -= sidemove[speed];
        }
        if (joyxmove > 0) {
            side += sidemove[speed];
        }
        if (joyxmove < 0) {
            side -= sidemove[speed];
        }
        side += mousex * 2;
    }
    if (G_IsStrafingLeft()) {
        side -= sidemove[speed];
    }
    if (G_IsStrafingRight()) {
        side += sidemove[speed];
    }

    if (side > MAXPLMOVE) {
        side = MAXPLMOVE;
    } else if (side < -MAXPLMOVE) {
        side = -MAXPLMOVE;
    }

    cmd->sidemove += side;
}

static void G_SetConsistancy(ticcmd_t* cmd, int maketic) {
    cmd->consistancy = consistancy[consoleplayer][maketic % BACKUPTICS];
}

//
// G_BuildTicCmd
// Builds a ticcmd from all of the available inputs or reads it
// from the demo buffer. If recording a demo, write it out
// 
void G_BuildTicCmd(ticcmd_t* cmd, int maketic) {
    memset(cmd, 0, sizeof(ticcmd_t));

    G_SetConsistancy(cmd, maketic);
    G_SetSideMove(cmd);
    G_SetForwardMove(cmd);
    G_SetAngleTurn(cmd);
    G_SetChatChar(cmd);
    G_SetButtons(cmd);

    next_weapon = 0;
    if (mousex == 0) {
        // No movement in the previous frame.
        testcontrols_mousespeed = 0;
    }
    mousex = 0;
    mousey = 0;
}

static void G_SetSkyTexture() {
    const char* skytexturename;
    if (gamemap < 12) {
        skytexturename = "SKY1";
    } else if (gamemap < 21) {
        skytexturename = "SKY2";
    } else {
        skytexturename = "SKY3";
    }
    skytexturename = DEH_String(skytexturename);

    sky_tex = R_TextureNumForName(skytexturename);
}

//
// Clear cmd building stuff.
//
static void G_ClearCmdStuff() {
    joyxmove = 0;
    joyymove = 0;
    joystrafemove = 0;

    mousex = 0;
    mousey = 0;

    sendpause = false;
    sendsave = false;
    paused = false;

    memset(gamekeydown, 0, sizeof(gamekeydown));
    memset(mousearray, 0, sizeof(mousearray));
    memset(joyarray, 0, sizeof(joyarray));
}

static void G_ResetPlayer() {
    for (int i = 0; i < MAXPLAYERS; i++) {
        turbodetected[i] = false;
        if (playeringame[i] && players[i].playerstate == PST_DEAD) {
            players[i].playerstate = PST_REBORN;
        }
        memset(players[i].frags, 0, sizeof(players[i].frags));
    }
}

static bool G_ShouldSetSkyTexture() {
    if (gamemode != commercial) {
        // In Doom 1, the sky is already set in "G_InitNew".
        return false;
    }
    if (gameversion == exe_final2 || gameversion == exe_chex) {
        // The "Sky never changes in Doom II" bug was fixed in
        // the id Anthology version of doom2.exe for Final Doom.
        return true;
    }
    return false;
}

//
// Set the sky map.
// First thing, we have a dummy sky texture name, a flat. The data is in the
// WAD only because we look for an actual index, instead of simply setting one.
//
static void G_SetSky() {
    sky_flat = R_FlatNumForName(DEH_String(SKYFLATNAME));
    if (G_ShouldSetSkyTexture()) {
        G_SetSkyTexture();
    }
}

//
// G_DoLoadLevel 
//
void G_DoLoadLevel() {
    G_SetSky();

    if (wipegamestate == GS_LEVEL) {
        // Force a wipe.
        wipegamestate = -1;
    }

    gamestate = GS_LEVEL;

    G_ResetPlayer();

    P_SetupLevel(gameepisode, gamemap);
    displayplayer = consoleplayer; // view the guy you are playing
    gameaction = ga_nothing;
    Z_CheckHeap();

    G_ClearCmdStuff();

    if (testcontrols) {
        players[consoleplayer].message = "Press escape to quit.";
    }
}

static void SetJoyButtons(unsigned int buttons_mask)
{
    int i;

    for (i=0; i<MAX_JOY_BUTTONS; ++i)
    {
        int button_on = (buttons_mask & (1 << i)) != 0;

        // Detect button press:

        if (!joybuttons[i] && button_on)
        {
            // Weapon cycling:

            if (i == joybprevweapon)
            {
                next_weapon = -1;
            }
            else if (i == joybnextweapon)
            {
                next_weapon = 1;
            }
        }

        joybuttons[i] = button_on;
    }
}

static void SetMouseButtons(unsigned int buttons_mask)
{
    int i;

    for (i=0; i<MAX_MOUSE_BUTTONS; ++i)
    {
        unsigned int button_on = (buttons_mask & (1 << i)) != 0;

        // Detect button press:

        if (!mousebuttons[i] && button_on)
        {
            if (i == mousebprevweapon)
            {
                next_weapon = -1;
            }
            else if (i == mousebnextweapon)
            {
                next_weapon = 1;
            }
        }

	mousebuttons[i] = button_on;
    }
}

//
// G_Responder  
// Get info needed to make ticcmd_ts for the players.
//
bool G_Responder(event_t *ev) {
    // allow spy mode changes even during the demo
    if (gamestate == GS_LEVEL && ev->type == ev_keydown &&
        ev->data1 == key_spy && (singledemo || !deathmatch))
    {
        // spy mode
        do {
            displayplayer++;
            if (displayplayer == MAXPLAYERS) {
                displayplayer = 0;
            }
        } while (!playeringame[displayplayer] && displayplayer != consoleplayer);
        return true;
    }

    // any other key pops up menu if in demos
    if (gameaction == ga_nothing && !singledemo &&
        (demoplayback || gamestate == GS_DEMOSCREEN))
    {
        if (ev->type == ev_keydown || (ev->type == ev_mouse && ev->data1) ||
            (ev->type == ev_joystick && ev->data1))
        {
            M_StartControlPanel();
            return true;
        }
        return false;
    }

    if (gamestate == GS_LEVEL) {
        if (HU_Responder(ev)) {
            // chat ate the event
            return true;
        }
        if (ST_Responder(ev)) {
            // status window ate it
            return true;
        }
        if (AM_Responder(ev)) {
            // automap ate it
            return true;
        }
    }

    if (gamestate == GS_FINALE) {
        if (F_Responder(ev)) {
            // finale ate the event
            return true;
        }
    }

    if (testcontrols && ev->type == ev_mouse) {
        // If we are invoked by setup to test the controls, save the
        // mouse speed so that we can display it on-screen.
        // Perform a low pass filter on this so that the thermometer
        // appears to move smoothly.
        testcontrols_mousespeed = abs(ev->data2);
    }

    // If the next/previous weapon keys are pressed, set the next_weapon
    // variable to change weapons when the next ticcmd is generated.
    if (ev->type == ev_keydown && ev->data1 == key_prevweapon) {
        next_weapon = -1;
    } else if (ev->type == ev_keydown && ev->data1 == key_nextweapon) {
        next_weapon = 1;
    }

    switch (ev->type) {
        case ev_keydown:
            if (ev->data1 == key_pause) {
                sendpause = true;
            } else if (ev->data1 < NUMKEYS) {
                gamekeydown[ev->data1] = true;
            }
            // eat key down events
            return true;

        case ev_keyup:
            if (ev->data1 < NUMKEYS) {
                gamekeydown[ev->data1] = false;
            }
            // always let key up events filter down
            return false;

        case ev_mouse:
            SetMouseButtons(ev->data1);
            mousex = ev->data2 * (mouseSensitivity + 5) / 10;
            mousey = ev->data3 * (mouseSensitivity + 5) / 10;
            // eat events
            return true;

        case ev_joystick:
            SetJoyButtons(ev->data1);
            joyxmove = ev->data2;
            joyymove = ev->data3;
            joystrafemove = ev->data4;
            // eat events
            return true;

        default:
            break;
    }

    return false;
}

//
// do main actions
//
static void G_RunTickers() {
    switch (gamestate) {
        case GS_LEVEL:
            P_Ticker();
            ST_Ticker();
            AM_Ticker();
            HU_Ticker();
            break;
        case GS_INTERMISSION:
            WI_Ticker();
            break;
        case GS_FINALE:
            F_Ticker();
            break;
        case GS_DEMOSCREEN:
            D_PageTicker();
            break;
    }
}

static void G_PrepareSaveGame(byte buttons) {
    if (!savedescription[0]) {
        M_StringCopy(savedescription, "NET GAME", sizeof(savedescription));
    }
    savegameslot = (buttons & BTS_SAVEMASK) >> BTS_SAVESHIFT;
    gameaction = ga_savegame;
}

static void G_PauseGame() {
    paused ^= 1;
    if (paused) {
        S_PauseSound();
    } else {
        S_ResumeSound();
    }
}

static void G_UpdateOldGameState() {
    oldgamestate = gamestate;
}

//
// Have we just finished displaying an intermission screen?
//
static bool G_HasFinishedWI() {
    return oldgamestate == GS_INTERMISSION && gamestate != GS_INTERMISSION;
}

static void G_CheckSpecialButtons() {
    for (int i = 0; i < MAXPLAYERS; i++) {
        if (!playeringame[i]) {
            continue;
        }

        byte buttons = players[i].cmd.buttons;
        if ((buttons & BT_SPECIAL) == 0) {
            continue;
        }
        byte button_mask = buttons & BT_SPECIALMASK;
        if (button_mask == BTS_PAUSE) {
            G_PauseGame();
            continue;
        }
        if (button_mask == BTS_SAVEGAME) {
            G_PrepareSaveGame(buttons);
        }
    }
}

//
// Get commands, check consistency and build new consistency check.
//
static void G_UpdateNetConsistency() {
    int buf = (gametic / ticdup) % BACKUPTICS;

    for (int i = 0; i < MAXPLAYERS; i++) {
        if (!playeringame[i]) {
            continue;
        }
        ticcmd_t* cmd = &players[i].cmd;
        memcpy(cmd, &netcmds[i], sizeof(ticcmd_t));

        if (demoplayback) {
            G_ReadDemoTiccmd(cmd);
        }
        if (demorecording) {
            G_WriteDemoTiccmd(cmd);
        }

        // check for turbo cheats
        // check ~ 4 seconds whether to display the turbo message.
        // store if the turbo threshold was exceeded in any tics
        // over the past 4 seconds.  offset the checking period
        // for each player so messages are not displayed at the
        // same time.
        if (cmd->forwardmove > TURBOTHRESHOLD) {
            turbodetected[i] = true;
        }
        if ((gametic & 31) == 0 && ((gametic >> 5) % MAXPLAYERS) == i && turbodetected[i]) {
            static char turbomessage[80];
            M_snprintf(turbomessage, sizeof(turbomessage), "%s is turbo!", player_names[i]);
            players[consoleplayer].message = turbomessage;
            turbodetected[i] = false;
        }

        if (netgame && !netdemo && !(gametic % ticdup)) {
            if (gametic > BACKUPTICS && consistancy[i][buf] != cmd->consistancy) {
                I_Error("consistency failure (%i should be %i)", cmd->consistancy, consistancy[i][buf]);
            }
            if (players[i].mo) {
                consistancy[i][buf] = (byte) players[i].mo->x;
            } else {
                consistancy[i][buf] = (byte) rndindex;
            }
        }
    }
}

static void G_DoScreenShot() {
    V_ScreenShot("DOOM%02i.%s");
    players[consoleplayer].message = DEH_String("screen shot");
    gameaction = ga_nothing;
}

//
// Do things to change the game state.
//
static void G_RunGameActions() {
    while (gameaction != ga_nothing) {
        switch (gameaction) {
            case ga_loadlevel:
                G_DoLoadLevel();
                break;
            case ga_newgame:
                G_DoNewGame();
                break;
            case ga_loadgame:
                G_DoLoadGame();
                break;
            case ga_savegame:
                G_DoSaveGame();
                break;
            case ga_playdemo:
                G_DoPlayDemo();
                break;
            case ga_completed:
                G_DoCompleted();
                break;
            case ga_victory:
                F_StartFinale();
                break;
            case ga_worlddone:
                G_DoWorldDone();
                break;
            case ga_screenshot:
                G_DoScreenShot();
                break;
            case ga_nothing:
                break;
        }
    }
}

//
// do player reborns if needed
//
static void G_RebornPlayers() {
    for (int i = 0; i < MAXPLAYERS; i++) {
        if (playeringame[i] && players[i].playerstate == PST_REBORN) {
            G_DoReborn(i);
        }
    }
}
 
//
// G_Ticker
// Make ticcmd_ts for the players.
//
void G_Ticker() {
    G_RebornPlayers();
    G_RunGameActions();
    G_UpdateNetConsistency();
    G_CheckSpecialButtons();
    if (G_HasFinishedWI()) {
        WI_End();
    }
    G_UpdateOldGameState();
    G_RunTickers();
} 
 
 
//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//
 
 

//
// G_PlayerFinishLevel
// Can when a playernum completes a level.
//
void G_PlayerFinishLevel(int playernum) {
    player_t*player = &players[playernum];

    memset(player->powers, 0, sizeof (player->powers));
    memset(player->cards, 0, sizeof (player->cards));

    player->mo->flags &= ~MF_SHADOW;    // cancel invisibility
    player->extralight = 0;             // cancel gun flashes
    player->fixedcolormap = 0;          // cancel ir gogles
    player->damagecount = 0;            // no palette changes
    player->bonuscount = 0;
}

static void G_ResetWeapons(player_t* player) {
    player->pendingweapon = wp_pistol;
    player->readyweapon = wp_pistol;
    player->weaponowned[wp_fist] = true;
    player->weaponowned[wp_pistol] = true;
    player->ammo[am_clip] = deh_initial_bullets;

    for (int i = 0; i < NUMAMMO; i++) {
        player->maxammo[i] = maxammo[i];
    }
}

static void G_ResetPlayerState(player_t* player) {
    player->playerstate = PST_LIVE;
    player->health = deh_initial_health;     // Use dehacked value
}

static void G_ResetButtons(player_t* player) {
    // don't do anything immediately
    player->attackdown = true;
    player->usedown = true;
}

static void G_ClearPlayerData(player_t* player) {
    memset(player, 0, sizeof(*player));
}

// Variables used to persist multiplayer stats when player is reborn.
static int frags[MAXPLAYERS];
static int killcount = 0;
static int itemcount = 0;
static int secretcount = 0;

static void G_RestoreMultiplayerStats(player_t* player) {
    memcpy(player->frags, frags, sizeof(player->frags));
    player->killcount = killcount;
    player->itemcount = itemcount;
    player->secretcount = secretcount;
}

static void G_SaveMultiplayerStats(const player_t* player) {
    memcpy(frags, player->frags, sizeof(frags));
    killcount = player->killcount;
    itemcount = player->itemcount;
    secretcount = player->secretcount;
}

//
// G_PlayerReborn
// Called after a player dies
// except for multiplayer stats, everything is cleared and initialized
//
void G_PlayerReborn(int playernum) {
    player_t* player = &players[playernum];

    G_SaveMultiplayerStats(player);
    G_ClearPlayerData(player);
    G_RestoreMultiplayerStats(player);
    G_ResetButtons(player);
    G_ResetPlayerState(player);
    G_ResetWeapons(player);
}

//
// G_CheckSpot  
// Returns false if the player cannot be respawned
// at the given mapthing_t spot  
// because something is occupying it 
//
void P_SpawnPlayer(mapthing_t* mthing);

//
// The code in the released source looks like this:
//
//    an = ( ANG45 * (((unsigned int) mthing->angle)/45) )
//         >> ANGLETOFINESHIFT;
//    mo = P_SpawnMobj (x+20*finecosine[an], y+20*finesine[an]
//                     , ss->sector->floorheight
//                     , MT_TFOG);
//
// But 'an' can be a signed value in the DOS version. This means that
// we get a negative index and the lookups into finecosine/finesine
// end up dereferencing values in finetangent[].
// A player spawning on a deathmatch start facing directly west spawns
// "silently" with no spawn fog. Emulate this.
//
// This code is imported from PrBoom+.
//
static mobj_t* G_SpawnTeleportMobj(const mapthing_t* mthing) {
    // This calculation overflows in Vanilla Doom, but here we deliberately
    // avoid integer overflow as it is undefined behavior, so the value of
    // 'an' will always be positive.
    signed int an = (ANG45 >> ANGLETOFINESHIFT) * ((signed int) mthing->angle / 45);
    fixed_t xa;
    fixed_t ya;

    switch (an) {
        case 4096:            // -4096:
            xa = TAN(0);      // finecosine[-4096]
            ya = TAN(ANG270); // finesine[-4096]
            break;
        case 5120:                    // -3072:
            xa = TAN(ANG45);          // finecosine[-3072]
            ya = TAN(ANG270 + ANG45); // finesine[-3072]
            break;
        case 6144:       // -2048:
            xa = SIN(0); // finecosine[-2048]
            ya = TAN(0); // finesine[-2048]
            break;
        case 7168:           // -1024:
            xa = SIN(ANG45); // finecosine[-1024]
            ya = TAN(ANG45); // finesine[-1024]
            break;
        case 0:
        case 1024:
        case 2048:
        case 3072:
            xa = finecosine[an];
            ya = finesine[an];
            break;
        default:
            I_Error("G_CheckSpot: unexpected angle %d\n", an);
            xa = ya = 0;
            break;
    }

    fixed_t x = mthing->x << FRACBITS;
    fixed_t y = mthing->y << FRACBITS;
    const subsector_t* subsector = R_PointInSubsector(x, y);

    x = x + (20 * xa);
    y = y + (20 * ya);
    fixed_t z = subsector->sector->floorheight;

    return P_SpawnMobj(x, y, z, MT_TFOG);
}

static void G_SpawnTeleportFog(const mapthing_t* mthing) {
    mobj_t* mo = G_SpawnTeleportMobj(mthing);

    // don't start sound on first frame
    if (players[consoleplayer].viewz != 1) {
        S_StartSound(mo, sfx_telept);
    }
}

//
// Flush an old corpse if needed
//
static void G_FlushOldBody(player_t* player) {
    int bodynum = bodyqueslot % BODYQUESIZE;
    if (bodyqueslot >= BODYQUESIZE) {
        P_RemoveMobj(bodyque[bodynum]);
    }
    bodyque[bodynum] = player->mo;
    bodyqueslot++;
}

static bool G_OccupySamePosition(player_t* player, const mapthing_t* mthing) {
    fixed_t x = mthing->x << FRACBITS;
    fixed_t y = mthing->y << FRACBITS;
    return P_CheckPosition(player->mo, x, y);
}

static bool G_CheckSpotFirstSpawn(const player_t* player, const mapthing_t* mthing) {
    fixed_t x = mthing->x << FRACBITS;
    fixed_t y = mthing->y << FRACBITS;
    int playernum = player - players;
    for (int i = 0; i < playernum; i++) {
        if (players[i].mo->x == x && players[i].mo->y == y) {
            return false;
        }
    }
    return true;
}

//
// Check if first spawn of level, before corpses
//
static bool G_IsFirstSpawn(player_t* player) {
    return !player->mo;
}
 
static bool G_CheckSpot(int playernum, const mapthing_t* mthing) {
    player_t* player = &players[playernum];
    if (G_IsFirstSpawn(player)) {
        return G_CheckSpotFirstSpawn(player, mthing);
    }
    if (G_OccupySamePosition(player, mthing)) {
        G_FlushOldBody(player);
        G_SpawnTeleportFog(mthing);
        return true;
    }
    return false;
} 


//
// G_DeathMatchSpawnPlayer 
// Spawns a player at one of the random death match spots
// called at level load and each death
//
void G_DeathMatchSpawnPlayer(int playernum) {
    int selections = deathmatch_p - deathmatchstarts;
    if (selections < 4) {
        I_Error("Only %i deathmatch spots, 4 required", selections);
    }
 
    for (int j = 0 ; j < 20 ; j++) {
	int i = P_Random() % selections;
	if (G_CheckSpot(playernum, &deathmatchstarts[i])) {
	    deathmatchstarts[i].type = (short) (playernum + 1);
	    P_SpawnPlayer(&deathmatchstarts[i]);
	    return;
	}
    }

    // no good spot, so the player will probably get stuck 
    P_SpawnPlayer(&playerstarts[playernum]);
}

//
// G_DoReborn 
// 
void G_DoReborn(int playernum) {
    if (!netgame) {
	// reload the level from scratch
	gameaction = ga_loadlevel;
        return;
    }

    // respawn at the start

    // first dissasociate the corpse
    players[playernum].mo->player = NULL;

    // spawn at random spot if in death match
    if (deathmatch) {
        G_DeathMatchSpawnPlayer (playernum);
        return;
    }
    if (G_CheckSpot(playernum, &playerstarts[playernum])) {
        P_SpawnPlayer(&playerstarts[playernum]);
        return;
    }

    // try to spawn at one of the other players spots
    for (int i = 0; i < MAXPLAYERS; i++) {
        if (G_CheckSpot(playernum, &playerstarts[i]) ) {
            playerstarts[i].type = (short) (playernum + 1); // fake as other player
            P_SpawnPlayer(&playerstarts[i]);
            playerstarts[i].type = (short) (i + 1); // restore
            return;
        }
        // he's going to be inside something.  Too bad.
    }

    P_SpawnPlayer(&playerstarts[playernum]);
} 
 
 
void G_ScreenShot(void) {
    gameaction = ga_screenshot;
} 
 


// DOOM Par Times
static const int pars[4][10] = {
    {0},
    {0, 30, 75, 120, 90, 165, 180, 180, 30, 165},
    {0, 90, 90, 90, 120, 90, 360, 240, 30, 170},
    {0, 90, 45, 90, 150, 90, 90, 165, 30, 135}
};

// DOOM II Par Times
static const int cpars[32] = {
    30,  90,  120, 120, 90,  150, 120, 120, 270, 90,  //  1-10
    210, 150, 150, 150, 210, 150, 420, 150, 210, 150, // 11-20
    240, 150, 180, 150, 150, 300, 330, 420, 300, 180, // 21-30
    120, 30                                           // 31-32
};

// Chex Quest Par Times
static const int chexpars[6] = {0, 120, 360, 480, 200, 360};


//
// G_DoCompleted 
//
bool secretexit;

void G_ExitLevel() {
    secretexit = false; 
    gameaction = ga_completed; 
} 

// Here's for the german edition.
void G_SecretExitLevel() {
    // IF NO WOLF3D LEVELS, NO SECRET EXIT!
    if ((gamemode == commercial) && (W_CheckNumForName("map31") < 0)) {
        secretexit = false;
    } else {
        secretexit = true;
    }
    gameaction = ga_completed; 
}

static void G_UpdateWIPlayersStats() {
    for (int i = 0; i < MAXPLAYERS; i++) {
        wminfo.plyr[i].in = playeringame[i];
        wminfo.plyr[i].skills = players[i].killcount;
        wminfo.plyr[i].sitems = players[i].itemcount;
        wminfo.plyr[i].ssecret = players[i].secretcount;
        wminfo.plyr[i].stime = leveltime;
        memcpy(wminfo.plyr[i].frags, players[i].frags, sizeof(wminfo.plyr[i].frags));
    }
}

static void G_UpdateWIParTimeCommercial() {
    // map33 reads its par time from beyond the cpars[] array
    if (gamemap == 33) {
        int cpars32;
        memcpy(&cpars32, DEH_String(GAMMALVL0), sizeof(int));
        cpars32 = LONG(cpars32);
        wminfo.partime = TICRATE*cpars32;
        return;
    }
    wminfo.partime = TICRATE*cpars[gamemap-1];
}

static void G_UpdateWIParTime() {
    // Set par time. Exceptions are added for purposes of
    // statcheck regression testing.
    if (gamemode == commercial) {
        G_UpdateWIParTimeCommercial();
        return;
    }
    // Doom episode 4 doesn't have a par time, so this
    // overflows into the cpars array.
    if (gameepisode < 4) {
        if (gameversion == exe_chex && gameepisode == 1 && gamemap < 6) {
            wminfo.partime = TICRATE * chexpars[gamemap];
        } else {
            wminfo.partime = TICRATE * pars[gameepisode][gamemap];
        }
        return;
    }
    wminfo.partime = TICRATE * cpars[gamemap];
}

static void G_UpdateWINextMapCommercial() {
    if (secretexit) {
        if (gamemap == 15) {
            wminfo.next = 30;
            return;
        }
        if (gamemap == 31) {
            wminfo.next = 31;
        }
        return;
    }
    switch (gamemap) {
        case 31:
        case 32:
            wminfo.next = 15;
            break;
        default:
            wminfo.next = gamemap;
    }
}

static void G_UpdateWINextMap() {
    // wminfo.next is 0 biased, unlike gamemap
    if (gamemode == commercial) {
        G_UpdateWINextMapCommercial();
        return;
    }
    if (secretexit) {
        // go to secret level
        wminfo.next = 8;
        return;
    }
    if (gamemap == 9) {
        // returning from secret level
        switch (gameepisode) {
            case 1:
                wminfo.next = 3;
                break;
            case 2:
                wminfo.next = 5;
                break;
            case 3:
                wminfo.next = 6;
                break;
            case 4:
                wminfo.next = 2;
                break;
            default:
                break;
        }
        return;
    }
    // go to next level
    wminfo.next = gamemap;
}

static void G_UpdateWILevelStats() {
    wminfo.didsecret = players[consoleplayer].didsecret;
    wminfo.epsd = gameepisode - 1;
    wminfo.last = gamemap - 1;
    wminfo.maxkills = totalkills;
    wminfo.maxitems = totalitems;
    wminfo.maxsecret = totalsecret;
    wminfo.maxfrags = 0;
    wminfo.pnum = consoleplayer;
}

static void G_UpdateWIStats() {
    G_UpdateWILevelStats();
    G_UpdateWINextMap();
    G_UpdateWIParTime();
    G_UpdateWIPlayersStats();
}

static void G_DoIntermission() {
    gamestate = GS_INTERMISSION;
    viewactive = false;
    automapactive = false;
    G_UpdateWIStats();
    StatCopy(&wminfo);
    WI_Start(&wminfo);
}

static void G_SetSecretLevelDone() {
    // exit secret level
    for (int i = 0; i < MAXPLAYERS; i++) {
        players[i].didsecret = true;
    }
}

static bool G_HasFinishedSecretLevel() {
    return (gamemap == 9) && (gamemode != commercial);
}

static bool G_HasFinishedGame() {
    if (gamemode == commercial) {
        return false;
    }
    if (gameversion == exe_chex && gamemap == 5) {
        // Chex Quest ends after 5 levels, rather than 8.
        return true;
    }
    return gamemap == 8;
}

static void G_FinishLevel() {
    for (int i = 0; i < MAXPLAYERS; i++) {
        if (playeringame[i]) {
            // take away cards and stuff
            G_PlayerFinishLevel(i);
        }
    }
}
 
void G_DoCompleted(void) {
    gameaction = ga_nothing;
    G_FinishLevel();
    if (automapactive) {
        AM_Stop();
    }
    if (G_HasFinishedGame()) {
        gameaction = ga_victory;
        return;
    }
    if (G_HasFinishedSecretLevel()) {
        G_SetSecretLevelDone();
    }
    G_DoIntermission();
}


//
// G_WorldDone 
//
void G_WorldDone(void) {
    gameaction = ga_worlddone; 

    if (secretexit) {
        players[consoleplayer].didsecret = true;
    }

    if (gamemode == commercial) {
	switch (gamemap) {
	  case 15:
	  case 31:
	    if (!secretexit) {
                break;
            }
	  case 6:
	  case 11:
	  case 20:
	  case 30:
	    F_StartFinale();
	    break;
	}
    }
} 
 
void G_DoWorldDone(void) {
    gamestate = GS_LEVEL;
    gamemap = wminfo.next + 1;
    G_DoLoadLevel();
    gameaction = ga_nothing;
    viewactive = true;
}



//
// G_InitFromSavegame
// Can be called by the startup code or the menu task. 
//

char savename[256];

void G_LoadGame(char* name) {
    M_StringCopy(savename, name, sizeof(savename));
    gameaction = ga_loadgame;
} 

void G_DoLoadGame(void) {
    int savedleveltime;
	 
    gameaction = ga_nothing;
    save_stream = M_fopen(savename, "rb");

    if (save_stream == NULL) {
        I_Error("Could not load savegame %s", savename);
    }

    savegame_error = false;

    if (!P_ReadSaveGameHeader()) {
        fclose(save_stream);
        return;
    }

    savedleveltime = leveltime;

    // load a base level 
    G_InitNew(gameskill, gameepisode, gamemap);

    leveltime = savedleveltime;

    // dearchive all the modifications
    P_UnArchivePlayers();
    P_UnArchiveWorld();
    P_UnArchiveThinkers();
    P_UnArchiveSpecials();

    if (!P_ReadSaveGameEOF()) {
        I_Error("Bad savegame");
    }

    fclose(save_stream);

    if (setsizeneeded) {
        R_ExecuteSetViewSize ();
    }

    // draw the pattern into the back screen
    R_FillBackScreen();
} 
 

//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string 
//
void G_SaveGame(int slot, char* description) {
    savegameslot = slot;
    M_StringCopy(savedescription, description, sizeof(savedescription));
    sendsave = true;
}

void G_DoSaveGame() {
    char* savegame_file;
    char* temp_savegame_file;
    char* recovery_savegame_file;

    recovery_savegame_file = NULL;
    temp_savegame_file = P_TempSaveGameFile();
    savegame_file = P_SaveGameFile(savegameslot);

    // Open the savegame file for writing.  We write to a temporary file
    // and then rename it at the end if it was successfully written.
    // This prevents an existing savegame from being overwritten by
    // a corrupted one, or if a savegame buffer overrun occurs.
    save_stream = M_fopen(temp_savegame_file, "wb");

    if (save_stream == NULL) {
        // Failed to save the game, so we're going to have to abort. But
        // to be nice, save to somewhere else before we call I_Error().
        recovery_savegame_file = M_TempFile("recovery.dsg");
        save_stream = M_fopen(recovery_savegame_file, "wb");
        if (save_stream == NULL) {
            I_Error("Failed to open either '%s' or '%s' to write savegame.",
                    temp_savegame_file, recovery_savegame_file);
        }
    }

    savegame_error = false;

    P_WriteSaveGameHeader(savedescription);

    P_ArchivePlayers();
    P_ArchiveWorld();
    P_ArchiveThinkers();
    P_ArchiveSpecials();

    P_WriteSaveGameEOF();

    // Enforce the same savegame size limit as in Vanilla Doom,
    // except if the vanilla_savegame_limit setting is turned off.

    if (vanilla_savegame_limit && ftell(save_stream) > SAVEGAMESIZE) {
        I_Error("Savegame buffer overrun");
    }

    // Finish up, close the savegame file.
    fclose(save_stream);

    if (recovery_savegame_file != NULL) {
        // We failed to save to the normal location, but we wrote a
        // recovery file to the temp directory. Now we can bomb out
        // with an error.
        I_Error("Failed to open savegame file '%s' for writing.\n"
                "But your game has been saved to '%s' for recovery.",
                temp_savegame_file, recovery_savegame_file);
    }

    // Now rename the temporary savegame file to the actual savegame
    // file, overwriting the old savegame if there was one there.

    M_remove(savegame_file);
    M_rename(temp_savegame_file, savegame_file);

    gameaction = ga_nothing;
    M_StringCopy(savedescription, "", sizeof(savedescription));

    players[consoleplayer].message = DEH_String(GGSAVED);

    // draw the pattern into the back screen
    R_FillBackScreen();
}


//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set. 
//
skill_t d_skill;
int d_episode;
int d_map;

void G_DeferedInitNew(skill_t skill, int episode, int map) {
    d_skill = skill;
    d_episode = episode;
    d_map = map;
    gameaction = ga_newgame;
}

void G_DoNewGame() {
    demoplayback = false;
    netdemo = false;
    netgame = false;
    deathmatch = false;
    playeringame[1] = false;
    playeringame[2] = false;
    playeringame[3] = false;
    respawnparm = false;
    fastparm = false;
    nomonsters = false;
    consoleplayer = 0;
    G_InitNew(d_skill, d_episode, d_map);
    gameaction = ga_nothing;
} 


void G_InitNew(skill_t skill, int episode, int map) {
    const char *skytexturename;
    int             i;

    if (paused) {
	paused = false;
	S_ResumeSound();
    }

    if (skill > sk_nightmare) {
        skill = sk_nightmare;
    }

    if (gameversion >= exe_ultimate) {
        if (episode == 0) {
            episode = 4;
        }
    } else {
        if (episode < 1) {
            episode = 1;
        }
        if (episode > 3) {
            episode = 3;
        }
    }

    if (episode > 1 && gamemode == shareware) {
        episode = 1;
    }

    if (map < 1) {
        map = 1;
    }

    if ((map > 9) && ( gamemode != commercial)) {
        map = 9;
    }

    M_ClearRandom();

    if (skill == sk_nightmare || respawnparm ) {
        respawnmonsters = true;
    } else {
        respawnmonsters = false;
    }

    if (fastparm || (skill == sk_nightmare && gameskill != sk_nightmare)) {
	for (i = S_SARG_RUN1; i <= S_SARG_PAIN2; i++) {
            states[i].tics >>= 1;
        }
	mobjinfo[MT_BRUISERSHOT].speed = 20*FRACUNIT;
	mobjinfo[MT_HEADSHOT].speed = 20*FRACUNIT;
	mobjinfo[MT_TROOPSHOT].speed = 20*FRACUNIT;
    } else if (skill != sk_nightmare && gameskill == sk_nightmare) {
	for (i = S_SARG_RUN1; i<=S_SARG_PAIN2; i++) {
            states[i].tics <<= 1;
        }
	mobjinfo[MT_BRUISERSHOT].speed = 15*FRACUNIT;
	mobjinfo[MT_HEADSHOT].speed = 10*FRACUNIT;
	mobjinfo[MT_TROOPSHOT].speed = 10*FRACUNIT;
    }

    // force players to be initialized upon first level load
    for (i = 0 ; i < MAXPLAYERS ; i++) {
        players[i].playerstate = PST_REBORN;
    }

    usergame = true;                // will be set false if a demo
    paused = false;
    demoplayback = false;
    automapactive = false;
    viewactive = true;
    gameepisode = episode;
    gamemap = map;
    gameskill = skill;

    // Set the sky to use.
    //
    // Note: This IS broken, but it is how Vanilla Doom behaves.
    // See http://doomwiki.org/wiki/Sky_never_changes_in_Doom_II.
    //
    // Because we set the sky here at the start of a game, not at the
    // start of a level, the sky texture never changes unless we
    // restore from a saved game.  This was fixed before the Doom
    // source release, but this IS the way Vanilla DOS Doom behaves.

    if (gamemode == commercial) {
        skytexturename = DEH_String("SKY3");
        sky_tex = R_TextureNumForName(skytexturename);
        if (gamemap < 21)
        {
            skytexturename = DEH_String(gamemap < 12 ? "SKY1" : "SKY2");
            sky_tex = R_TextureNumForName(skytexturename);
        }
    } else {
        switch (gameepisode) {
          default:
          case 1:
            skytexturename = "SKY1";
            break;
          case 2:
            skytexturename = "SKY2";
            break;
          case 3:
            skytexturename = "SKY3";
            break;
          case 4:        // Special Edition sky
            skytexturename = "SKY4";
            break;
        }
        skytexturename = DEH_String(skytexturename);
        sky_tex = R_TextureNumForName(skytexturename);
    }

    G_DoLoadLevel();
}


//
// DEMO RECORDING 
//
#define DEMOMARKER 0x80


void G_ReadDemoTiccmd (ticcmd_t* cmd) 
{ 
    if (*demo_p == DEMOMARKER) {
	// end of demo data stream 
	G_CheckDemoStatus (); 
	return; 
    } 
    cmd->forwardmove = ((signed char)*demo_p++); 
    cmd->sidemove = ((signed char)*demo_p++); 

    // If this is a longtics demo, read back in higher resolution

    if (longtics) {
        cmd->angleturn = *demo_p++;
        cmd->angleturn |= (*demo_p++) << 8;
    } else {
        cmd->angleturn = ((unsigned char) *demo_p++)<<8; 
    }

    cmd->buttons = (unsigned char)*demo_p++; 
} 

// Increase the size of the demo buffer to allow unlimited demos

static void IncreaseDemoBuffer(void)
{
    int current_length;
    byte *new_demobuffer;
    byte *new_demop;
    int new_length;

    // Find the current size

    current_length = demoend - demobuffer;
    
    // Generate a new buffer twice the size
    new_length = current_length * 2;
    
    new_demobuffer = Z_Malloc(new_length, PU_STATIC, 0);
    new_demop = new_demobuffer + (demo_p - demobuffer);

    // Copy over the old data

    memcpy(new_demobuffer, demobuffer, current_length);

    // Free the old buffer and point the demo pointers at the new buffer.

    Z_Free(demobuffer);

    demobuffer = new_demobuffer;
    demo_p = new_demop;
    demoend = demobuffer + new_length;
}

void G_WriteDemoTiccmd (ticcmd_t* cmd) 
{ 
    byte *demo_start;

    if (gamekeydown[key_demo_quit])           // press q to end demo recording 
	G_CheckDemoStatus (); 

    demo_start = demo_p;

    *demo_p++ = cmd->forwardmove; 
    *demo_p++ = cmd->sidemove; 

    // If this is a longtics demo, record in higher resolution
 
    if (longtics)
    {
        *demo_p++ = (cmd->angleturn & 0xff);
        *demo_p++ = (cmd->angleturn >> 8) & 0xff;
    }
    else
    {
        *demo_p++ = cmd->angleturn >> 8; 
    }

    *demo_p++ = cmd->buttons; 

    // reset demo pointer back
    demo_p = demo_start;

    if (demo_p > demoend - 16)
    {
        if (vanilla_demo_limit)
        {
            // no more space 
            G_CheckDemoStatus (); 
            return; 
        }
        else
        {
            // Vanilla demo limit disabled: unlimited
            // demo lengths!

            IncreaseDemoBuffer();
        }
    } 
	
    G_ReadDemoTiccmd (cmd);         // make SURE it is exactly the same 
} 
 
 
 
//
// G_RecordDemo
//
void G_RecordDemo(const char* name) {
    usergame = false;
    size_t demoname_size = strlen(name) + 5;
    demoname = Z_Malloc((int) demoname_size, PU_STATIC, NULL);
    M_snprintf(demoname, demoname_size, "%s.lmp", name);

    int maxsize = 0x20000;
    //!
    // @arg <size>
    // @category demo
    // @vanilla
    //
    // Specify the demo buffer size (KiB)
    //
    int i = M_CheckParmWithArgs("-maxdemo", 1);
    if (i) {
        maxsize = atoi(myargv[i + 1]) * 1024;
    }

    demobuffer = Z_Malloc(maxsize, PU_STATIC, NULL);
    demoend = &demobuffer[maxsize];

    demorecording = true;
}

// Get the demo version code appropriate for the version set in gameversion.
int G_VanillaVersionCode() {
    switch (gameversion) {
        case exe_doom_1_666:
            return 106;
        case exe_doom_1_7:
            return 107;
        case exe_doom_1_8:
            return 108;
        default:
            // All other versions are variants on v1.9.
            return 109;
    }
}

void G_BeginRecording() {
    demo_p = demobuffer;

    //!
    // @category demo
    //
    // Record a high resolution "Doom 1.91" demo.
    //
    longtics =
        D_NonVanillaRecord(M_ParmExists("-longtics"), "Doom 1.91 demo format");

    // If not recording a longtics demo, record in low res
    lowres_turn = !longtics;

    if (longtics) {
        *demo_p++ = DOOM_191_VERSION;
    } else if (gameversion > exe_doom_1_2) {
        *demo_p++ = G_VanillaVersionCode();
    }

    *demo_p++ = gameskill;
    *demo_p++ = gameepisode;
    *demo_p++ = gamemap;
    if (longtics || gameversion > exe_doom_1_2) {
        *demo_p++ = deathmatch;
        *demo_p++ = respawnparm;
        *demo_p++ = fastparm;
        *demo_p++ = nomonsters;
        *demo_p++ = consoleplayer;
    }

    for (int i = 0; i < MAXPLAYERS; i++) {
        *demo_p++ = playeringame[i];
    }
}


//
// G_PlayDemo 
//

static const char *defdemoname;
 
void G_DeferedPlayDemo(const char *name)
{ 
    defdemoname = name; 
    gameaction = ga_playdemo; 
} 

// Generate a string describing a demo version

static const char *DemoVersionDescription(int version)
{
    static char resultbuf[16];

    switch (version)
    {
        case 104:
            return "v1.4";
        case 105:
            return "v1.5";
        case 106:
            return "v1.6/v1.666";
        case 107:
            return "v1.7/v1.7a";
        case 108:
            return "v1.8";
        case 109:
            return "v1.9";
        case 111:
            return "v1.91 hack demo?";
        default:
            break;
    }

    // Unknown version.  Perhaps this is a pre-v1.4 IWAD?  If the version
    // byte is in the range 0-4 then it can be a v1.0-v1.2 demo.

    if (version >= 0 && version <= 4)
    {
        return "v1.0/v1.1/v1.2";
    }
    else
    {
        M_snprintf(resultbuf, sizeof(resultbuf),
                   "%i.%i (unknown)", version / 100, version % 100);
        return resultbuf;
    }
}

void G_DoPlayDemo (void)
{
    skill_t skill;
    int i, lumpnum, episode, map;
    int demoversion;
    bool olddemo = false;

    lumpnum = W_GetNumForName(defdemoname);
    gameaction = ga_nothing;
    demobuffer = W_CacheLumpNum(lumpnum, PU_STATIC);
    demo_p = demobuffer;

    demoversion = *demo_p++;

    if (demoversion >= 0 && demoversion <= 4)
    {
        olddemo = true;
        demo_p--;
    }

    longtics = false;

    // Longtics demos use the modified format that is generated by cph's
    // hacked "v1.91" doom exe. This is a non-vanilla extension.
    if (D_NonVanillaPlayback(demoversion == DOOM_191_VERSION, lumpnum,
                             "Doom 1.91 demo format"))
    {
        longtics = true;
    }
    else if (demoversion != G_VanillaVersionCode() &&
             !(gameversion <= exe_doom_1_2 && olddemo))
    {
        const char *message = "Demo is from a different game version!\n"
                              "(read %i, should be %i)\n"
                              "\n"
                              "*** You may need to upgrade your version "
                                  "of Doom to v1.9. ***\n"
                              "    See: https://www.doomworld.com/classicdoom"
                                        "/info/patches.php\n"
                              "    This appears to be %s.";

        I_Error(message, demoversion, G_VanillaVersionCode(),
                         DemoVersionDescription(demoversion));
    }

    skill = *demo_p++; 
    episode = *demo_p++; 
    map = *demo_p++; 
    if (!olddemo)
    {
        deathmatch = *demo_p++;
        respawnparm = *demo_p++;
        fastparm = *demo_p++;
        nomonsters = *demo_p++;
        consoleplayer = *demo_p++;
    }
    else
    {
        deathmatch = 0;
        respawnparm = 0;
        fastparm = 0;
        nomonsters = 0;
        consoleplayer = 0;
    }
    
        
    for (i=0 ; i<MAXPLAYERS ; i++) 
	playeringame[i] = *demo_p++; 

    if (playeringame[1] || M_CheckParm("-solo-net") > 0
                        || M_CheckParm("-netdemo") > 0)
    {
	netgame = true;
	netdemo = true;
    }

    // don't spend a lot of time in loadlevel 
    precache = false;
    G_InitNew (skill, episode, map); 
    precache = true; 
    starttime = I_GetTime (); 

    usergame = false; 
    demoplayback = true; 
} 

//
// G_TimeDemo 
//
void G_TimeDemo (char* name) 
{
    //!
    // @category video
    // @vanilla
    //
    // Disable rendering the screen entirely.
    //

    nodrawers = M_CheckParm ("-nodraw");

    timingdemo = true; 
    singletics = true; 

    defdemoname = name; 
    gameaction = ga_playdemo; 
} 
 
 
/* 
=================== 
= 
= G_CheckDemoStatus 
= 
= Called after a death or level completion to allow demos to be cleaned up 
= Returns true if a new demo loop action will take place 
=================== 
*/ 
 
bool G_CheckDemoStatus (void)
{ 
    int             endtime; 
	 
    if (timingdemo) 
    { 
        float fps;
        int realtics;

	endtime = I_GetTime (); 
        realtics = endtime - starttime;
        fps = ((float) gametic * TICRATE) / realtics;

        // Prevent recursive calls
        timingdemo = false;
        demoplayback = false;

	I_Error ("timed %i gametics in %i realtics (%f fps)",
                 gametic, realtics, fps);
    } 
	 
    if (demoplayback) 
    { 
        W_ReleaseLumpName(defdemoname);
	demoplayback = false; 
	netdemo = false;
	netgame = false;
	deathmatch = false;
	playeringame[1] = playeringame[2] = playeringame[3] = 0;
	respawnparm = false;
	fastparm = false;
	nomonsters = false;
	consoleplayer = 0;
        
        if (singledemo) 
            I_Quit (); 
        else 
            D_AdvanceDemo (); 

	return true; 
    } 
 
    if (demorecording) 
    { 
	*demo_p++ = DEMOMARKER; 
	M_WriteFile (demoname, demobuffer, demo_p - demobuffer); 
	Z_Free (demobuffer); 
	demorecording = false; 
	I_Error ("Demo %s recorded",demoname); 
    } 
	 
    return false; 
} 
 
 
 

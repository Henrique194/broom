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
// DESCRIPTION:
//	Status bar code.
//	Does the face/direction indicator animatin.
//	Does palette indicators as well (red pain/berserk, bright pickup)
//


#include "d_items.h"
#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "m_random.h"
#include "w_wad.h"

#include "deh_str.h"
#include "doomdef.h"

#include "g_game.h"
#include "d_mode.h"

#include "st_stuff.h"
#include "st_lib.h"
#include "r_local.h"

#include "p_inter.h"

// Needs access to LFB.
#include "v_video.h"

// State.
#include "doomstat.h"


//
// STATUS BAR DATA
//


// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS   1
#define STARTBONUSPALS 9
#define NUMREDPALS     8
#define NUMBONUSPALS   4
// Radiation suit, green shift.
#define RADIATIONPAL 13

// Location of status bar
#define ST_X 0

#define ST_FX 143

// Number of status faces.
#define ST_NUMPAINFACES     5
#define ST_NUMSTRAIGHTFACES 3
#define ST_NUMTURNFACES     2
#define ST_NUMSPECIALFACES  3

#define ST_FACESTRIDE                                                          \
    (ST_NUMSTRAIGHTFACES + ST_NUMTURNFACES + ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES 2

#define ST_NUMFACES (ST_FACESTRIDE * ST_NUMPAINFACES + ST_NUMEXTRAFACES)

#define ST_TURNOFFSET     (ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET     (ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET (ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET  (ST_EVILGRINOFFSET + 1)
#define ST_GODFACE        (ST_NUMPAINFACES * ST_FACESTRIDE)
#define ST_DEADFACE       (ST_GODFACE + 1)

#define ST_FACESX 143
#define ST_FACESY 168

#define ST_EVILGRINCOUNT     (2 * TICRATE)
#define ST_STRAIGHTFACECOUNT (TICRATE / 2)
#define ST_TURNCOUNT         (1 * TICRATE)
#define ST_RAMPAGEDELAY      (2 * TICRATE)

#define ST_MUCHPAIN 20


// Location and size of statistics, justified according to widget type.
// Problem is, within which space? STbar? Screen?
// Note: This could be read in by a lump. Problem is, is the stuff
//       rendered into a buffer, or into the frame buffer?

// AMMO number pos.
#define ST_AMMOWIDTH 3
#define ST_AMMOX     44
#define ST_AMMOY     171

// HEALTH number pos.
#define ST_HEALTHX 90
#define ST_HEALTHY 171

// Weapon pos.
#define ST_ARMSX      111
#define ST_ARMSY      172
#define ST_ARMSBGX    104
#define ST_ARMSBGY    168
#define ST_ARMSXSPACE 12
#define ST_ARMSYSPACE 10

// Frags pos.
#define ST_FRAGSX     138
#define ST_FRAGSY     171
#define ST_FRAGSWIDTH 2

// ARMOR number pos.
#define ST_ARMORX 221
#define ST_ARMORY 171

// Key icon positions.
#define ST_KEY0X 239
#define ST_KEY0Y 171
#define ST_KEY1X 239
#define ST_KEY1Y 181
#define ST_KEY2X 239
#define ST_KEY2Y 191

// Ammunition counter.
#define ST_AMMO0WIDTH 3
#define ST_AMMO0X     288
#define ST_AMMO0Y     173
#define ST_AMMO1WIDTH ST_AMMO0WIDTH
#define ST_AMMO1X     288
#define ST_AMMO1Y     179
#define ST_AMMO2WIDTH ST_AMMO0WIDTH
#define ST_AMMO2X     288
#define ST_AMMO2Y     191
#define ST_AMMO3WIDTH ST_AMMO0WIDTH
#define ST_AMMO3X     288
#define ST_AMMO3Y     185

// Indicate maximum ammunition.
// Only needed because backpack exists.
#define ST_MAXAMMO0WIDTH 3
#define ST_MAXAMMO0X     314
#define ST_MAXAMMO0Y     173
#define ST_MAXAMMO1WIDTH ST_MAXAMMO0WIDTH
#define ST_MAXAMMO1X     314
#define ST_MAXAMMO1Y     179
#define ST_MAXAMMO2WIDTH ST_MAXAMMO0WIDTH
#define ST_MAXAMMO2X     314
#define ST_MAXAMMO2Y     191
#define ST_MAXAMMO3WIDTH ST_MAXAMMO0WIDTH
#define ST_MAXAMMO3X     314
#define ST_MAXAMMO3Y     185

// Dimensions given in characters.
#define ST_MSGWIDTH 52

// graphics are drawn to a backing screen and blitted to the real screen
pixel_t* st_backing_screen;

// main player in game
player_t* st_plyr;

// ST_Start() has just been called
bool st_firsttime;

// lump number for PLAYPAL
static int lu_palette;

// used for timing
static unsigned int st_clock;

// used for making messages go away
static int st_msgcounter = 0;

// whether left-side main status bar is active
static bool st_statusbaron;

// !deathmatch
static bool st_notdeathmatch;

// !deathmatch && st_statusbaron
static bool st_armson;

// !deathmatch
static bool st_fragson;

// main bar left
static patch_t* sbar;

// main bar right, for doom 1.0
static patch_t* sbarr;

// 0-9, tall numbers
static patch_t* tallnum[10];

// tall % sign
static patch_t* tallpercent;

// 0-9, short, yellow (,different!) numbers
static patch_t* shortnum[10];

// 3 key-cards, 3 skulls
static patch_t* keys[NUMCARDS];

// face status patches
static patch_t* faces[ST_NUMFACES];

// face background
static patch_t* faceback;

// main bar right
static patch_t* armsbg;

// weapon ownership patches
static patch_t* arms[6][2];

// ready-weapon widget
static st_number_t w_ready;

// in deathmatch only, summary of frags stats
static st_number_t w_frags;

// health widget
static st_percent_t w_health;

// arms background
static st_binicon_t w_armsbg;


// weapon ownership widgets
static st_multicon_t w_arms[6];

// face status widget
static st_multicon_t w_faces;

// keycard widgets
static st_multicon_t w_keyboxes[3];

// armor widget
static st_percent_t w_armor;

// ammo widgets
static st_number_t w_ammo[4];

// max ammo widgets
static st_number_t w_maxammo[4];


// number of frags so far in deathmatch
static int st_fragscount;

// used to use appopriately pained face
static int st_oldhealth = -1;

// used for evil grin
static bool oldweaponsowned[NUMWEAPONS];

// count until face changes
static int st_facecount = 0;

// current face index, used by w_faces
static int st_faceindex = 0;

// holds key-type for each key box on bar
static int keyboxes[3];

// a random number per tick
static int st_randomnumber;

cheatseq_t cheat_mus = CHEAT("idmus", 2);
cheatseq_t cheat_god = CHEAT("iddqd", 0);
cheatseq_t cheat_ammo = CHEAT("idkfa", 0);
cheatseq_t cheat_ammonokey = CHEAT("idfa", 0);
cheatseq_t cheat_noclip = CHEAT("idspispopd", 0);
cheatseq_t cheat_commercial_noclip = CHEAT("idclip", 0);

cheatseq_t cheat_powerup[7] = {
    CHEAT("idbeholdv", 0), CHEAT("idbeholds", 0), CHEAT("idbeholdi", 0),
    CHEAT("idbeholdr", 0), CHEAT("idbeholda", 0), CHEAT("idbeholdl", 0),
    CHEAT("idbehold", 0),
};

cheatseq_t cheat_choppers = CHEAT("idchoppers", 0);
cheatseq_t cheat_clev = CHEAT("idclev", 2);
cheatseq_t cheat_mypos = CHEAT("idmypos", 0);


//
// STATUS BAR CODE
//
void ST_Stop(void);

void ST_refreshBackground(void)
{

    if (st_statusbaron)
    {
        V_UseBuffer(st_backing_screen);

	V_DrawPatch(ST_X, 0, sbar);

	// draw right side of bar if needed (Doom 1.0)
	if (sbarr)
	    V_DrawPatch(ST_ARMSBGX, 0, sbarr);

	if (netgame)
	    V_DrawPatch(ST_FX, 0, faceback);

        V_RestoreBuffer();

	V_CopyRect(ST_X, 0, st_backing_screen, ST_WIDTH, ST_HEIGHT, ST_X, ST_Y);
    }

}

//
// w_frags
//
static void ST_UpdateFragsWidget() {
    st_fragson = deathmatch && st_statusbaron;
    st_fragscount = 0;

    for (int i = 0; i < MAXPLAYERS; i++) {
        if (i != consoleplayer) {
            st_fragscount += st_plyr->frags[i];
        } else {
            st_fragscount -= st_plyr->frags[i];
        }
    }
}

//
// w_arms[]
//
static void ST_UpdateArmsWidgets() {
    st_armson = st_statusbaron && !deathmatch;
}

//
// w_armsbg
//
static void ST_UpdateArmsBackgroundWidget() {
    st_notdeathmatch = !deathmatch;
}

static int ST_CalcPainOffset(void)
{
    int		health;
    static int	lastcalc;
    static int	oldhealth = -1;
    
    health = st_plyr->health > 100 ? 100 : st_plyr->health;

    if (health != oldhealth)
    {
	lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
	oldhealth = health;
    }
    return lastcalc;
}

static int ST_CalcAttackedOffset() {
    fixed_t plr_x = st_plyr->mo->x;
    fixed_t plr_y = st_plyr->mo->y;
    fixed_t enemy_x = st_plyr->attacker->x;
    fixed_t enemy_y = st_plyr->attacker->y;

    angle_t enemy_angle = R_PointToAngle2(plr_x, plr_y, enemy_x, enemy_y);
    angle_t angle_distance;
    bool turn_right;

    if (enemy_angle > st_plyr->mo->angle) {
        angle_distance = enemy_angle - st_plyr->mo->angle;
        turn_right = angle_distance > ANG180;
    } else {
        angle_distance = st_plyr->mo->angle - enemy_angle;
        turn_right = angle_distance <= ANG180;
    }

    if (angle_distance < ANG45) {
        return ST_RAMPAGEOFFSET;
    }
    if (turn_right) {
        return ST_TURNOFFSET;
    }
    return ST_TURNOFFSET + 1;
}

static bool ST_IsPlayerDead() {
    return st_plyr->health == 0;
}

static bool ST_PickedUpNewWeapon() {
    if (!st_plyr->bonuscount) {
        // did not pick up any item
        return false;
    }

    bool new_weapon = false;
    for (int i = 0; i < NUMWEAPONS; i++) {
        if (oldweaponsowned[i] != st_plyr->weaponowned[i]) {
            new_weapon = true;
            oldweaponsowned[i] = st_plyr->weaponowned[i];
        }
    }
    return new_weapon;
}

static bool ST_ReceivedDamage() {
    return st_plyr->damagecount != 0;
}

static bool ST_AttackedByEnemy() {
    if (ST_ReceivedDamage()) {
        return st_plyr->attacker && st_plyr->attacker != st_plyr->mo;
    }
    return false;
}

static bool ST_ReceivedGreatDamage() {
    if (ST_ReceivedDamage()) {
        return st_plyr->health - st_oldhealth > ST_MUCHPAIN;
    }
    return false;
}

static bool ST_OnRampage() {
    static int lastattackdown = -1;

    if (st_plyr->attackdown == 0) {
        lastattackdown = -1;
        return false;
    }
    if (lastattackdown == -1) {
        lastattackdown = ST_RAMPAGEDELAY;
        return false;
    }
    if (!--lastattackdown) {
        lastattackdown = 1;
        return true;
    }
    return false;
}

static bool ST_IsInvulnerable() {
    if (st_plyr->cheats & CF_GODMODE) {
        return true;
    }
    return st_plyr->powers[pw_invulnerability] != 0;
}

//
// This is a not-very-pretty routine which handles the face states and their timing.
// the precedence of expressions is:
//   dead > evil grin > turned head > straight ahead
//
static void ST_UpdateFaceWidget(void) {
    static int priority = 0;

    if (priority < 10 && ST_IsPlayerDead()) {
        priority = 9;
        st_faceindex = ST_DEADFACE;
        st_facecount = 1;
    }
    if (priority < 9 && ST_PickedUpNewWeapon()) {
        // evil grin if just picked up weapon
        priority = 8;
        st_facecount = ST_EVILGRINCOUNT;
        st_faceindex = ST_CalcPainOffset() + ST_EVILGRINOFFSET;
    }
    if (priority < 8 && ST_AttackedByEnemy()) {
        priority = 7;
        st_facecount = ST_TURNCOUNT;
        if (ST_ReceivedGreatDamage()) {
            st_faceindex = ST_CalcPainOffset() + ST_OUCHOFFSET;
        } else {
            st_faceindex = ST_CalcPainOffset() + ST_CalcAttackedOffset();
        }
    }
    if (priority < 7 && ST_ReceivedDamage()) {
        // getting hurt because of your own damn stupidity
        st_facecount = ST_TURNCOUNT;
        if (ST_ReceivedGreatDamage()) {
            priority = 7;
            st_faceindex = ST_CalcPainOffset() + ST_OUCHOFFSET;
        } else {
            priority = 6;
            st_faceindex = ST_CalcPainOffset() + ST_RAMPAGEOFFSET;
        }
    }
    if (priority < 6 && ST_OnRampage()) {
        priority = 5;
        st_faceindex = ST_CalcPainOffset() + ST_RAMPAGEOFFSET;
        st_facecount = 1;
    }
    if (priority < 5 && ST_IsInvulnerable()) {
        priority = 4;
        st_faceindex = ST_GODFACE;
        st_facecount = 1;
    }
    if (st_facecount == 0) {
        // look left or look right if the facecount has timed out
        priority = 0;
        st_faceindex = ST_CalcPainOffset() + (st_randomnumber % 3);
        st_facecount = ST_STRAIGHTFACECOUNT;
    }

    st_facecount--;
}

//
// Update keycard multiple widgets.
//
static void ST_UpdateKeyCardsWidgets() {
    for (int i = 0; i < 3; i++) {
        keyboxes[i] = st_plyr->cards[i] ? i : -1;
        if (st_plyr->cards[i + 3]) {
            keyboxes[i] = i + 3;
        }
    }
}

static void ST_UpdateAmmoWidget() {
    static int largeammo = 1994; // means "n/a"

    // must redirect the pointer if the ready weapon has changed.
    if (weaponinfo[st_plyr->readyweapon].ammo == am_noammo) {
        w_ready.num = &largeammo;
    } else {
        w_ready.num = &st_plyr->ammo[weaponinfo[st_plyr->readyweapon].ammo];
    }
    w_ready.data = st_plyr->readyweapon;
}

static void ST_UpdateWidgets(void) {
    ST_UpdateAmmoWidget();
    ST_UpdateKeyCardsWidgets();
    // refresh everything if this is him coming back to life
    ST_UpdateFaceWidget();
    ST_UpdateArmsBackgroundWidget();
    ST_UpdateArmsWidgets();
    ST_UpdateFragsWidget();

    // get rid of chat window if up because of message
    st_msgcounter--;
}

void ST_Ticker(void) {
    st_clock++;
    st_randomnumber = M_Random();
    ST_UpdateWidgets();
    st_oldhealth = st_plyr->health;
}

static int st_palette = 0;

static void ST_SetPalette(int palette) {
    st_palette = palette;

    const byte* pal =
        (byte *) W_CacheLumpNum(lu_palette, PU_CACHE) + (palette * 768);
    I_SetPalette(pal);
}

static bool ST_IsUsingRadSuit() {
    return st_plyr->powers[pw_ironfeet] > 4 * 32
           || st_plyr->powers[pw_ironfeet] & 8;
}

static int ST_CalculateBonusPalette() {
    // Vanilla Bug: unused palette STARTBONUSPALS
    int palette = (st_plyr->bonuscount + 7) >> 3;
    if (palette >= NUMBONUSPALS) {
        palette = NUMBONUSPALS - 1;
    }
    return palette + STARTBONUSPALS;
}

static int ST_CalculateDamagePalette(int cnt) {
    // Vanilla Bug: unused palette STARTREDPALS
    int palette = (cnt + 7) >> 3;
    if (palette >= NUMREDPALS) {
        palette = NUMREDPALS - 1;
    }
    return palette + STARTREDPALS;
}

static int ST_GetEffectiveDamageCount() {
    int cnt = st_plyr->damagecount;

    if (st_plyr->powers[pw_strength]) {
        // slowly fade the berzerk out
        int bzc = 12 - (st_plyr->powers[pw_strength] >> 6);
        if (bzc > cnt) {
            cnt = bzc;
        }
    }

    return cnt;
}

static int ST_CalculatePalette() {
    int cnt = ST_GetEffectiveDamageCount();
    if (cnt) {
        return ST_CalculateDamagePalette(cnt);
    }
    if (st_plyr->bonuscount) {
        return ST_CalculateBonusPalette();
    }
    if (ST_IsUsingRadSuit()) {
        return RADIATIONPAL;
    }
    return 0;
}

static int ST_MapPalette(int palette) {
    if (gameversion != exe_chex) {
        return palette;
    }
    // In Chex Quest, the player never sees red. Instead, the radiation suit
    // palette is used to tint the screen green, as though the player is being
    // covered in goo by an attacking flemoid.
    if (palette < STARTREDPALS || palette >= STARTREDPALS + NUMREDPALS) {
        // Not red palette.
        return palette;
    }
    return RADIATIONPAL;
}

static int ST_SelectNewPalette() {
    int palette = ST_CalculatePalette();
    return ST_MapPalette(palette);
}

void ST_doPaletteStuff(void) {
    int palette = ST_SelectNewPalette();
    if (palette != st_palette) {
        ST_SetPalette(palette);
    }
}

void ST_drawWidgets(bool refresh)
{
    int		i;

    // used by w_arms[] widgets
    st_armson = st_statusbaron && !deathmatch;

    // used by w_frags widget
    st_fragson = deathmatch && st_statusbaron; 

    STlib_updateNum(&w_ready, refresh);

    for (i=0;i<4;i++)
    {
	STlib_updateNum(&w_ammo[i], refresh);
	STlib_updateNum(&w_maxammo[i], refresh);
    }

    STlib_updatePercent(&w_health, refresh);
    STlib_updatePercent(&w_armor, refresh);

    STlib_updateBinIcon(&w_armsbg, refresh);

    for (i=0;i<6;i++)
	STlib_updateMultIcon(&w_arms[i], refresh);

    STlib_updateMultIcon(&w_faces, refresh);

    for (i=0;i<3;i++)
	STlib_updateMultIcon(&w_keyboxes[i], refresh);

    STlib_updateNum(&w_frags, refresh);

}

void ST_doRefresh(void) {
    st_firsttime = false;
    // draw status bar background to off-screen buff
    ST_refreshBackground();
    // and refresh all widgets
    ST_drawWidgets(true);
}

void ST_diffDraw(void) {
    // update all widgets
    ST_drawWidgets(false);
}

void ST_Drawer(bool fullscreen, bool refresh) {
    st_statusbaron = (!fullscreen) || automapactive;
    st_firsttime = st_firsttime || refresh;

    // Do red-/gold-shifts from damage/items
    ST_doPaletteStuff();

    //    // If just after ST_Start(), refresh all
    //    if (st_firsttime) ST_doRefresh();
    //    // Otherwise, update as little as possible
    //    else ST_diffDraw();

    ST_doRefresh();
}

typedef void (*load_callback_t)(const char *lumpname, patch_t **variable);

// Iterates through all graphics to be loaded or unloaded, along with
// the variable they use, invoking the specified callback function.

static void ST_loadUnloadGraphics(load_callback_t callback)
{

    int		i;
    int		j;
    int		facenum;
    
    char	namebuf[9];

    // Load the numbers, tall and short
    for (i=0;i<10;i++)
    {
	DEH_snprintf(namebuf, 9, "STTNUM%d", i);
        callback(namebuf, &tallnum[i]);

	DEH_snprintf(namebuf, 9, "STYSNUM%d", i);
        callback(namebuf, &shortnum[i]);
    }

    // Load percent key.
    //Note: why not load STMINUS here, too?

    callback(DEH_String("STTPRCNT"), &tallpercent);

    // key cards
    for (i=0;i<NUMCARDS;i++)
    {
	DEH_snprintf(namebuf, 9, "STKEYS%d", i);
        callback(namebuf, &keys[i]);
    }

    // arms background
    callback(DEH_String("STARMS"), &armsbg);

    // arms ownership widgets
    for (i=0; i<6; i++)
    {
	DEH_snprintf(namebuf, 9, "STGNUM%d", i+2);

	// gray #
        callback(namebuf, &arms[i][0]);

	// yellow #
	arms[i][1] = shortnum[i+2]; 
    }

    // face backgrounds for different color players
    DEH_snprintf(namebuf, 9, "STFB%d", consoleplayer);
    callback(namebuf, &faceback);

    // status bar background bits
    if (W_CheckNumForName("STBAR") >= 0)
    {
        callback(DEH_String("STBAR"), &sbar);
        sbarr = NULL;
    }
    else
    {
        callback(DEH_String("STMBARL"), &sbar);
        callback(DEH_String("STMBARR"), &sbarr);
    }

    // face states
    facenum = 0;
    for (i=0; i<ST_NUMPAINFACES; i++)
    {
	for (j=0; j<ST_NUMSTRAIGHTFACES; j++)
	{
	    DEH_snprintf(namebuf, 9, "STFST%d%d", i, j);
            callback(namebuf, &faces[facenum]);
            ++facenum;
	}
	DEH_snprintf(namebuf, 9, "STFTR%d0", i);	// turn right
        callback(namebuf, &faces[facenum]);
        ++facenum;
	DEH_snprintf(namebuf, 9, "STFTL%d0", i);	// turn left
        callback(namebuf, &faces[facenum]);
        ++facenum;
	DEH_snprintf(namebuf, 9, "STFOUCH%d", i);	// ouch!
        callback(namebuf, &faces[facenum]);
        ++facenum;
	DEH_snprintf(namebuf, 9, "STFEVL%d", i);	// evil grin ;)
        callback(namebuf, &faces[facenum]);
        ++facenum;
	DEH_snprintf(namebuf, 9, "STFKILL%d", i);	// pissed off
        callback(namebuf, &faces[facenum]);
        ++facenum;
    }

    callback(DEH_String("STFGOD0"), &faces[facenum]);
    ++facenum;
    callback(DEH_String("STFDEAD0"), &faces[facenum]);
    ++facenum;
}

static void ST_loadCallback(const char *lumpname, patch_t **variable)
{
    *variable = W_CacheLumpName(lumpname, PU_STATIC);
}

void ST_loadGraphics(void)
{
    ST_loadUnloadGraphics(ST_loadCallback);
}

void ST_loadData(void)
{
    lu_palette = W_GetNumForName (DEH_String("PLAYPAL"));
    ST_loadGraphics();
}

void ST_initData(void)
{

    int		i;

    st_firsttime = true;
    st_plyr = &players[consoleplayer];

    st_clock = 0;

    st_statusbaron = true;

    st_faceindex = 0;
    st_palette = -1;

    st_oldhealth = -1;

    for (i=0;i<NUMWEAPONS;i++)
	oldweaponsowned[i] = st_plyr->weaponowned[i];

    for (i=0;i<3;i++)
	keyboxes[i] = -1;

    STlib_init();

}



void ST_createWidgets(void)
{

    int i;

    // ready weapon ammo
    STlib_initNum(&w_ready,
		  ST_AMMOX,
		  ST_AMMOY,
		  tallnum,
		  &st_plyr->ammo[weaponinfo[st_plyr->readyweapon].ammo],
		  &st_statusbaron,
		  ST_AMMOWIDTH );

    // the last weapon type
    w_ready.data = st_plyr->readyweapon;

    // health percentage
    STlib_initPercent(&w_health,
		      ST_HEALTHX,
		      ST_HEALTHY,
		      tallnum,
		      &st_plyr->health,
		      &st_statusbaron,
		      tallpercent);

    // arms background
    STlib_initBinIcon(&w_armsbg,
		      ST_ARMSBGX,
		      ST_ARMSBGY,
		      armsbg,
		      &st_notdeathmatch,
		      &st_statusbaron);

    // weapons owned
    for(i=0;i<6;i++)
    {
        STlib_initMultIcon(&w_arms[i],
                           ST_ARMSX+(i%3)*ST_ARMSXSPACE,
                           ST_ARMSY+(i/3)*ST_ARMSYSPACE,
                           arms[i],
                           &st_plyr->weaponowned[i+1],
                           &st_armson);
    }

    // frags sum
    STlib_initNum(&w_frags,
		  ST_FRAGSX,
		  ST_FRAGSY,
		  tallnum,
		  &st_fragscount,
		  &st_fragson,
		  ST_FRAGSWIDTH);

    // faces
    STlib_initMultIcon(&w_faces,
		       ST_FACESX,
		       ST_FACESY,
		       faces,
		       &st_faceindex,
		       &st_statusbaron);

    // armor percentage - should be colored later
    STlib_initPercent(&w_armor,
		      ST_ARMORX,
		      ST_ARMORY,
		      tallnum,
		      &st_plyr->armorpoints,
		      &st_statusbaron, tallpercent);

    // keyboxes 0-2
    STlib_initMultIcon(&w_keyboxes[0],
		       ST_KEY0X,
		       ST_KEY0Y,
		       keys,
		       &keyboxes[0],
		       &st_statusbaron);
    
    STlib_initMultIcon(&w_keyboxes[1],
		       ST_KEY1X,
		       ST_KEY1Y,
		       keys,
		       &keyboxes[1],
		       &st_statusbaron);

    STlib_initMultIcon(&w_keyboxes[2],
		       ST_KEY2X,
		       ST_KEY2Y,
		       keys,
		       &keyboxes[2],
		       &st_statusbaron);

    // ammo count (all four kinds)
    STlib_initNum(&w_ammo[0],
		  ST_AMMO0X,
		  ST_AMMO0Y,
		  shortnum,
		  &st_plyr->ammo[0],
		  &st_statusbaron,
		  ST_AMMO0WIDTH);

    STlib_initNum(&w_ammo[1],
		  ST_AMMO1X,
		  ST_AMMO1Y,
		  shortnum,
		  &st_plyr->ammo[1],
		  &st_statusbaron,
		  ST_AMMO1WIDTH);

    STlib_initNum(&w_ammo[2],
		  ST_AMMO2X,
		  ST_AMMO2Y,
		  shortnum,
		  &st_plyr->ammo[2],
		  &st_statusbaron,
		  ST_AMMO2WIDTH);
    
    STlib_initNum(&w_ammo[3],
		  ST_AMMO3X,
		  ST_AMMO3Y,
		  shortnum,
		  &st_plyr->ammo[3],
		  &st_statusbaron,
		  ST_AMMO3WIDTH);

    // max ammo count (all four kinds)
    STlib_initNum(&w_maxammo[0],
		  ST_MAXAMMO0X,
		  ST_MAXAMMO0Y,
		  shortnum,
		  &st_plyr->maxammo[0],
		  &st_statusbaron,
		  ST_MAXAMMO0WIDTH);

    STlib_initNum(&w_maxammo[1],
		  ST_MAXAMMO1X,
		  ST_MAXAMMO1Y,
		  shortnum,
		  &st_plyr->maxammo[1],
		  &st_statusbaron,
		  ST_MAXAMMO1WIDTH);

    STlib_initNum(&w_maxammo[2],
		  ST_MAXAMMO2X,
		  ST_MAXAMMO2Y,
		  shortnum,
		  &st_plyr->maxammo[2],
		  &st_statusbaron,
		  ST_MAXAMMO2WIDTH);
    
    STlib_initNum(&w_maxammo[3],
		  ST_MAXAMMO3X,
		  ST_MAXAMMO3Y,
		  shortnum,
		  &st_plyr->maxammo[3],
		  &st_statusbaron,
		  ST_MAXAMMO3WIDTH);

}

static bool	st_stopped = true;


void ST_Start(void) {
    if (!st_stopped) {
        ST_Stop();
    }
    ST_initData();
    ST_createWidgets();
    st_stopped = false;
}

void ST_Stop(void) {
    if (st_stopped) {
        return;
    }
    I_SetPalette(W_CacheLumpNum(lu_palette, PU_CACHE));
    st_stopped = true;
}

void ST_Init(void) {
    ST_loadData();
    int size_screen = ST_WIDTH * ST_HEIGHT * sizeof(*st_backing_screen);
    st_backing_screen = (pixel_t *) Z_Malloc(size_screen, PU_STATIC, 0);
}

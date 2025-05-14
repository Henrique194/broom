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
//	Player related stuff.
//	Bobbing POV/weapon, movement.
//	Pending weapon.
//


#include "doomdef.h"
#include "d_event.h"
#include "p_local.h"
#include "doomstat.h"


// Index of the special effects (INVUL inverse) map.
#define INVERSECOLORMAP 32


//
// Movement.
//

// 16 pixels of bob
#define MAXBOB 0x100000

static bool onground;


//
// P_Thrust
// Moves the given origin along a given angle.
//
static void P_Thrust(player_t* player, angle_t angle, fixed_t move) {
    player->mo->momx += FixedMul(move, COS(angle));
    player->mo->momy += FixedMul(move, SIN(angle));
}


static fixed_t P_CalcBob(const player_t* player) {
    angle_t angle = (FINEANGLES / 20 * leveltime) << ANGLETOFINESHIFT;
    fixed_t bob = FixedMul(player->bob / 2, SIN(angle));
    return bob;
}

static void P_UpdateViewZ(player_t* player) {
    fixed_t bob = P_CalcBob(player);
    player->viewz = player->mo->z + player->viewheight + bob;
    if (player->viewz > player->mo->ceilingz - 4*FRACUNIT) {
        player->viewz = player->mo->ceilingz - 4*FRACUNIT;
    }
}

static void P_UpdateViewHeight(player_t* player) {
    if (player->playerstate != PST_LIVE) {
        return;
    }
    player->viewheight += player->deltaviewheight;
    if (player->viewheight > VIEWHEIGHT) {
        player->viewheight = VIEWHEIGHT;
        player->deltaviewheight = 0;
    }
    if (player->viewheight < VIEWHEIGHT/2) {
        player->viewheight = VIEWHEIGHT/2;
        if (player->deltaviewheight <= 0) {
            player->deltaviewheight = 1;
        }
    }
    if (player->deltaviewheight) {
        player->deltaviewheight += FRACUNIT/4;
        if (!player->deltaviewheight) {
            player->deltaviewheight = 1;
        }
    }
}

//
// Regular movement bobbing (needs to be calculated for gun swing even if not on ground)
// OPTIMIZE: tablify angle
// Note: a LUT allows for effects like a ramp with low health.
//
static void P_UpdatePlayerBob(player_t* player) {
    fixed_t momx_sq = FixedMul(player->mo->momx, player->mo->momx);
    fixed_t momy_sq = FixedMul(player->mo->momy, player->mo->momy);
    player->bob = (momx_sq + momy_sq) / 4;
    if (player->bob > MAXBOB) {
        player->bob = MAXBOB;
    }
}

//
// P_CalcHeight
// Calculate the walking / running height adjustment
//
static void P_CalcHeight(player_t* player) {
    P_UpdatePlayerBob(player);
    if (!onground || (player->cheats & CF_NOMOMENTUM)) {
	player->viewz = player->mo->z + player->viewheight;
	return;
    }
    // move viewheight
    P_UpdateViewHeight(player);
    P_UpdateViewZ(player);
}


//
// P_MovePlayer
//
static void P_MovePlayer(player_t* player) {
    const ticcmd_t* cmd = &player->cmd;
    mobj_t* plr_mo = player->mo;

    plr_mo->angle += (cmd->angleturn << FRACBITS);

    // Do not let the player control movement if not on ground.
    onground = (plr_mo->z <= plr_mo->floorz);

    if (cmd->forwardmove && onground) {
        P_Thrust(player, plr_mo->angle, cmd->forwardmove * 2048);
    }
    if (cmd->sidemove && onground) {
        P_Thrust(player, plr_mo->angle - ANG90, cmd->sidemove * 2048);
    }
    if ((cmd->forwardmove || cmd->sidemove) && plr_mo->state == &states[S_PLAY]) {
        P_SetMobjState(plr_mo, S_PLAY_RUN1);
    }
}


static void P_FaceAttacker(player_t* player) {
    fixed_t x1 = player->mo->x;
    fixed_t y1 = player->mo->y;
    fixed_t x2 = player->attacker->x;
    fixed_t y2 = player->attacker->y;

    angle_t angle = R_PointToAngle2(x1, y1, x2, y2);
    angle_t delta = angle - player->mo->angle;

    if (delta < ANG5 || delta > (unsigned)-ANG5) {
        // Looking at killer, so fade damage flash down.
        player->mo->angle = angle;
        if (player->damagecount) {
            player->damagecount--;
        }
        return;
    }
    if (delta < ANG180) {
        player->mo->angle += ANG5;
        return;
    }
    player->mo->angle -= ANG5;
}

static void P_FallCameraToGround(player_t* player) {
    if (player->viewheight > 6*FRACUNIT) {
        player->viewheight -= FRACUNIT;
    }
    if (player->viewheight < 6*FRACUNIT) {
        player->viewheight = 6*FRACUNIT;
    }
    player->deltaviewheight = 0;
    onground = (player->mo->z <= player->mo->floorz);
}

//
// P_DeathThink
// Fall on your face when dying.
// Decrease POV height to floor height.
//
static void P_DeathThink(player_t* player) {
    P_MovePsprites(player);
    P_FallCameraToGround(player);
    P_CalcHeight(player);

    if (player->attacker && player->attacker != player->mo) {
        P_FaceAttacker(player);
    } else if (player->damagecount) {
        player->damagecount--;
    }
    if (player->cmd.buttons & BT_USE) {
        player->playerstate = PST_REBORN;
    }
}


static void P_UpdateColorMap(player_t* player) {
    int invul = player->powers[pw_invulnerability];
    if (invul) {
        if (invul > (4 * 32) || (invul & 8)) {
            player->fixedcolormap = INVERSECOLORMAP;
        } else {
            player->fixedcolormap = 0;
        }
        return;
    }
    int infrared = player->powers[pw_infrared];
    if (infrared) {
        if (infrared > (4 * 32) || (infrared & 8)) {
            // almost full bright
            player->fixedcolormap = 1;
        } else {
            player->fixedcolormap = 0;
        }
        return;
    }
    player->fixedcolormap = 0;
}

static void P_UpdateScreenFlashing(player_t* player) {
    if (player->damagecount) {
        player->damagecount--;
    }
    if (player->bonuscount) {
        player->bonuscount--;
    }
}

//
// Counters, time dependend power ups.
//
static void P_UpdatePowerUps(player_t* player) {
    if (player->powers[pw_strength]) {
        // Strength counts up to diminish fade.
        player->powers[pw_strength]++;
    }
    if (player->powers[pw_invulnerability]) {
        player->powers[pw_invulnerability]--;
    }
    if (player->powers[pw_invisibility]) {
        player->powers[pw_invisibility]--;
        if (!player->powers[pw_invisibility]) {
            player->mo->flags &= ~MF_SHADOW;
        }
    }
    if (player->powers[pw_infrared]) {
        player->powers[pw_infrared]--;
    }
    if (player->powers[pw_ironfeet]) {
        player->powers[pw_ironfeet]--;
    }
}

static void P_SetPendingWeapon(player_t* player) {
    // The actual changing of the weapon is done when the weapon psprite can do it
    // (read: not in the middle of an attack).
    byte buttons = player->cmd.buttons;
    weapontype_t newweapon = (buttons & BT_WEAPONMASK) >> BT_WEAPONSHIFT;

    if (newweapon == wp_fist && player->weaponowned[wp_chainsaw]
        && !(player->readyweapon == wp_chainsaw && player->powers[pw_strength])) {
        newweapon = wp_chainsaw;
    }

    if ((gamemode == commercial)
        && newweapon == wp_shotgun
        && player->weaponowned[wp_supershotgun]
        && player->readyweapon != wp_supershotgun)
    {
        newweapon = wp_supershotgun;
    }

    if (player->weaponowned[newweapon] && newweapon != player->readyweapon) {
        // Do not go to plasma or BFG in shareware, even if cheated.
        if ((newweapon != wp_plasma && newweapon != wp_bfg) || (gamemode != shareware)) {
            player->pendingweapon = newweapon;
        }
    }
}

static void P_DoButtonsActions(player_t* player) {
    ticcmd_t* cmd = &player->cmd;

    if (cmd->buttons & BT_SPECIAL) {
        // A special event has no other buttons.
        cmd->buttons = 0;
    }
    if (cmd->buttons & BT_CHANGE) {
        // Player wants to change weapon
        P_SetPendingWeapon(player);
    }
    if (cmd->buttons & BT_USE) {
        if (!player->usedown) {
            P_UseLines(player);
            player->usedown = true;
        }
    } else {
        player->usedown = false;
    }
}

static void P_TryMovePlayer(player_t* player) {
    // Reaction time is used to prevent movement for a bit after a teleport.
    if (player->mo->reactiontime) {
        player->mo->reactiontime--;
        return;
    }
    // Move around.
    P_MovePlayer(player);
}

//
// This function is responsible to give the effect of the
// chainsaw being "pulled" closer to the target.
//
static void P_DoChainSawPulledEffect(player_t* player) {
    ticcmd_t* cmd = &player->cmd;
    cmd->angleturn = 0;
    cmd->forwardmove = 0xc800 / 512;
    cmd->sidemove = 0;
    player->mo->flags &= ~MF_JUSTATTACKED;
}

static void P_UpdatePlayerNoClip(player_t* player) {
    if (player->cheats & CF_NOCLIP) {
        player->mo->flags |= MF_NOCLIP;
    } else {
        player->mo->flags &= ~MF_NOCLIP;
    }
}

//
// P_PlayerThink
//
void P_PlayerThink(player_t* player) {
    P_UpdatePlayerNoClip(player);  // fixme: do this in the cheat code
    if (player->mo->flags & MF_JUSTATTACKED) {
        P_DoChainSawPulledEffect(player);
    }
    if (player->playerstate == PST_DEAD) {
	P_DeathThink(player);
	return;
    }
    P_TryMovePlayer(player);
    P_CalcHeight(player);
    if (player->mo->subsector->sector->special) {
        P_PlayerInSpecialSector(player);
    }
    P_DoButtonsActions(player);
    P_MovePsprites(player);  // cycle player sprites
    P_UpdatePowerUps(player);
    P_UpdateScreenFlashing(player);
    P_UpdateColorMap(player);
}

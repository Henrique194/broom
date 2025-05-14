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
//	Weapon sprite animation, weapon objects.
//	Action functions for weapons.
//


#include "doomdef.h"
#include "d_event.h"
#include "d_items.h"

#include "deh_misc.h"

#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"

// State.
#include "doomstat.h"

// Data.
#include "sounds.h"

#include "p_pspr.h"

#include "a_player.h"

#define LOWERSPEED FRACUNIT * 6
#define RAISESPEED FRACUNIT * 6

#define WEAPONBOTTOM 128 * FRACUNIT
#define WEAPONTOP    32 * FRACUNIT


//
// P_SetPsprite
//
static void P_SetPsprite(player_t* player, int position, statenum_t stnum) {
    pspdef_t* psp = &player->psprites[position];

    // an initial state of 0 could cycle through
    do {
        if (stnum == 0) {
            // object removed itself
            psp->state = NULL;
            break;
        }

        state_t* state = &states[stnum];
        psp->state = state;
        psp->tics = state->tics; // could be 0

        if (state->misc1) {
            // coordinate set
            psp->sx = state->misc1 << FRACBITS;
            psp->sy = state->misc2 << FRACBITS;
        }

        // Call action routine.
        // Modified handling.
        if (state->action.acp2) {
            state->action.acp2(player, psp);
            if (psp->state == NULL) {
                break;
            }
        }

        stnum = psp->state->nextstate;
    } while (psp->tics == 0);
}



//
// P_BringUpWeapon
// Starts bringing the pending weapon up from the bottom of the screen.
// Uses player
//
static void P_BringUpWeapon(player_t* player) {
    if (player->pendingweapon == wp_nochange) {
        player->pendingweapon = player->readyweapon;
    }
    if (player->pendingweapon == wp_chainsaw) {
        S_StartSound(player->mo, sfx_sawup);
    }

    statenum_t newstate = weaponinfo[player->pendingweapon].upstate;
    player->pendingweapon = wp_nochange;
    player->psprites[ps_weapon].sy = WEAPONBOTTOM;

    P_SetPsprite(player, ps_weapon, newstate);
}

static bool P_CanSelectBfg(const player_t* player) {
    if (gamemode == shareware) {
        return false;
    }
    return player->weaponowned[wp_bfg] && player->ammo[am_cell] > 40;
}

static bool P_CanSelectRocketLauncher(const player_t* player) {
    return player->weaponowned[wp_missile] && player->ammo[am_misl];
}

static bool P_CanSelectChainSaw(const player_t* player) {
    return player->weaponowned[wp_chainsaw];
}

static bool P_CanSelectPistol(const player_t* player) {
    return player->ammo[am_clip];
}

static bool P_CanSelectShotGun(const player_t* player) {
    return player->weaponowned[wp_shotgun] && player->ammo[am_shell];
}

static bool P_CanSelectChainGun(const player_t* player) {
    return player->weaponowned[wp_chaingun] && player->ammo[am_clip];
}

static bool P_CanSelectSuperShotgun(const player_t* player) {
    if (gamemode != commercial) {
        return false;
    }
    return player->weaponowned[wp_supershotgun] && player->ammo[am_shell] > 2;
}

static bool P_CanSelectPlasmaGun(const player_t* player) {
    if (gamemode == shareware) {
        return false;
    }
    return player->weaponowned[wp_plasma] && player->ammo[am_cell];
}

//
// Pick a weapon to change to.
// Preferences are set here.
//
static weapontype_t P_SelectNextWeapon(player_t* player) {
    if (P_CanSelectPlasmaGun(player)) {
        return wp_plasma;
    }
    if (P_CanSelectSuperShotgun(player)) {
        return wp_supershotgun;
    }
    if (P_CanSelectChainGun(player)) {
        return wp_chaingun;
    }
    if (P_CanSelectShotGun(player)) {
        return wp_shotgun;
    }
    if (P_CanSelectPistol(player)) {
        return wp_pistol;
    }
    if (P_CanSelectChainSaw(player)) {
        return wp_chainsaw;
    }
    if (P_CanSelectRocketLauncher(player)) {
        return wp_missile;
    }
    if (P_CanSelectBfg(player)) {
        return wp_bfg;
    }
    // If everything fails.
    return wp_fist;
}

//
// Returns true if current ammunition is sufficient.
//
static bool P_IsAmmoSufficient(player_t* player) {
    ammotype_t ammo = weaponinfo[player->readyweapon].ammo;
    int min_ammo;

    // Minimal amount for one shot varies.
    if (player->readyweapon == wp_bfg) {
        min_ammo = deh_bfg_cells_per_shot;
    } else if (player->readyweapon == wp_supershotgun) {
        // Double barrel.
        min_ammo = 2;
    } else {
        // Regular.
        min_ammo = 1;
    }

    // Some do not need ammunition anyway.
    return ammo == am_noammo || player->ammo[ammo] >= min_ammo;
}

//
// P_CheckAmmo
// Returns true if there is enough ammo to shoot.
// If not, selects the next weapon to use.
//
static bool P_CheckAmmo(player_t* player) {
    if (P_IsAmmoSufficient(player)) {
        return true;
    }
    // Out of ammo, pick a weapon to change to.
    player->pendingweapon = P_SelectNextWeapon(player);
    // Now set appropriate weapon overlay.
    P_SetPsprite(player, ps_weapon, weaponinfo[player->readyweapon].downstate);
    return false;
}


//
// P_FireWeapon.
//
static void P_FireWeapon(player_t* player) {
    if (P_CheckAmmo(player)) {
        P_SetMobjState(player->mo, S_PLAY_ATK1);
        statenum_t newstate = weaponinfo[player->readyweapon].atkstate;
        P_SetPsprite(player, ps_weapon, newstate);
        P_NoiseAlert(player->mo, player->mo);
    }
}


//
// P_DropWeapon
// Player died, so put the weapon away.
//
void P_DropWeapon(player_t* player) {
    P_SetPsprite(player, ps_weapon, weaponinfo[player->readyweapon].downstate);
}

//
// Bob the weapon based on movement speed.
//
static void A_BobWeapon(const player_t* player, pspdef_t* psp) {
    angle_t angle = (128 * leveltime) << ANGLETOFINESHIFT;
    psp->sx = FRACUNIT + FixedMul(player->bob, COS(angle));
    // Avoid negative sine values.
    angle = angle % ANG180;
    psp->sy = WEAPONTOP + FixedMul(player->bob, SIN(angle));
}

static bool A_IsHoldingChainsaw(const player_t* player, const pspdef_t* psp) {
    return player->readyweapon == wp_chainsaw && psp->state == &states[S_SAW];
}

static bool A_InAttackState(const player_t* player) {
    return player->mo->state == &states[S_PLAY_ATK1]
           || player->mo->state == &states[S_PLAY_ATK2];
}

//
// A_WeaponReady
// The player can fire the weapon or change to another weapon at this time.
// Follows after getting weapon up, or after previous attack/fire sequence.
//
void A_WeaponReady(player_t* player, pspdef_t* psp) {
    if (A_InAttackState(player)) {
        // Get out of attack state.
        P_SetMobjState(player->mo, S_PLAY);
    }
    if (A_IsHoldingChainsaw(player, psp)) {
        S_StartSound(player->mo, sfx_sawidl);
    }

    // Check for change:
    // if player is dead, put the weapon away.
    if (player->pendingweapon != wp_nochange || player->health == 0) {
        // Change weapon (pending weapon should already be validated).
        P_DropWeapon(player);
        return;
    }

    // Check for fire:
    // the missile launcher and bfg do not auto fire.
    if (player->cmd.buttons & BT_ATTACK) {
        if (!player->attackdown || (player->readyweapon != wp_missile &&
                                    player->readyweapon != wp_bfg))
        {
            player->attackdown = true;
            P_FireWeapon(player);
            return;
        }
    } else {
        player->attackdown = false;
    }

    A_BobWeapon(player, psp);
}

//
// Check for fire.
// (if a weaponchange is pending, let it go through instead)
//
static bool A_IsReFiring(const player_t* player) {
    return (player->cmd.buttons & BT_ATTACK)
           && player->pendingweapon == wp_nochange
           && player->health;
}

//
// A_ReFire
// The player can re-fire the weapon without lowering it entirely.
//
void A_ReFire(player_t* player, pspdef_t* psp) {
    if (A_IsReFiring(player)) {
        player->refire++;
        P_FireWeapon(player);
        return;
    }
    player->refire = 0;
    P_CheckAmmo(player);
}


void A_CheckReload(player_t* player, pspdef_t* psp) {
    P_CheckAmmo(player);
}


//
// A_Lower
// Lowers current weapon, and changes weapon at bottom.
//
void A_Lower(player_t* player, pspdef_t* psp) {
    psp->sy += LOWERSPEED;
    // Is already down.
    if (psp->sy < WEAPONBOTTOM) {
        return;
    }
    // Player is dead.
    if (player->playerstate == PST_DEAD) {
        // don't bring weapon back up
        psp->sy = WEAPONBOTTOM;
        return;
    }
    // The old weapon has been lowered off the screen,
    // so change the weapon and start raising it
    if (player->health == 0) {
        // Player is dead, so keep the weapon off screen.
        P_SetPsprite(player, ps_weapon, S_NULL);
        return;
    }
    player->readyweapon = player->pendingweapon;
    P_BringUpWeapon(player);
}


//
// A_Raise
//
void A_Raise(player_t* player, pspdef_t* psp) {
    psp->sy -= RAISESPEED;
    if (psp->sy > WEAPONTOP) {
        return;
    }
    psp->sy = WEAPONTOP;

    // The weapon has been raised all the way, so change to the ready state.
    statenum_t newstate = weaponinfo[player->readyweapon].readystate;
    P_SetPsprite(player, ps_weapon, newstate);
}


//
// A_GunFlash
//
void A_GunFlash(player_t* player, pspdef_t* psp) {
    P_SetMobjState(player->mo, S_PLAY_ATK2);
    P_SetPsprite(player, ps_flash, weaponinfo[player->readyweapon].flashstate);
}


//
// WEAPON ATTACKS
//


//
// A_Punch
//
void A_Punch(player_t* player, pspdef_t* psp) {
    int damage = (P_Random() % 10 + 1) << 1;
    if (player->powers[pw_strength]) {
        damage *= 10;
    }
    angle_t angle = player->mo->angle + (P_SubRandom() << 18);
    int slope = P_AimLineAttack(player->mo, angle, MELEERANGE);
    P_LineAttack(player->mo, angle, MELEERANGE, slope, damage);

    // turn to face target
    if (linetarget) {
        S_StartSound(player->mo, sfx_punch);
        player->mo->angle = R_PointToAngle2(player->mo->x, player->mo->y,
                                            linetarget->x, linetarget->y);
    }
}

static void A_FaceTarget(player_t* player) {
    fixed_t plr_x = player->mo->x;
    fixed_t plr_y = player->mo->y;
    fixed_t target_x = linetarget->x;
    fixed_t target_y = linetarget->y;

    angle_t angle = R_PointToAngle2(plr_x, plr_y, target_x, target_y);
    angle_t diff_angle = angle - player->mo->angle;

    if (diff_angle > ANG180) {
        if ((signed int) diff_angle < -ANG90 / 20) {
            player->mo->angle = angle + ANG90 / 21;
        } else {
            player->mo->angle -= ANG90 / 20;
        }
        return;
    }
    if (diff_angle > ANG90 / 20) {
        player->mo->angle = angle - ANG90 / 21;
    } else {
        player->mo->angle += ANG90 / 20;
    }
}


//
// A_Saw
// Chainsaw
//
void A_Saw(player_t* player, pspdef_t* psp) {
    int damage = 2 * (P_Random() % 10 + 1);
    angle_t angle = player->mo->angle + (P_SubRandom() << 18);

    // Use MELEERANGE + 1 so the puff doesn't skip the flash.
    fixed_t distance = MELEERANGE + 1;
    int slope = P_AimLineAttack(player->mo, angle, distance);
    P_LineAttack(player->mo, angle, distance, slope, damage);

    if (linetarget) {
        S_StartSound(player->mo, sfx_sawhit);
        // turn to face target
        A_FaceTarget(player);
        player->mo->flags |= MF_JUSTATTACKED;
        return;
    }
    S_StartSound(player->mo, sfx_sawful);
}

//
// Doom does not check the bounds of the ammo array.  As a result,
// it is possible to use an ammo type > 4 that overflows into the
// maxammo array and affects that instead.  Through dehacked, for
// example, it is possible to make a weapon that decreases the max
// number of ammo for another weapon.  Emulate this.
//
static void DecreaseAmmo(player_t* player, int ammonum, int amount) {
    if (ammonum < NUMAMMO) {
        player->ammo[ammonum] -= amount;
        return;
    }
    player->maxammo[ammonum - NUMAMMO] -= amount;
}


//
// A_FireMissile
//
void A_FireMissile(player_t* player, pspdef_t* psp) {
    int ammo = weaponinfo[player->readyweapon].ammo;
    int amount = 1;
    DecreaseAmmo(player, ammo, amount);
    P_SpawnPlayerMissile(player->mo, MT_ROCKET);
}


//
// A_FireBFG
//
void A_FireBFG(player_t* player, pspdef_t* psp) {
    int ammo = weaponinfo[player->readyweapon].ammo;
    int amount = deh_bfg_cells_per_shot;
    DecreaseAmmo(player, ammo, amount);
    P_SpawnPlayerMissile(player->mo, MT_BFG);
}


//
// A_FirePlasma
//
void A_FirePlasma(player_t* player, pspdef_t* psp) {
    const weaponinfo_t* ready_weapon = &weaponinfo[player->readyweapon];
    DecreaseAmmo(player, ready_weapon->ammo, 1);
    P_SetPsprite(player, ps_flash, ready_weapon->flashstate + (P_Random() & 1));
    P_SpawnPlayerMissile(player->mo, MT_PLASMA);
}


fixed_t bulletslope;

//
// P_BulletSlope
// Sets a slope so a near miss is at approximately the height of the intended target
//
static void P_BulletSlope(mobj_t* mo) {
    // see which target is to be aimed at
    angle_t an = mo->angle;
    bulletslope = P_AimLineAttack(mo, an, 16 * 64 * FRACUNIT);
    if (linetarget) {
        return;
    }

    an += 1 << 26;
    bulletslope = P_AimLineAttack(mo, an, 16 * 64 * FRACUNIT);
    if (linetarget) {
        return;
    }

    an -= 2 << 26;
    bulletslope = P_AimLineAttack(mo, an, 16 * 64 * FRACUNIT);
}


//
// P_GunShot
//
static void P_GunShot(mobj_t* mo, bool accurate) {
    int damage = 5 * (P_Random() % 3 + 1);
    angle_t angle = mo->angle;
    if (!accurate) {
        angle += P_SubRandom() << 18;
    }
    P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage);
}


//
// A_FirePistol
//
void A_FirePistol(player_t *player, pspdef_t *psp) {
    S_StartSound(player->mo, sfx_pistol);

    P_SetMobjState(player->mo, S_PLAY_ATK2);
    DecreaseAmmo(player, weaponinfo[player->readyweapon].ammo, 1);

    P_SetPsprite(player, ps_flash, weaponinfo[player->readyweapon].flashstate);

    P_BulletSlope(player->mo);
    P_GunShot(player->mo, !player->refire);
}


//
// A_FireShotgun
//
void
A_FireShotgun
( player_t*	player,
  pspdef_t*	psp ) 
{
    int		i;
	
    S_StartSound (player->mo, sfx_shotgn);
    P_SetMobjState (player->mo, S_PLAY_ATK2);

    DecreaseAmmo(player, weaponinfo[player->readyweapon].ammo, 1);

    P_SetPsprite (player,
		  ps_flash,
		  weaponinfo[player->readyweapon].flashstate);

    P_BulletSlope (player->mo);
	
    for (i=0 ; i<7 ; i++)
	P_GunShot (player->mo, false);
}



//
// A_FireShotgun2
//
void
A_FireShotgun2
( player_t*	player,
  pspdef_t*	psp ) 
{
    int		i;
    angle_t	angle;
    int		damage;
		
	
    S_StartSound (player->mo, sfx_dshtgn);
    P_SetMobjState (player->mo, S_PLAY_ATK2);

    DecreaseAmmo(player, weaponinfo[player->readyweapon].ammo, 2);

    P_SetPsprite (player,
		  ps_flash,
		  weaponinfo[player->readyweapon].flashstate);

    P_BulletSlope (player->mo);
	
    for (i=0 ; i<20 ; i++)
    {
	damage = 5*(P_Random ()%3+1);
	angle = player->mo->angle;
	angle += P_SubRandom() << ANGLETOFINESHIFT;
	P_LineAttack (player->mo,
		      angle,
		      MISSILERANGE,
		      bulletslope + (P_SubRandom() << 5), damage);
    }
}

void A_OpenShotgun2(player_t* player, pspdef_t* psp) {
    S_StartSound(player->mo, sfx_dbopn);
}

void A_LoadShotgun2(player_t* player, pspdef_t* psp) {
    S_StartSound(player->mo, sfx_dbload);
}

void A_CloseShotgun2(player_t* player, pspdef_t* psp) {
    S_StartSound(player->mo, sfx_dbcls);
    A_ReFire(player, psp);
}


//
// A_FireCGun
//
void A_FireCGun(player_t* player, pspdef_t* psp) {
    // Vanilla Bug: the chaingun makes two sounds even if firing a
    // single bullet. This happens because we play the chaingun sound
    // before checking if we have ammo.
    // More info on this bug here:
    // https://doomwiki.org/wiki/Chaingun_makes_two_sounds_firing_single_bullet
    S_StartSound(player->mo, sfx_pistol);
    const weaponinfo_t* weapon = &weaponinfo[player->readyweapon];

    if (!player->ammo[weapon->ammo]) {
        return;
    }

    P_SetMobjState(player->mo, S_PLAY_ATK2);
    DecreaseAmmo(player, weapon->ammo, 1);

    statenum_t flash_state = weapon->flashstate;
    if (psp->state == &states[S_CHAIN2]) {
        // If the chaingun is in the second firing state (S_CHAIN2),
        // use a different flash state (S_CHAINFLASH2). This variation
        // creates a more realistic effect by adding variety to the
        // lighting emitted by each shot.
        flash_state++;
    }
    P_SetPsprite(player, ps_flash, flash_state);

    P_BulletSlope(player->mo);
    P_GunShot(player->mo, !player->refire);
}


//
// ?
//
void A_Light0(player_t* player, pspdef_t* psp) {
    player->extralight = 0;
}

void A_Light1(player_t* player, pspdef_t* psp) {
    player->extralight = 1;
}

void A_Light2(player_t* player, pspdef_t* psp) {
    player->extralight = 2;
}


//
// A_BFGSpray
// Spawn a BFG explosion on every monster in view
//
void A_BFGSpray (mobj_t* mo) 
{
    int			i;
    int			j;
    int			damage;
    angle_t		an;
	
    // offset angles from its attack angle
    for (i=0 ; i<40 ; i++)
    {
	an = mo->angle - ANG90/2 + ANG90/40*i;

	// mo->target is the originator (player)
	//  of the missile
	P_AimLineAttack (mo->target, an, 16*64*FRACUNIT);

	if (!linetarget)
	    continue;

	P_SpawnMobj (linetarget->x,
		     linetarget->y,
		     linetarget->z + (linetarget->height>>2),
		     MT_EXTRABFG);
	
	damage = 0;
	for (j=0;j<15;j++)
	    damage += (P_Random()&7) + 1;

	P_DamageMobj (linetarget, mo->target,mo->target, damage);
    }
}


//
// A_BFGsound
//
void A_BFGsound(player_t *player, pspdef_t *psp) {
    S_StartSound(player->mo, sfx_bfg);
}


//
// P_SetupPsprites
// Called at start of level for each player.
//
void P_SetupPsprites(player_t* player) {
    // Remove all psprites.
    for (int i = 0 ; i < NUMPSPRITES; i++) {
        player->psprites[i].state = NULL;
    }

    // Spawn the gun.
    player->pendingweapon = player->readyweapon;
    P_BringUpWeapon(player);
}


//
// P_MovePsprites
// Called every tic by player thinking routine.
//
void P_MovePsprites(player_t* player) {
    pspdef_t* psp = &player->psprites[0];

    for (int i = 0; i < NUMPSPRITES; i++, psp++) {
        // a null state means not active
        if (psp->state == NULL) {
            continue;
        }
        // a -1 tic count never changes
        if (psp->tics == -1) {
            continue;
        }
        // drop tic count and possibly change state
        psp->tics--;
        if (psp->tics == 0) {
            P_SetPsprite(player, i, psp->state->nextstate);
        }
    }

    player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
    player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
}

//
// Player death scream.
//
void A_PlayerScream(mobj_t *mo) {
    // Default death sound.
    int sound = sfx_pldeth;
    if ((gamemode == commercial) && (mo->health < -50)) {
        // If the player dies less than -50% without gibbing.
        sound = sfx_pdiehi;
    }
    S_StartSound(mo, sound);
}

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
//	Handling interactions (i.e., collisions).
//


#include "p_inter.h"

#include "doomdef.h"
#include "dstrings.h"
#include "d_items.h"
#include "deh_str.h"
#include "deh_misc.h"
#include "doomstat.h"
#include "m_random.h"
#include "i_system.h"
#include "am_map.h"
#include "p_local.h"
#include "s_sound.h"


#define BONUSADD 6

// a weapon is found with two clip loads, a big item has five clip loads
int maxammo[NUMAMMO] = {200, 50, 300, 50};
int clipammo[NUMAMMO] = {10, 4, 20, 1};


//
// GET STUFF
//

static bool P_CanSelectRocketLauncher(const player_t* player) {
    return player->readyweapon == wp_fist && player->weaponowned[wp_missile];
}

static bool P_CanSelectPlasmaGun(const player_t* player) {
    if (player->readyweapon != wp_fist && player->readyweapon != wp_pistol) {
        return false;
    }
    return player->weaponowned[wp_plasma];
}

static bool P_CanSelectShotGun(const player_t* player) {
    if (player->readyweapon != wp_fist && player->readyweapon != wp_pistol) {
        return false;
    }
    return player->weaponowned[wp_shotgun];
}

static bool P_CanSelectPistol(const player_t* player) {
    return player->readyweapon == wp_fist;
}

static bool P_CanSelectChainGun(const player_t* player) {
    return player->readyweapon == wp_fist && player->weaponowned[wp_chaingun];
}

//
// Select new weapon.
// Preferences are not user selectable.
//
static void P_SelectNewWeapon(player_t* player, ammotype_t ammo) {
    switch (ammo) {
        case am_clip:
            if (P_CanSelectChainGun(player)) {
                player->pendingweapon = wp_chaingun;
            } else if (P_CanSelectPistol(player)) {
                player->pendingweapon = wp_pistol;
            }
            break;
        case am_shell:
            if (P_CanSelectShotGun(player)) {
                player->pendingweapon = wp_shotgun;
            }
            break;
        case am_cell:
            if (P_CanSelectPlasmaGun(player)) {
                player->pendingweapon = wp_plasma;
            }
            break;
        case am_misl:
            if (P_CanSelectRocketLauncher(player)) {
                player->pendingweapon = wp_missile;
            }
            break;
        default:
            break;
    }
}

static int P_CalculateClipRounds(ammotype_t ammo, int num) {
    int rounds;
    if (num) {
        rounds = num * clipammo[ammo];
    } else {
        rounds = clipammo[ammo] / 2;
    }
    if (gameskill == sk_baby || gameskill == sk_nightmare) {
        // give double ammo in trainer mode, you'll need in nightmare
        rounds <<= 1;
    }
    return rounds;
}

static void P_UpdatePlayerAmmo(player_t* player, ammotype_t ammo, int num) {
    int rounds = P_CalculateClipRounds(ammo, num);
    player->ammo[ammo] += rounds;
    if (player->ammo[ammo] > player->maxammo[ammo]) {
        player->ammo[ammo] = player->maxammo[ammo];
    }
}

//
// P_GiveAmmo
// Num is the number of clip loads, not the individual count (0 = 1/2 clip).
// Returns false if the ammo can't be picked up at all.
//
static bool P_GiveAmmo(player_t* player, ammotype_t ammo, int num) {
    if (ammo == am_noammo) {
        return false;
    }
    if (ammo >= NUMAMMO) {
        I_Error("P_GiveAmmo: bad type %i", ammo);
    }
    if (player->ammo[ammo] == player->maxammo[ammo]) {
        return false;
    }

    int oldammo = player->ammo[ammo];
    P_UpdatePlayerAmmo(player, ammo, num);
    if (oldammo == 0) {
        // We were down to zero, so select a new weapon.
        // Note that in case ammo was non zero, we don't change up weapons,
        // because player was lower on purpose.
        P_SelectNewWeapon(player, ammo);
    }

    return true;
}


//
// P_GiveWeapon
// The weapon name may have a MF_DROPPED flag ored in.
//
static bool P_GiveWeapon(player_t* player, weapontype_t weapon,
                            bool dropped)
{
    bool gaveammo;
    bool gaveweapon;

    if (netgame && (deathmatch != 2) && !dropped) {
        // leave placed weapons forever on net games
        if (player->weaponowned[weapon]) {
            return false;
        }

        player->bonuscount += BONUSADD;
        player->weaponowned[weapon] = true;

        if (deathmatch) {
            P_GiveAmmo(player, weaponinfo[weapon].ammo, 5);
        } else {
            P_GiveAmmo(player, weaponinfo[weapon].ammo, 2);
        }
        player->pendingweapon = weapon;
        if (player == &players[consoleplayer]) {
            S_StartSound(NULL, sfx_wpnup);
        }
        return false;
    }

    if (weaponinfo[weapon].ammo != am_noammo) {
        // give one clip with a dropped weapon, two clips with a found weapon
        if (dropped) {
            gaveammo = P_GiveAmmo(player, weaponinfo[weapon].ammo, 1);
        } else {
            gaveammo = P_GiveAmmo(player, weaponinfo[weapon].ammo, 2);
        }
    } else {
        gaveammo = false;
    }

    if (player->weaponowned[weapon]) {
        gaveweapon = false;
    } else {
        gaveweapon = true;
        player->weaponowned[weapon] = true;
        player->pendingweapon = weapon;
    }

    return (gaveweapon || gaveammo);
}


//
// P_GiveHealth
// Returns false if the health isn't needed at all
//
static bool P_GiveHealth(player_t* player, int num) {
    if (player->health >= MAXHEALTH) {
        return false;
    }
    player->health += num;
    if (player->health > MAXHEALTH) {
        player->health = MAXHEALTH;
    }
    player->mo->health = player->health;
    return true;
}


//
// P_GiveArmor
// Returns false if the armor is worse than the current armor.
//
static bool P_GiveArmor(player_t* player, int armortype) {
    int hits = armortype * 100;
    if (player->armorpoints >= hits) {
        // don't pick up
        return false;
    }
    player->armortype = armortype;
    player->armorpoints = hits;
    return true;
}


//
// P_GiveCard
//
static void P_GiveCard(player_t* player, card_t card) {
    if (player->cards[card]) {
        // already have card
        return;
    }
    player->bonuscount = BONUSADD;
    player->cards[card] = true;
}


//
// P_GivePower
//
bool P_GivePower(player_t* player, int power) {
    switch (power) {
        case pw_invulnerability:
            player->powers[power] = INVULNTICS;
            return true;
        case pw_invisibility:
            player->powers[power] = INVISTICS;
            player->mo->flags |= MF_SHADOW;
            return true;
        case pw_infrared:
            player->powers[power] = INFRATICS;
            return true;
        case pw_ironfeet:
            player->powers[power] = IRONTICS;
            return true;
        case pw_strength:
            P_GiveHealth(player, 100);
            player->powers[power] = 1;
            return true;
        default:
            if (player->powers[power]) {
                // already got it
                return false;
            }
            player->powers[power] = 1;
            return true;
    }
}

//
// The sound that should be used when picking up an item.
//
static int pickup_sound;

//
// Removes the special item after being picked up by the player.
//
static void P_RemoveSpecial(mobj_t* special, player_t* player) {
    if (special->flags & MF_COUNTITEM) {
        player->itemcount++;
    }
    P_RemoveMobj(special);
    player->bonuscount += BONUSADD;
    if (player == &players[consoleplayer]) {
        S_StartSound(NULL, pickup_sound);
    }
}

//
// Returns a status code based on the handling of the "special" object:
//
// -1: If the "special" object cannot be handled by the function
//  0: If the "special" object can be handled but can not picked up
//  1: If the "special" object was successfully picked up by the player
//
// Parameters:
//   special: A pointer to the "special" object to be interacted with
//   player:  A pointer to the player attempting to interact with the
//            "special" object
//
typedef int (*pickup_func_t)(const mobj_t* special, player_t* player);

static int P_PickUpWeapon(const mobj_t* special, player_t* player) {
    bool dropped;

    switch (special->sprite) {
        case SPR_BFUG:
            if (P_GiveWeapon(player, wp_bfg, false)) {
                player->message = DEH_String(GOTBFG9000);
                pickup_sound = sfx_wpnup;
                return true;
            }
            return false;
        case SPR_MGUN:
            dropped = (special->flags & MF_DROPPED) != 0;
            if (P_GiveWeapon(player, wp_chaingun, dropped)) {
                player->message = DEH_String(GOTCHAINGUN);
                pickup_sound = sfx_wpnup;
                return true;
            }
            return false;
        case SPR_CSAW:
            if (P_GiveWeapon(player, wp_chainsaw, false)) {
                player->message = DEH_String(GOTCHAINSAW);
                pickup_sound = sfx_wpnup;
                return true;
            }
            return false;
        case SPR_LAUN:
            if (P_GiveWeapon(player, wp_missile, false)) {
                player->message = DEH_String(GOTLAUNCHER);
                pickup_sound = sfx_wpnup;
                return true;
            }
            return false;
        case SPR_PLAS:
            if (P_GiveWeapon(player, wp_plasma, false)) {
                player->message = DEH_String(GOTPLASMA);
                pickup_sound = sfx_wpnup;
                return true;
            }
            return false;
        case SPR_SHOT:
            dropped = (special->flags & MF_DROPPED) != 0;
            if (P_GiveWeapon(player, wp_shotgun, dropped)) {
                player->message = DEH_String(GOTSHOTGUN);
                pickup_sound = sfx_wpnup;
                return true;
            }
            return false;
        case SPR_SGN2:
            dropped = (special->flags & MF_DROPPED) != 0;
            if (P_GiveWeapon(player, wp_supershotgun, dropped)) {
                player->message = DEH_String(GOTSHOTGUN2);
                pickup_sound = sfx_wpnup;
                return true;
            }
            return false;
        default:
            return -1;
    }
}

static int P_PickUpAmmo(const mobj_t* special, player_t* player) {
    switch (special->sprite) {
        case SPR_CLIP:
            if (special->flags & MF_DROPPED) {
                if (!P_GiveAmmo(player, am_clip, 0)) {
                    return false;
                }
            } else {
                if (!P_GiveAmmo(player, am_clip, 1)) {
                    return false;
                }
            }
            player->message = DEH_String(GOTCLIP);
            return true;
        case SPR_AMMO:
            if (P_GiveAmmo(player, am_clip, 5)) {
                player->message = DEH_String(GOTCLIPBOX);
                return true;
            }
            return false;
        case SPR_ROCK:
            if (P_GiveAmmo(player, am_misl, 1)) {
                player->message = DEH_String(GOTROCKET);
                return true;
            }
            return false;
        case SPR_BROK:
            if (P_GiveAmmo(player, am_misl, 5)) {
                player->message = DEH_String(GOTROCKBOX);
                return true;
            }
            return false;
        case SPR_CELL:
            if (P_GiveAmmo(player, am_cell, 1)) {
                player->message = DEH_String(GOTCELL);
                return true;
            }
            return false;
        case SPR_CELP:
            if (P_GiveAmmo(player, am_cell, 5)) {
                player->message = DEH_String(GOTCELLBOX);
                return true;
            }
            return false;
        case SPR_SHEL:
            if (P_GiveAmmo(player, am_shell, 1)) {
                player->message = DEH_String(GOTSHELLS);
                return true;
            }
            return false;
        case SPR_SBOX:
            if (P_GiveAmmo(player, am_shell, 5)) {
                player->message = DEH_String(GOTSHELLBOX);
                return true;
            }
            return false;
        case SPR_BPAK:
            if (!player->backpack) {
                for (int i = 0; i < NUMAMMO; i++) {
                    player->maxammo[i] *= 2;
                }
                player->backpack = true;
            }
            for (int i = 0; i < NUMAMMO; i++) {
                P_GiveAmmo(player, i, 1);
            }
            player->message = DEH_String(GOTBACKPACK);
            return true;
        default:
            return -1;
    }
}

static int P_PickUpPowerUp(const mobj_t* special, player_t* player) {
    switch (special->sprite) {
        case SPR_PINV:
            if (P_GivePower(player, pw_invulnerability)) {
                player->message = DEH_String(GOTINVUL);
                if (gameversion > exe_doom_1_2) {
                    pickup_sound = sfx_getpow;
                }
                return true;
            }
            return false;
        case SPR_PSTR:
            if (P_GivePower(player, pw_strength)) {
                player->message = DEH_String(GOTBERSERK);
                if (player->readyweapon != wp_fist) {
                    player->pendingweapon = wp_fist;
                }
                if (gameversion > exe_doom_1_2) {
                    pickup_sound = sfx_getpow;
                }
                return true;
            }
            return false;
        case SPR_PINS:
            if (P_GivePower(player, pw_invisibility)) {
                player->message = DEH_String(GOTINVIS);
                if (gameversion > exe_doom_1_2) {
                    pickup_sound = sfx_getpow;
                }
                return true;
            }
            return false;
        case SPR_SUIT:
            if (P_GivePower(player, pw_ironfeet)) {
                player->message = DEH_String(GOTSUIT);
                if (gameversion > exe_doom_1_2) {
                    pickup_sound = sfx_getpow;
                }
                return true;
            }
            return false;
        case SPR_PMAP:
            if (P_GivePower(player, pw_allmap)) {
                player->message = DEH_String(GOTMAP);
                if (gameversion > exe_doom_1_2) {
                    pickup_sound = sfx_getpow;
                }
                return true;
            }
            return false;
        case SPR_PVIS:
            if (P_GivePower(player, pw_infrared)) {
                player->message = DEH_String(GOTVISOR);
                if (gameversion > exe_doom_1_2) {
                    pickup_sound = sfx_getpow;
                }
                return true;
            }
            return false;
        default:
            return -1;
    }
}

static int P_PickUpHealthItem(const mobj_t* special, player_t* player) {
    switch (special->sprite) {
        case SPR_STIM:
            if (P_GiveHealth(player, 10)) {
                player->message = DEH_String(GOTSTIM);
                return true;
            }
            return false;
        case SPR_MEDI:
            if (P_GiveHealth(player, 25)) {
                // Vanilla Bug: the first condition is never entered, i.e.
                // the GOTMEDINEED is never displayed.
                // More info on this bug here:
                // https://doomwiki.org/wiki/Picked_up_a_medikit_that_you_REALLY_need!
                if (player->health < 25) {
                    player->message = DEH_String(GOTMEDINEED);
                } else {
                    player->message = DEH_String(GOTMEDIKIT);
                }
                return true;
            }
            return false;
        default:
            return -1;
    }
}

static int P_PickUpCards(const mobj_t* special, player_t* player) {
    switch (special->sprite) {
        case SPR_BKEY:
            if (!player->cards[it_bluecard]) {
                player->message = DEH_String(GOTBLUECARD);
            }
            P_GiveCard(player, it_bluecard);
            if (!netgame) {
                return true;
            }
            return false;
        case SPR_YKEY:
            if (!player->cards[it_yellowcard]) {
                player->message = DEH_String(GOTYELWCARD);
            }
            P_GiveCard(player, it_yellowcard);
            if (!netgame) {
                return true;
            }
            return false;
        case SPR_RKEY:
            if (!player->cards[it_redcard]) {
                player->message = DEH_String(GOTREDCARD);
            }
            P_GiveCard(player, it_redcard);
            if (!netgame) {
                return true;
            }
            return false;
        case SPR_BSKU:
            if (!player->cards[it_blueskull]) {
                player->message = DEH_String(GOTBLUESKUL);
            }
            P_GiveCard(player, it_blueskull);
            if (!netgame) {
                return true;
            }
            return false;
        case SPR_YSKU:
            if (!player->cards[it_yellowskull]) {
                player->message = DEH_String(GOTYELWSKUL);
            }
            P_GiveCard(player, it_yellowskull);
            if (!netgame) {
                return true;
            }
            return false;
        case SPR_RSKU:
            if (!player->cards[it_redskull]) {
                player->message = DEH_String(GOTREDSKULL);
            }
            P_GiveCard(player, it_redskull);
            if (!netgame) {
                return true;
            }
            return false;
        default:
            return -1;
    }
}

static int P_PickUpBonus(const mobj_t* special, player_t* player) {
    switch (special->sprite) {
        case SPR_BON1:
            player->health++; // can go over 100%
            if (player->health > deh_max_health) {
                player->health = deh_max_health;
            }
            player->mo->health = player->health;
            player->message = DEH_String(GOTHTHBONUS);
            return true;
        case SPR_BON2:
            player->armorpoints++; // can go over 100%
            if (player->armorpoints > deh_max_armor && gameversion > exe_doom_1_2) {
                player->armorpoints = deh_max_armor;
            }
            // deh_green_armor_class only applies to the green armor shirt;
            // for the armor helmets, armortype 1 is always used.
            if (!player->armortype) {
                player->armortype = 1;
            }
            player->message = DEH_String(GOTARMBONUS);
            return true;
        case SPR_SOUL:
            player->health += deh_soulsphere_health;
            if (player->health > deh_max_soulsphere) {
                player->health = deh_max_soulsphere;
            }
            player->mo->health = player->health;
            player->message = DEH_String(GOTSUPER);
            if (gameversion > exe_doom_1_2) {
                pickup_sound = sfx_getpow;
            }
            return true;
        case SPR_MEGA:
            if (gamemode != commercial) {
                return false;
            }
            player->health = deh_megasphere_health;
            player->mo->health = player->health;
            // We always give armor type 2 for the megasphere;
            // dehacked only affects the MegaArmor.
            P_GiveArmor(player, 2);
            player->message = DEH_String(GOTMSPHERE);
            if (gameversion > exe_doom_1_2) {
                pickup_sound = sfx_getpow;
            }
            return true;
        default:
            return -1;
    }
}

static int P_PickUpArmor(const mobj_t* special, player_t* player) {
    switch (special->sprite) {
        case SPR_ARM1:
            if (P_GiveArmor(player, deh_green_armor_class)) {
                player->message = DEH_String(GOTARMOR);
                return true;
            }
            return false;
        case SPR_ARM2:
            if (P_GiveArmor(player, deh_blue_armor_class)) {
                player->message = DEH_String(GOTMEGA);
                return true;
            }
            return false;
        default:
            return -1;
    }
}

static pickup_func_t pickup_funcs[] = {
    &P_PickUpArmor,
    &P_PickUpBonus,
    &P_PickUpCards,
    &P_PickUpHealthItem,
    &P_PickUpPowerUp,
    &P_PickUpAmmo,
    &P_PickUpWeapon,
};

static bool P_PickUp(const mobj_t* special, player_t* player) {
    // Default pick up sound.
    pickup_sound = sfx_itemup;

    for (int i = 0; i < arrlen(pickup_funcs); i++) {
        pickup_func_t func = pickup_funcs[i];
        int picked_up = func(special, player);
        if (picked_up != -1) {
            return picked_up;
        }
    }
    I_Error("P_PickUp: Unknown gettable thing");
}

//
// P_TouchSpecialThing
//
void P_TouchSpecialThing(mobj_t* special, mobj_t* toucher) {
    player_t* player = toucher->player;
    fixed_t dz = special->z - toucher->z;
    if (dz > toucher->height || dz < (-8 * FRACUNIT)) {
        // Out of reach.
        return;
    }
    if (toucher->health <= 0) {
        // Dead thing touching.
        // Can happen with a sliding player corpse.
        return;
    }
    if (P_PickUp(special, player)) {
        P_RemoveSpecial(special, player);
    }
}



//
// Drop stuff.
// This determines the kind of object spawned during the death frame of a thing.
//
static void P_DropItem(const mobj_t* target) {
    mobjtype_t item;
    switch (target->type) {
        case MT_WOLFSS:
        case MT_POSSESSED:
            item = MT_CLIP;
            break;
        case MT_SHOTGUY:
            item = MT_SHOTGUN;
            break;
        case MT_CHAINGUY:
            item = MT_CHAINGUN;
            break;
        default:
            return;
    }

    mobj_t* mo = P_SpawnMobj(target->x, target->y, ONFLOORZ, item);
    mo->flags |= MF_DROPPED; // special versions of items
}

static void P_DecrementTics(mobj_t* target) {
    target->tics -= P_Random() & 3;
    if (target->tics < 1) {
        target->tics = 1;
    }
}

static bool P_IsGibbed(const mobj_t* target) {
    return target->health < -target->info->spawnhealth
           && target->info->xdeathstate;
}

static void P_SetDeathState(mobj_t* target) {
    if (P_IsGibbed(target)) {
        P_SetMobjState(target, target->info->xdeathstate);
        return;
    }
    P_SetMobjState(target, target->info->deathstate);
}

static void P_KillPlayer(mobj_t* target) {
    target->flags &= ~MF_SOLID;
    target->player->playerstate = PST_DEAD;
    P_DropWeapon(target->player);
    if (target->player == &players[consoleplayer] && automapactive) {
        // don't die in auto map, switch view prior to dying
        AM_Stop();
    }
}

static void P_CountMobjDeath(mobj_t* source, mobj_t* target) {
    if (source && source->player) {
        // count for intermission
        if (target->flags & MF_COUNTKILL) {
            source->player->killcount++;
        }
        if (target->player) {
            source->player->frags[target->player - players]++;
        }
    } else if (!netgame && (target->flags & MF_COUNTKILL)) {
        // count all monster deaths, even those caused by other monsters
        players[0].killcount++;
    }
    if (target->player && !source) {
        // count environment kills against you
        target->player->frags[target->player - players]++;
    }
}

static void P_FallToGround(mobj_t* target) {
    target->height >>= 2;
}

static void P_SetDeathFlags(mobj_t* target) {
    if (target->type != MT_SKULL) {
        target->flags &= ~MF_NOGRAVITY;
    }
    target->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY);
    target->flags |= (MF_CORPSE | MF_DROPOFF);
}

//
// KillMobj
//
static void P_KillMobj(mobj_t* source, mobj_t* target) {
    P_SetDeathFlags(target);
    P_FallToGround(target);
    P_CountMobjDeath(source, target);
    if (target->player) {
        P_KillPlayer(target);
    }
    P_SetDeathState(target);
    P_DecrementTics(target);
    if (gameversion != exe_chex) {
        // In Chex Quest, monsters don't drop items.
        P_DropItem(target);
    }
}



static bool P_IsInvulnerable(const player_t* player) {
    return (player->cheats & CF_GODMODE) || player->powers[pw_invulnerability];
}

//
// Below certain threshold, ignore damage in GOD mode, or with INVUL power.
//
static bool P_CanEnterPainState(const mobj_t* target) {
    if (P_Random() >= target->info->painchance) {
        return false;
    }
    if (target->flags & MF_SKULLFLY) {
        return false;
    }
    return true;
}

static bool P_CanChaseAttacker(const mobj_t* target, const mobj_t* source) {
    if (target->threshold && target->type != MT_VILE) {
        // already chasing another attacker
        return false;
    }
    if (source == NULL) {
        // no attacker to chase
        return false;
    }
    if (source == target && gameversion > exe_doom_1_2) {
        // do not chase yourself
        return false;
    }
    if (source->type == MT_VILE) {
        // do not chase archies
        return false;
    }
    return true;
}

static void P_ChaseAttacker(mobj_t* target, mobj_t* source) {
    target->target = source;
    target->threshold = BASETHRESHOLD;

    const state_t* spawn_state = &states[target->info->spawnstate];
    int see_state = target->info->seestate;

    if (target->state == spawn_state && see_state != S_NULL) {
        P_SetMobjState(target, see_state);
    }
}

static void P_WakeUpThing(mobj_t* target) {
    target->reactiontime = 0;
}

static void P_DamageThing(mobj_t* target, mobj_t* source, int damage) {
    target->health -= damage;

    if (target->health <= 0) {
        P_KillMobj(source, target);
        return;
    }
    if (P_CanEnterPainState(target)) {
        // fight back!
        target->flags |= MF_JUSTHIT;
        P_SetMobjState(target, target->info->painstate);
    }
    P_WakeUpThing(target);
    if (P_CanChaseAttacker(target, source)) {
        // if not intent on another player, chase after this one
        P_ChaseAttacker(target, source);
    }
}

//
// Reduces damage using armor, if available.
// Return the amount of damage that is not absorbed
// by the armor (i.e., remaining damage).
//
static int P_UseArmor(player_t* player, int damage) {
    if (player->armortype == 0) {
        // no armor
        return damage;
    }

    int saved = (player->armortype == 1 ? damage / 3 : damage / 2);
    if (player->armorpoints <= saved) {
        // armor is used up
        saved = player->armorpoints;
        player->armortype = 0;
    }
    player->armorpoints -= saved;

    return damage - saved;
}

static void P_DamagePlayer(player_t* player, mobj_t* source, int damage) {
    player->health -= damage;
    player->damagecount += damage;
    player->attacker = source;

    if (player->health < 0) {
        player->health = 0;
    }
    if (player->damagecount > 100) {
        // teleport stomp does 10k points...
        player->damagecount = 100;
    }
}

static bool P_CanDamagePlayer(const player_t* player, int damage) {
    if (damage >= 1000) {
        return true;
    }
    return !P_IsInvulnerable(player);
}

static bool P_CanDamage(const mobj_t* target, int damage) {
    if (target->player) {
        return P_CanDamagePlayer(target->player, damage);
    }
    return true;
}

static bool P_ShouldFallForward(const mobj_t* target,
                                   const mobj_t* inflictor, int damage)
{
    fixed_t height_diff = target->z - inflictor->z;
    return damage < 40
           && damage > target->health
           && height_diff > (64 * FRACUNIT)
           && (P_Random() % 2 != 0);
}

static void P_ApplyDamagePush(mobj_t *target, const mobj_t *inflictor,
                              int damage)
{
    angle_t ang = R_PointToAngle2(inflictor->x, inflictor->y, target->x, target->y);
    fixed_t thrust = damage * (FRACUNIT >> 3) * 100 / target->info->mass;

    // make fall forwards sometimes
    if (P_ShouldFallForward(target, inflictor, damage)) {
        ang += ANG180;
        thrust *= 4;
    }

    target->momx += FixedMul(thrust, COS(ang));
    target->momy += FixedMul(thrust, SIN(ang));
}

//
// End of game hell hack.
//
static int P_ApplyE1M8Hack(const mobj_t* target, int damage) {
    if (target->player == NULL) {
        // Hack only applies if the target is a player.
        return damage;
    }
    if (damage < target->health) {
        // Hack only applies for big damage.
        return damage;
    }
    if (target->subsector->sector->special != 11) {
        // Hack only applies if player is in special sector.
        return damage;
    }
    return target->health - 1;
}

//
// Some close combat weapons should not inflict thrust and push the victim
// out of reach, thus kick away unless using the chainsaw.
//
static bool P_CanPushMobj(const mobj_t* target, const mobj_t* inflictor,
                             const mobj_t* source)
{
    if (inflictor == NULL) {
        // damage is not inflicted by a movable object
        // (e.g., slime, environmental hazards)
        return false;
    }
    if (target->flags & MF_NOCLIP) {
        // disabled collision, so no push
        return false;
    }
    if (source == NULL || source->player == NULL) {
        // attacker is not player, so always apply push
        return true;
    }
    // chainsaw does not trigger a push effect
    return source->player->readyweapon != wp_chainsaw;
}

//
// Adjusts damage based on level difficulty.
//
static int P_AdjustDamage(const mobj_t* target, int damage) {
    if (target->player && gameskill == sk_baby) {
        // take half damage in trainer mode
        return damage >> 1;
    }
    return damage;
}

//
// P_DamageMobj
// Damages both enemies and players.
//
// "target":
//     The thing taking the damage.
//
// "inflictor":
//     The thing responsible for causing the damage. This can be a creature,
//     missile, or NULL (e.g., slime, environmental hazards).
//
// "source":
//     The thing that "target" will chase after taking damage.
//     This can be a creature or NULL.
//
// Notes:
//     - For melee attacks, "source" and "inflictor" are the same.
//     - "source" can be NULL for environmental damage, such as slime or
//       barrel explosions.
//
void P_DamageMobj(mobj_t* target, const mobj_t* inflictor, mobj_t* source,
                  int damage)
{
    if (!(target->flags & MF_SHOOTABLE)) {
        // shouldn't happen...
        return;
    }
    if (target->health <= 0) {
        return;
    }
    if (target->flags & MF_SKULLFLY) {
        // Vanilla Bug: this code clears the lost soul momentum, but it does not
        // clear its damage push, causing the lost soul to drift slowly away
        // from the direction of the attack while remaining rather helplessly
        // in its own attack state.
        // More info on this bug here:
        // https://doomwiki.org/wiki/Lost_soul_charging_backwards
        target->momx = 0;
        target->momy = 0;
        target->momz = 0;
    }

    damage = P_AdjustDamage(target, damage);
    if (P_CanPushMobj(target, inflictor, source)) {
        P_ApplyDamagePush(target, inflictor, damage);
    }
    damage = P_ApplyE1M8Hack(target, damage);
    if (P_CanDamage(target, damage)) {
        if (target->player) {
            damage = P_UseArmor(target->player, damage);
            P_DamagePlayer(target->player, source, damage);
        }
        P_DamageThing(target, source, damage);
    }
}

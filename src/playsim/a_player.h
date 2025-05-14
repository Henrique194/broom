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
//
//


#ifndef __P_ACTION__
#define __P_ACTION__


#include "d_player.h"
#include "p_pspr.h"

void A_BFGsound(player_t* player, pspdef_t* psp);
void A_BFGSpray(mobj_t* mo);
void A_CheckReload(player_t* player, pspdef_t* psp);
void A_FireBFG(player_t* player, pspdef_t* psp);
void A_FireCGun(player_t* player, pspdef_t* psp);
void A_FireMissile(player_t* player, pspdef_t* psp);
void A_FirePistol(player_t* player, pspdef_t* psp);
void A_FirePlasma(player_t* player, pspdef_t* psp);
void A_FireShotgun(player_t* player, pspdef_t* psp);
void A_FireShotgun2(player_t* player, pspdef_t* psp);
void A_OpenShotgun2(player_t* player, pspdef_t* psp);
void A_LoadShotgun2(player_t* player, pspdef_t* psp);
void A_GunFlash(player_t* player, pspdef_t* psp);
void A_Light0(player_t* player, pspdef_t* psp);
void A_Light1(player_t* player, pspdef_t* psp);
void A_Light2(player_t* player, pspdef_t* psp);
void A_Lower(player_t* player, pspdef_t* psp);
void A_Punch(player_t* player, pspdef_t* psp);
void A_Raise(player_t* player, pspdef_t* psp);
void A_Saw(player_t* player, pspdef_t* psp);
void A_WeaponReady(player_t* player, pspdef_t* psp);
void A_PlayerScream(mobj_t* mo);


#endif

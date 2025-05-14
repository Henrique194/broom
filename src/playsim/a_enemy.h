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


#ifndef __P_ENEMY__
#define __P_ENEMY__


#include "d_player.h"
#include "p_mobj.h"

void A_KeenDie(mobj_t* actor);
void A_Look(mobj_t* actor);
void A_Chase(mobj_t* actor);
void A_FaceTarget(mobj_t* actor);
void A_PosAttack(mobj_t* actor);
void A_SPosAttack(mobj_t* actor);
void A_CPosAttack(mobj_t* actor);
void A_CPosRefire(mobj_t* actor);
void A_SpidRefire(mobj_t* actor);
void A_BspiAttack(mobj_t* actor);
void A_TroopAttack(mobj_t* actor);
void A_SargAttack(mobj_t* actor);
void A_HeadAttack(mobj_t* actor);
void A_CyberAttack(mobj_t* actor);
void A_BruisAttack(mobj_t* actor);
void A_SkelMissile(mobj_t* actor);
void A_Tracer(mobj_t* actor);
void A_SkelWhoosh(mobj_t* actor);
void A_SkelFist(mobj_t* actor);
void A_VileChase(mobj_t* actor);
void A_VileStart(mobj_t* actor);
void A_StartFire(mobj_t* actor);
void A_FireCrackle(mobj_t* actor);
void A_Fire(mobj_t* actor);
void A_VileTarget(mobj_t* actor);
void A_VileAttack(mobj_t* actor);
void A_FatRaise(mobj_t* actor);
void A_FatAttack1(mobj_t* actor);
void A_FatAttack2(mobj_t* actor);
void A_FatAttack3(mobj_t* actor);
void A_SkullAttack(mobj_t* actor);
void A_PainAttack(mobj_t* actor);
void A_PainDie(mobj_t* actor);
void A_Scream(mobj_t* actor);
void A_XScream(mobj_t* actor);
void A_Pain(mobj_t* actor);
void A_Fall(mobj_t* actor);
void A_Explode(mobj_t* actor);
void A_BossDeath(const mobj_t* actor);
void A_Hoof(mobj_t* actor);
void A_Metal(mobj_t* actor);
void A_BabyMetal(mobj_t* actor);
void A_ReFire(player_t* player, pspdef_t* psp);
void A_CloseShotgun2(player_t* player, pspdef_t* psp);
void A_BrainAwake(const mobj_t* actor);
void A_BrainPain(mobj_t* actor);
void A_BrainScream(const mobj_t* actor);
void A_BrainExplode(const mobj_t* actor);
void A_BrainDie(const mobj_t* actor);
void A_BrainSpit(mobj_t* actor);
void A_SpawnSound(mobj_t* actor);
void A_SpawnFly(mobj_t* actor);

#endif

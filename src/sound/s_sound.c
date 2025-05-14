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


#include <stdio.h>
#include <stdlib.h>

#include "i_sound.h"
#include "i_system.h"

#include "deh_str.h"

#include "doomstat.h"

#include "sounds.h"
#include "s_sound.h"

#include "m_misc.h"
#include "m_random.h"

#include "p_local.h"
#include "w_wad.h"
#include "z_zone.h"

//
// When to clip out sounds.
// Does not fit the large outdoor areas.
//
#define S_CLIPPING_DIST (1200 * FRACUNIT)

//
// Distance tp origin when sounds should be maxed out.
// This should relate to movement clipping resolution (see BLOCKMAP handling).
// In the source code release: (160*FRACUNIT).
// Changed back to the Vanilla value of 200 (why was this changed?)
//
#define S_CLOSE_DIST (200 * FRACUNIT)

//
// The range over which sound attenuates
//
#define S_ATTENUATOR ((S_CLIPPING_DIST - S_CLOSE_DIST) >> FRACBITS)

//
// Stereo separation
//
#define S_STEREO_SWING (96 * FRACUNIT)

#define NORM_SEP 128

typedef struct
{
    // sound information (if null, channel avail.)
    sfxinfo_t* sfxinfo;

    // origin of sound
    mobj_t *origin;

    // handle of the sound being played
    int handle;

    int pitch;
} channel_t;

//
// The set of channels available
//
static channel_t *channels;

//
// Maximum volume of a sound effect.
// Internal default is max out of 0-15.
//
int sfxVolume = 8;

//
// Maximum volume of music.
//
int musicVolume = 8;

//
// Internal volume level, ranging from 0-127
//
static int snd_SfxVolume;

//
// Whether songs are mus_paused
//
static bool mus_paused;

//
// Music currently being played
//
static musicinfo_t *mus_playing = NULL;

//
// Number of channels to use
//
int snd_channels = 8;


//
// Allocating the internal channels for mixing (the maximum numer of sounds
// rendered simultaneously) within zone memory.
//
static void S_AllocChannels() {
    size_t size = snd_channels * sizeof(channel_t);
    channels = Z_Malloc((int) size, PU_STATIC, 0);

    // Free all channels for use
    for (int i = 0; i < snd_channels; i++) {
        channels[i].sfxinfo = 0;
    }
}

static void S_SetOPLDriverVer() {
    if (gameversion != exe_doom_1_666) {
        I_SetOPLDriverVer(opl_doom_1_9);
        return;
    }
    if (logical_gamemission == doom) {
        I_SetOPLDriverVer(opl_doom1_1_666);
        return;
    }
    I_SetOPLDriverVer(opl_doom2_1_666);
}

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume, allocates channel buffer, sets S_sfx lookup.
//
void S_Init(int sfx_vol, int music_vol) {
    S_SetOPLDriverVer();
    I_PrecacheSounds(S_sfx, NUMSFX);
    S_SetSfxVolume(sfx_vol);
    S_SetMusicVolume(music_vol);
    S_AllocChannels();

    // no sounds are playing, and they are not mus_paused
    mus_paused = false;

    // Note that sounds have not been cached (yet).
    for (int i = 1; i < NUMSFX; i++) {
        S_sfx[i].lumpnum = S_sfx[i].usefulness = -1;
    }

    // Doom defaults to pitch-shifting off.
    if (snd_pitchshift == -1) {
        snd_pitchshift = 0;
    }

    I_AtExit(S_Shutdown, true);
}

void S_Shutdown(void) {
    I_ShutdownSound();
    I_ShutdownMusic();
}

static void S_StopChannel(int cnum) {
    channel_t* channel = &channels[cnum];
    if (channel->sfxinfo == NULL) {
        return;
    }
    // stop the sound playing
    if (I_SoundIsPlaying(channel->handle)) {
        I_StopSound(channel->handle);
    }
    // degrade usefulness of sound data
    channel->sfxinfo->usefulness--;
    channel->sfxinfo = NULL;
    channel->origin = NULL;
}

static int S_GetLevelMusic() {
    if (gamemode == commercial) {
        return mus_runnin + gamemap - 1;
    }
    if (gameepisode < 4) {
        return mus_e1m1 + (gameepisode - 1) * 9 + gamemap - 1;
    }
    // Ultimate Doom
    switch (gamemap) {
        //         Song    -    Who?    -    Where?
        case 1:
            return mus_e3m4; // American     e4m1
        case 2:
            return mus_e3m2; // Romero       e4m2
        case 3:
            return mus_e3m3; // Shawn        e4m3
        case 4:
            return mus_e1m5; // American     e4m4
        case 5:
            return mus_e2m7; // Tim          e4m5
        case 6:
            return mus_e2m4; // Romero       e4m6
        case 7:
            return mus_e2m6; // J.Anderson   e4m7 CHIRON.WAD
        case 8:
            return mus_e2m5; // Shawn        e4m8
        case 9:
            return mus_e1m9; // Tim          e4m9
        default:
            I_Error("S_GetLevelMusic: Could not find music for map %d\n",
                    gamemap);
    }
}

//
// Per level startup code.
// Kills playing sounds at start of level, determines music if any, changes music.
//
void S_Start() {
    // Kill all playing sounds at start of level (trust me - a good idea).
    for (int cnum = 0; cnum < snd_channels; cnum++) {
        if (channels[cnum].sfxinfo) {
            S_StopChannel(cnum);
        }
    }

    // Start new music for the level.
    mus_paused = false;
    S_ChangeMusic(S_GetLevelMusic(), true);
}

void S_StopSound(const mobj_t *origin) {
    for (int cnum = 0; cnum < snd_channels; cnum++) {
        const channel_t* channel = &channels[cnum];
        if (channel->sfxinfo && channel->origin == origin) {
            S_StopChannel(cnum);
            break;
        }
    }
}

//
// S_GetChannel :
//   If none available, return -1.  Otherwise channel #.
//

static int S_GetChannel(mobj_t *origin, sfxinfo_t *sfxinfo)
{
    // channel number to use
    int                cnum;

    channel_t*        c;

    // Find an open channel
    for (cnum=0 ; cnum<snd_channels ; cnum++)
    {
        if (!channels[cnum].sfxinfo)
        {
            break;
        }
        else if (origin && channels[cnum].origin == origin)
        {
            S_StopChannel(cnum);
            break;
        }
    }

    // None available
    if (cnum == snd_channels)
    {
        // Look for lower priority
        for (cnum=0 ; cnum<snd_channels ; cnum++)
        {
            if (channels[cnum].sfxinfo->priority >= sfxinfo->priority)
            {
                break;
            }
        }

        if (cnum == snd_channels)
        {
            // FUCK!  No lower priority.  Sorry, Charlie.
            return -1;
        }
        else
        {
            // Otherwise, kick out lower priority.
            S_StopChannel(cnum);
        }
    }

    c = &channels[cnum];

    // channel is decided to be cnum.
    c->sfxinfo = sfxinfo;
    c->origin = origin;

    return cnum;
}

static void S_AdjustVolume(fixed_t approx_dist, int* vol) {
    if (approx_dist < S_CLOSE_DIST) {
        *vol = snd_SfxVolume;
        return;
    }
    if (gamemap == 8) {
        if (approx_dist > S_CLIPPING_DIST) {
            approx_dist = S_CLIPPING_DIST;
        }
        *vol = 15 + ((snd_SfxVolume - 15) * ((S_CLIPPING_DIST - approx_dist) >> FRACBITS)) / S_ATTENUATOR;
        return;
    }
    // distance effect
    *vol = (snd_SfxVolume * ((S_CLIPPING_DIST - approx_dist) >> FRACBITS)) / S_ATTENUATOR;
}

static void S_AdjustStereoSeparation(const mobj_t *listener,
                                     const mobj_t *source, int *sep)
{
    // angle of source to listener
    angle_t angle =
        R_PointToAngle2(listener->x, listener->y, source->x, source->y);
    if (angle > listener->angle) {
        angle = angle - listener->angle;
    } else {
        angle = angle + (0xffffffff - listener->angle);
    }

    // Stereo separation.
    *sep = 128 - (FixedMul(S_STEREO_SWING, SIN(angle)) >> FRACBITS);
}


static fixed_t S_DistanceToSound(const mobj_t *listener, const mobj_t *source) {
    // calculate the distance to sound origin and clip it if necessary
    fixed_t adx = abs(listener->x - source->x);
    fixed_t ady = abs(listener->y - source->y);

    // From _GG1_ p.428. Appox. eucledian distance fast.
    fixed_t approx_dist = adx + ady - ((adx < ady ? adx : ady) >> 1);

    return approx_dist;
}

//
// Changes volume and stereo-separation variables from
// the norm of a sound effect to be played.
// If the sound is not audible, returns a 0.
// Otherwise, modifies parameters and returns 1.
//
static bool S_AdjustSoundParams(const mobj_t* listener, const mobj_t* source,
                                   int* vol, int* sep)
{
    fixed_t approx_dist = S_DistanceToSound(listener, source);

    if (gamemap != 8 && approx_dist > S_CLIPPING_DIST) {
        return false;
    }

    S_AdjustStereoSeparation(listener, source, sep);
    S_AdjustVolume(approx_dist, vol);

    return *vol > 0;
}

//
// clamp supplied integer to the range 0 <= x <= 255.
//
static int Clamp(int x) {
    if (x < 0) {
        return 0;
    }
    if (x > 255) {
        return 255;
    }
    return x;
}

void S_StartSound(void *origin_p, int sfx_id) {
    int sep;

    mobj_t* origin = (mobj_t *) origin_p;
    int volume = snd_SfxVolume;

    // check for bogus sound #
    if (sfx_id < 1 || sfx_id > NUMSFX) {
        I_Error("Bad sfx #: %d", sfx_id);
    }
    sfxinfo_t* sfx = &S_sfx[sfx_id];

    // Initialize sound parameters
    int pitch = NORM_PITCH;

    if (sfx->link) {
        volume += sfx->volume;
        pitch = sfx->pitch;
        if (volume < 1) {
            return;
        }
        if (volume > snd_SfxVolume) {
            volume = snd_SfxVolume;
        }
    }

    // Check to see if it is audible, and if not, modify the params
    if (origin && origin != players[consoleplayer].mo)
    {
        int rc = S_AdjustSoundParams(players[consoleplayer].mo, origin, &volume,
                                 &sep);

        if (origin->x == players[consoleplayer].mo->x &&
            origin->y == players[consoleplayer].mo->y)
        {
            sep = NORM_SEP;
        }

        if (!rc)
        {
            return;
        }
    }
    else
    {
        sep = NORM_SEP;
    }

    // hacks to vary the sfx pitches
    if (sfx_id >= sfx_sawup && sfx_id <= sfx_sawhit) {
        pitch += 8 - (M_Random() & 15);
    } else if (sfx_id != sfx_itemup && sfx_id != sfx_tink) {
        pitch += 16 - (M_Random() & 31);
    }
    pitch = Clamp(pitch);

    // kill old sound
    S_StopSound(origin);

    // try to find a channel
    int cnum = S_GetChannel(origin, sfx);
    if (cnum < 0) {
        return;
    }

    // increase the usefulness
    if (sfx->usefulness++ < 0) {
        sfx->usefulness = 1;
    }

    if (sfx->lumpnum < 0) {
        sfx->lumpnum = I_GetSfxLumpNum(sfx);
    }

    channels[cnum].pitch = pitch;
    channels[cnum].handle =
        I_StartSound(sfx, cnum, volume, sep, channels[cnum].pitch);
}

//
// Stop and resume music, during game PAUSE.
//
void S_PauseSound(void) {
    if (mus_playing && !mus_paused) {
        I_PauseSong();
        mus_paused = true;
    }
}

void S_ResumeSound(void) {
    if (mus_playing && mus_paused) {
        I_ResumeSong();
        mus_paused = false;
    }
}

static void S_UpdatePlayingSound(const mobj_t* listener, int cnum) {
    const channel_t* c = &channels[cnum];
    const sfxinfo_t* sfx = c->sfxinfo;

    // Initialize parameters.
    int volume = snd_SfxVolume;
    int sep = NORM_SEP;
    if (sfx->link) {
        volume += sfx->volume;
        if (volume < 1) {
            S_StopChannel(cnum);
            return;
        }
        if (volume > snd_SfxVolume) {
            volume = snd_SfxVolume;
        }
    }

    // Check non-local sounds for distance clipping or modify their params.
    if (c->origin && c->origin != listener) {
        bool audible =
            S_AdjustSoundParams(listener, c->origin, &volume, &sep);
        if (audible) {
            I_UpdateSoundParams(c->handle, volume, sep);
        } else {
            S_StopChannel(cnum);
        }
    }
}

//
// Updates music & sounds
//
void S_UpdateSounds(const mobj_t* listener) {
    I_UpdateSound();

    for (int cnum = 0; cnum < snd_channels; cnum++) {
        const channel_t* c = &channels[cnum];
        if (c->sfxinfo == NULL) {
            continue;
        }
        if (I_SoundIsPlaying(c->handle)) {
            S_UpdatePlayingSound(listener, cnum);
        } else {
            // If channel is allocated but sound has stopped, free it.
            S_StopChannel(cnum);
        }
    }
}

void S_SetMusicVolume(int volume) {
    if (volume < 0 || volume > 127) {
        I_Error("Attempt to set music volume at %d", volume);
    }
    I_SetMusicVolume(volume);
}

void S_SetSfxVolume(int volume) {
    if (volume < 0 || volume > 127) {
        I_Error("Attempt to set sfx volume at %d", volume);
    }
    snd_SfxVolume = volume;
}

//
// Starts some music with the music id found in sounds.h.
//
void S_StartMusic(int m_id) {
    S_ChangeMusic(m_id, false);
}

static bool S_IsUsingOplPlayback() {
    return snd_musicdevice == SNDDEVICE_ADLIB
           || snd_musicdevice == SNDDEVICE_SB;
}

static int S_GetMusicLump(const char* music_name) {
    char namebuf[9];
    M_snprintf(namebuf, sizeof(namebuf), "d_%s", DEH_String(music_name));
    return W_GetNumForName(namebuf);
}

static musicinfo_t* S_GetMusicInfo(int musicnum) {
    // The Doom IWAD file has two versions of the intro music: d_intro and d_introa.
    // The latter is used for OPL playback.
    if (musicnum == mus_intro
        && S_IsUsingOplPlayback()
        && W_CheckNumForName("D_INTROA") >= 0) {
        musicnum = mus_introa;
    }
    if (musicnum <= mus_None || musicnum >= NUMMUSIC) {
        I_Error("Bad music number %d", musicnum);
    } else {
        return &S_music[musicnum];
    }
}

void S_ChangeMusic(int musicnum, bool looping) {
    musicinfo_t* music = S_GetMusicInfo(musicnum);

    if (mus_playing == music) {
        return;
    }

    // Shutdown old music.
    S_StopMusic();

    if (!music->lumpnum) {
        music->lumpnum = S_GetMusicLump(music->name);
    }
    music->data = W_CacheLumpNum(music->lumpnum, PU_STATIC);
    music->handle = I_RegisterSong(music->data, W_LumpLength(music->lumpnum));

    I_PlaySong(music->handle, looping);

    mus_playing = music;
}

void S_StopMusic(void) {
    if (mus_playing) {
        if (mus_paused) {
            I_ResumeSong();
        }
        I_StopSong();
        I_UnRegisterSong(mus_playing->handle);
        W_ReleaseLumpNum(mus_playing->lumpnum);
        mus_playing->data = NULL;
        mus_playing = NULL;
    }
}

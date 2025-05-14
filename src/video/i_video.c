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
//	DOOM graphics stuff for SDL.
//


#include <stdlib.h>

#include "SDL.h"
#include "SDL_opengl.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include "config.h"
#include "d_loop.h"
#include "d_event.h"
#include "deh_str.h"
#include "doomtype.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "tables.h"
#include "v_icon.h"
#include "v_diskicon.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

// These are (1) the window (or the full screen) that our game is rendered to
// and (2) the renderer that scales the texture (see below) into this window.

static SDL_Window *screen;
static SDL_Renderer *renderer;

// Window title

static const char *window_title = "";

// The 320x200x8 paletted buffer that we draw to
// (i.e. the one that holds I_VideoBuffer).
static SDL_Surface *screenbuffer = NULL;

// The 320x200x32 RGBA intermediate buffer that we blit the screenbuffer to.
static SDL_Surface *argbbuffer = NULL;

// The intermediate 320x200 texture that we load the RGBA buffer to
// and that we render into texture_upscaled.
static SDL_Texture *texture = NULL;

// The texture which is upscaled by an integer factor UPSCALE using "nearest"
// scaling and which in turn is finally rendered to screen using "linear"
// scaling.
static SDL_Texture *texture_upscaled = NULL;

static SDL_Rect blit_rect = {
    0,
    0,
    SCREENWIDTH,
    SCREENHEIGHT
};

static uint32_t pixel_format;

// palette

static SDL_Color palette[256];
static bool palette_to_set;

// display has been set up?

static bool initialized = false;

// disable mouse?

static bool nomouse = false;
int usemouse = 1;

// Save screenshots in PNG format.

int png_screenshots = 0;

// SDL video driver name

char *video_driver = "";

// Window position:

char *window_position = "center";

// SDL display number on which to run.

int video_display = 0;

// Screen width and height, from configuration file.

int window_width = 800;
int window_height = 600;

// Fullscreen mode, 0x0 for SDL_WINDOW_FULLSCREEN_DESKTOP.
int fullscreen_width = 0, fullscreen_height = 0;

// Maximum number of pixels to use for intermediate scale buffer.

static int max_scaling_buffer_pixels = 16000000;

// Run in full screen mode?  (int type for config code)

int fullscreen = true;

// Aspect ratio correction mode

int aspect_ratio_correct = true;
static int actualheight;

// Force integer scales for resolution-independent rendering

int integer_scaling = false;

// VGA Porch palette change emulation
int vga_porch_flash = false;

// Force software rendering, for systems which lack effective hardware
// acceleration

int force_software_renderer = false;

// Time to wait for the screen to settle on startup before starting the
// game (ms)

static int startup_delay = 1000;

// Grab the mouse? (int type for config code). nograbmouse_override allows
// this to be temporarily disabled via the command line.

static int grabmouse = true;
static bool nograbmouse_override = false;

// The screen buffer; this is modified to draw things to the screen
pixel_t *I_VideoBuffer = NULL;

// If true, game is running as a screensaver
bool screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen
bool screenvisible = true;

// If true, we display dots at the bottom of the screen to 
// indicate FPS.
static bool display_fps_dots;

// If this is true, the screen is rendered but not blitted to the
// video buffer.
static bool noblit;

// Callback function to invoke to determine whether to grab the 
// mouse pointer.
static grabmouse_callback_t grabmouse_callback = NULL;

// Does the window currently have focus?
static bool window_focused = true;

// Window resize state.

static bool need_resize = false;
static unsigned int last_resize_time;
#define RESIZE_DELAY 500

// Gamma correction level to use

int usegamma = 0;

// Joystick/gamepad hysteresis
unsigned int joywait = 0;

static bool I_MouseShouldBeGrabbed() {
    // never grab the mouse when in screensaver mode
    if (screensaver_mode) {
        return false;
    }
    // if the window doesn't have focus, never grab it
    if (!window_focused) {
        return false;
    }
    // always grab the mouse when fullscreen (dont want to see the mouse pointer)
    if (fullscreen) {
        return true;
    }
    // Don't grab the mouse if mouse input is disabled
    if (!usemouse || nomouse) {
        return false;
    }
    // if we specify not to grab the mouse, never grab
    if (nograbmouse_override || !grabmouse) {
        return false;
    }
    // Invoke the grabmouse callback function to determine whether
    // the mouse should be grabbed
    if (grabmouse_callback) {
        return grabmouse_callback();
    }
    return true;
}

void I_SetGrabMouseCallback(grabmouse_callback_t func)
{
    grabmouse_callback = func;
}

// Set the variable controlling FPS dots.

void I_DisplayFPSDots(bool dots_on) {
    display_fps_dots = dots_on;
}

static void I_SetShowCursor(bool show) {
    if (!screensaver_mode) {
        // When the cursor is hidden, grab the input.
        // Relative mode implicitly hides the cursor.
        SDL_SetRelativeMouseMode(!show);
        SDL_GetRelativeMouseState(NULL, NULL);
    }
}

void I_ShutdownGraphics(void)
{
    if (initialized)
    {
        I_SetShowCursor(true);

        SDL_QuitSubSystem(SDL_INIT_VIDEO);

        initialized = false;
    }
}



//
// Adjust window_width / window_height variables to be an aspect ratio
// consistent with the aspect_ratio_correct variable.
//
static void I_AdjustWindowSize(void) {
    if (aspect_ratio_correct || integer_scaling) {
        if (window_width * actualheight <= window_height * SCREENWIDTH) {
            // We round up window_height if the ratio is not exact;
            // this leaves the result stable.
            window_height = (window_width * actualheight + SCREENWIDTH - 1) / SCREENWIDTH;
        } else {
            window_width = window_height * SCREENWIDTH / actualheight;
        }
    }
}

static void I_HandleWindowEvent(SDL_WindowEvent* event) {
    switch (event->event) {
        case SDL_WINDOWEVENT_EXPOSED:
            palette_to_set = true;
            break;
        case SDL_WINDOWEVENT_RESIZED:
            need_resize = true;
            last_resize_time = SDL_GetTicks();
            break;
        case SDL_WINDOWEVENT_MINIMIZED:
            // Don't render the screen when the window is minimized
            screenvisible = false;
            break;
        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
            screenvisible = true;
            break;
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            // Update the value of window_focused when we get a focus event.
            //
            // We try to make ourselves be well-behaved: the grab on the mouse
            // is removed if we lose focus (such as a popup window appearing),
            // and we dont move the mouse around if we aren't focused either.
            window_focused = true;
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
            window_focused = false;
            break;
        case SDL_WINDOWEVENT_MOVED:
            // We want to save the user's preferred monitor to use for running the
            // game, so that next time we're run we start on the same display. So
            // every time the window is moved, find which display we're now on and
            // update the video_display config variable.
            int i = SDL_GetWindowDisplayIndex(screen);
            if (i >= 0) {
                video_display = i;
            }
            break;
        default:
            break;
    }
}

static bool ToggleFullScreenKeyShortcut(SDL_Keysym *sym) {
    Uint16 flags = (KMOD_LALT | KMOD_RALT);
#if defined(__MACOSX__)
    flags |= (KMOD_LGUI | KMOD_RGUI);
#endif
    return (sym->scancode == SDL_SCANCODE_RETURN || 
            sym->scancode == SDL_SCANCODE_KP_ENTER) && (sym->mod & flags) != 0;
}

static void I_ToggleFullScreen(void) {
    unsigned int flags = 0;

    // TODO: Consider implementing fullscreen toggle for SDL_WINDOW_FULLSCREEN
    // (mode-changing) setup. This is hard because we have to shut down and
    // restart again.
    if (fullscreen_width != 0 || fullscreen_height != 0) {
        return;
    }

    fullscreen = !fullscreen;

    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    SDL_SetWindowFullscreen(screen, flags);

    if (!fullscreen) {
        I_AdjustWindowSize();
        SDL_SetWindowSize(screen, window_width, window_height);
    }
}

static void I_DoQuit() {
    if (screensaver_mode) {
        I_Quit();
        return;
    }
    event_t event;
    event.type = ev_quit;
    D_PostEvent(&event);
}

static void I_HandleKeyDownEvent(SDL_Event* event) {
    if (ToggleFullScreenKeyShortcut(&event->key.keysym)) {
        I_ToggleFullScreen();
        return;
    }
    I_HandleKeyboardEvent(event);
}

static void I_HandleEvent(SDL_Event* event) {
    switch (event->type) {
        case SDL_KEYDOWN:
            I_HandleKeyDownEvent(event);
            break;
        case SDL_KEYUP:
            I_HandleKeyboardEvent(event);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
            if (usemouse && !nomouse && window_focused) {
                I_HandleMouseEvent(event);
            }
            break;
        case SDL_QUIT:
            I_DoQuit();
            break;
        case SDL_WINDOWEVENT:
            if (event->window.windowID == SDL_GetWindowID(screen)) {
                I_HandleWindowEvent(&event->window);
            }
            break;
        default:
            break;
    }
}

static void I_GetEvent(void) {
    SDL_PumpEvents();

    SDL_Event sdlevent;
    while (SDL_PollEvent(&sdlevent)) {
        I_HandleEvent(&sdlevent);
    }
}

//
// I_StartTic
//
void I_StartTic(void) {
    if (!initialized) {
        return;
    }
    I_GetEvent();
    if (usemouse && !nomouse && window_focused) {
        I_ReadMouse();
    }
    if (joywait < I_GetTime()) {
        I_UpdateJoystick();
    }
}


//
// When releasing the mouse from grab, warp the mouse cursor to
// the bottom-right of the screen. This is a minimally distracting
// place for it to appear - we may only have released the grab
// because we're at an end of level intermission screen, for
// example.
//
static void I_WrapMouseCursor() {
    int screen_w, screen_h;
    SDL_GetWindowSize(screen, &screen_w, &screen_h);
    SDL_WarpMouseInWindow(screen, screen_w - 16, screen_h - 16);
    SDL_GetRelativeMouseState(NULL, NULL);
}

static bool I_ShowCursor(bool grab, bool currently_grabbed) {
    // Hide the cursor in screensaver mode
    if (screensaver_mode) {
        return false;
    }
    if (grab && !currently_grabbed) {
        return false;
    }
    if (!grab && currently_grabbed) {
        return true;
    }
    return false;
}

static void I_UpdateCursorGrab(void) {
    static bool currently_grabbed = false;

    bool grab = I_MouseShouldBeGrabbed();
    bool show_cursor = I_ShowCursor(grab, currently_grabbed);
    I_SetShowCursor(show_cursor);
    if (show_cursor) {
        I_WrapMouseCursor();
    }

    currently_grabbed = grab;
}

static void LimitTextureSize(int *w_upscale, int *h_upscale)
{
    SDL_RendererInfo rinfo;
    int orig_w, orig_h;

    orig_w = *w_upscale;
    orig_h = *h_upscale;

    // Query renderer and limit to maximum texture dimensions of hardware:
    if (SDL_GetRendererInfo(renderer, &rinfo) != 0)
    {
        I_Error("I_CreateUpscaledTexture: SDL_GetRendererInfo() call failed: %s",
                SDL_GetError());
    }

    while (*w_upscale * SCREENWIDTH > rinfo.max_texture_width)
    {
        --*w_upscale;
    }
    while (*h_upscale * SCREENHEIGHT > rinfo.max_texture_height)
    {
        --*h_upscale;
    }

    if ((*w_upscale < 1 && rinfo.max_texture_width > 0) ||
        (*h_upscale < 1 && rinfo.max_texture_height > 0))
    {
        I_Error("I_CreateUpscaledTexture: Can't create a texture big enough for "
                "the whole screen! Maximum texture size %dx%d",
                rinfo.max_texture_width, rinfo.max_texture_height);
    }

    // We limit the amount of texture memory used for the intermediate buffer,
    // since beyond a certain point there are diminishing returns. Also,
    // depending on the hardware there may be performance problems with very
    // huge textures, so the user can use this to reduce the maximum texture
    // size if desired.

    if (max_scaling_buffer_pixels < SCREENWIDTH * SCREENHEIGHT)
    {
        I_Error("I_CreateUpscaledTexture: max_scaling_buffer_pixels too small "
                "to create a texture buffer: %d < %d",
                max_scaling_buffer_pixels, SCREENWIDTH * SCREENHEIGHT);
    }

    while (*w_upscale * *h_upscale * SCREENWIDTH * SCREENHEIGHT
           > max_scaling_buffer_pixels)
    {
        if (*w_upscale > *h_upscale)
        {
            --*w_upscale;
        }
        else
        {
            --*h_upscale;
        }
    }

    if (*w_upscale != orig_w || *h_upscale != orig_h)
    {
        printf("I_CreateUpscaledTexture: Limited texture size to %dx%d "
               "(max %d pixels, max texture size %dx%d)\n",
               *w_upscale * SCREENWIDTH, *h_upscale * SCREENHEIGHT,
               max_scaling_buffer_pixels,
               rinfo.max_texture_width, rinfo.max_texture_height);
    }
}

static void I_GetUpscaleFactors(int* w_upscale, int* h_upscale) {
    int w = 0;
    int h = 0;

    // Get the size of the renderer output. The units this gives us will be
    // real world pixels, which are not necessarily equivalent to the screen's
    // window size (because of highdpi).
    if (SDL_GetRendererOutputSize(renderer, &w, &h) != 0) {
        I_Error("Failed to get renderer output size: %s", SDL_GetError());
    }

    // When the screen or window dimensions do not match the aspect ratio
    // of the texture, the rendered area is scaled down to fit. Calculate
    // the actual dimensions of the rendered area.
    if (w * actualheight < h * SCREENWIDTH) {
        // Tall window.
        h = w * actualheight / SCREENWIDTH;
    } else {
        // Wide window.
        w = h * SCREENWIDTH / actualheight;
    }

    // Pick texture size the next integer multiple of the screen dimensions.
    // If one screen dimension matches an integer multiple of the original
    // resolution, there is no need to overscale in this direction.
    *w_upscale = (w + SCREENWIDTH - 1) / SCREENWIDTH;
    *h_upscale = (h + SCREENHEIGHT - 1) / SCREENHEIGHT;

    // Minimum texture dimensions of 320x200.
    if (*w_upscale < 1) {
        *w_upscale = 1;
    }
    if (*h_upscale < 1) {
        *h_upscale = 1;
    }

    LimitTextureSize(w_upscale, h_upscale);
}

static void I_CreateUpscaledTexture(bool force) {
    static int w_upscale_old = 0;
    static int h_upscale_old = 0;

    int w_upscale = 0;
    int h_upscale = 0;
    I_GetUpscaleFactors(&w_upscale, &h_upscale);

    // Create a new texture only if the upscale factors have actually changed.
    if (h_upscale == h_upscale_old && w_upscale == w_upscale_old && !force) {
        return;
    }
    h_upscale_old = h_upscale;
    w_upscale_old = w_upscale;

    // Set the scaling quality for rendering the upscaled texture to "linear",
    // which looks much softer and smoother than "nearest" but does a better
    // job at downscaling from the upscaled texture to screen.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    SDL_Texture* new_texture = SDL_CreateTexture(renderer,
                                pixel_format,
                                SDL_TEXTUREACCESS_TARGET,
                                w_upscale*SCREENWIDTH,
                                h_upscale*SCREENHEIGHT);

    SDL_Texture* old_texture = texture_upscaled;
    texture_upscaled = new_texture;

    if (old_texture != NULL) {
        SDL_DestroyTexture(old_texture);
    }
}

static void I_UpdateScreen() {
    // Blit from the paletted 8-bit screen buffer to the intermediate
    // 32-bit RGBA buffer that we can load into the texture.
    SDL_LowerBlit(screenbuffer, &blit_rect, argbbuffer, &blit_rect);

    // Update the intermediate texture with the contents of the RGBA buffer.
    SDL_UpdateTexture(texture, NULL, argbbuffer->pixels, argbbuffer->pitch);

    // Make sure the pillarboxes are kept clear each frame.
    SDL_RenderClear(renderer);

    // Render this intermediate texture into the upscaled texture
    // using "nearest" integer scaling.
    SDL_SetRenderTarget(renderer, texture_upscaled);
    SDL_RenderCopy(renderer, texture, NULL, NULL);

    // Finally, render this upscaled texture to screen using linear scaling.
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderCopy(renderer, texture_upscaled, NULL, NULL);

    // Draw!
    SDL_RenderPresent(renderer);
}

static void I_UpdatePalette() {
    SDL_Palette* sdl_palette = screenbuffer->format->palette;
    SDL_SetPaletteColors(sdl_palette, palette, 0, 256);
    palette_to_set = false;

    if (vga_porch_flash) {
        // "flash" the pillars/letterboxes with palette changes, emulating
        // VGA "porch" behaviour (GitHub issue #832)
        Uint8 r = palette[0].r;
        Uint8 g = palette[0].g;
        Uint8 b = palette[0].b;
        Uint8 a = SDL_ALPHA_OPAQUE;
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
    }
}

//
// Draws little dots on the bottom of the screen.
//
static void I_DrawFpsDots() {
    static int lasttic;

    int i = I_GetTime();
    lasttic = i;
    int tics = i - lasttic;
    if (tics > 20) {
        tics = 20;
    }

    int y = SCREENHEIGHT - 1;

    for (i = 0 ; i < tics * 4; i+=4) {
        int screen_spot = i + (y * SCREENWIDTH);
        I_VideoBuffer[screen_spot] = 0xff;
    }
    for ( ; i < 20 * 4; i+=4) {
        int screen_spot = i + (y * SCREENWIDTH);
        I_VideoBuffer[screen_spot] = 0x0;
    }
}

static void I_ResizeWindow() {
    // When the window is resized (we're not in fullscreen mode),
    // save the new window size.
    int flags = (int) SDL_GetWindowFlags(screen);

    if ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == 0) {
        SDL_GetWindowSize(screen, &window_width, &window_height);

        // Adjust the window by resizing again so that the window
        // is the right aspect ratio.
        I_AdjustWindowSize();
        SDL_SetWindowSize(screen, window_width, window_height);
    }

    I_CreateUpscaledTexture(false);
    need_resize = false;
    palette_to_set = true;
}

static bool I_IsReadyResizeWindow() {
    return SDL_GetTicks() > last_resize_time + RESIZE_DELAY;
}

static bool I_CanUpdateScreen() {
    if (!initialized || noblit) {
        return false;
    }
    if (need_resize && !I_IsReadyResizeWindow()) {
        return false;
    }
    return true;
}

//
// I_FinishUpdate
//
void I_FinishUpdate() {
    if (!I_CanUpdateScreen()) {
        return;
    }
    if (need_resize) {
        I_ResizeWindow();
    }
    I_UpdateCursorGrab();
    if (display_fps_dots) {
        I_DrawFpsDots();
    }
    // Draw disk icon before blit, if necessary.
    V_DrawDiskIcon();
    if (palette_to_set) {
        I_UpdatePalette();
    }
    I_UpdateScreen();
    // Restore background and undo the disk indicator, if it was drawn.
    V_RestoreDiskBackground();
}


//
// I_ReadScreen
//
void I_ReadScreen(pixel_t* scr) {
    memcpy(scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT * sizeof(*scr));
}


//
// I_SetPalette
//
void I_SetPalette(const byte* doompalette) {
    const byte* gamma = gammatable[usegamma];

    for (int i = 0; i < 256; i++) {
        byte r = doompalette[i * 3];
        byte g = doompalette[(i * 3) + 1];
        byte b = doompalette[(i * 3) + 2];

        // Apply gamma correction.
        r = gamma[r];
        g = gamma[g];
        b = gamma[b];

        // Zero out the bottom two bits of each channel - the PC VGA
        // controller only supports 6 bits of accuracy.
        palette[i].r = r & ~3;
        palette[i].g = g & ~3;
        palette[i].b = b & ~3;
    }

    palette_to_set = true;
}

//
// Given an RGB value, find the closest matching palette index.
//
int I_GetPaletteIndex(int r, int g, int b) {
    int best = 0;
    int best_diff = INT_MAX;

    for (int i = 0; i < 256; ++i) {
        int diff = (r - palette[i].r) * (r - palette[i].r) +
                   (g - palette[i].g) * (g - palette[i].g) +
                   (b - palette[i].b) * (b - palette[i].b);

        if (diff < best_diff) {
            best = i;
            best_diff = diff;
        }
        if (diff == 0) {
            break;
        }
    }

    return best;
}

// 
// Set the window title
//
void I_SetWindowTitle(const char *title) {
    window_title = title;
}

//
// Call the SDL function to set the window title, based on 
// the title set with I_SetWindowTitle.
//
static void I_InitWindowTitle() {
    char* buf = M_StringJoin(window_title, " - ", PACKAGE_STRING, NULL);
    SDL_SetWindowTitle(screen, buf);
    free(buf);
}

//
// Set the application icon
//
void I_InitWindowIcon() {
    int depth = 32;
    int pitch = icon_w * 4;
    Uint32 r_mask = 0xffu << 24;
    Uint32 g_mask = 0xffu << 16;
    Uint32 b_mask = 0xffu << 8;
    Uint32 a_mask = 0xffu << 0;
    SDL_Surface* surface =
        SDL_CreateRGBSurfaceFrom(icon_data, icon_w, icon_h, depth, pitch,
                                 r_mask, g_mask, b_mask, a_mask);

    SDL_SetWindowIcon(screen, surface);
    SDL_FreeSurface(surface);
}

//
// Set video size to a particular scale factor (1x, 2x, 3x, etc.)
//
static void SetScaleFactor(int factor)
{
    // Pick 320x200 or 320x240, depending on aspect ratio correct

    window_width = factor * SCREENWIDTH;
    window_height = factor * actualheight;
    fullscreen = false;
}

void I_GraphicsCheckCommandLine(void)
{
    int i;

    //!
    // @category video
    // @vanilla
    //
    // Disable blitting the screen.
    //

    noblit = M_CheckParm ("-noblit");

    //!
    // @category video 
    //
    // Don't grab the mouse when running in windowed mode.
    //

    nograbmouse_override = M_ParmExists("-nograbmouse");

    // default to fullscreen mode, allow override with command line
    // nofullscreen because we love prboom

    //!
    // @category video 
    //
    // Run in a window.
    //

    if (M_CheckParm("-window") || M_CheckParm("-nofullscreen"))
    {
        fullscreen = false;
    }

    //!
    // @category video 
    //
    // Run in fullscreen mode.
    //

    if (M_CheckParm("-fullscreen"))
    {
        fullscreen = true;
    }

    //!
    // @category video 
    //
    // Disable the mouse.
    //

    nomouse = M_CheckParm("-nomouse") > 0;

    //!
    // @category video
    // @arg <x>
    //
    // Specify the screen width, in pixels. Implies -window.
    //

    i = M_CheckParmWithArgs("-width", 1);

    if (i > 0)
    {
        window_width = atoi(myargv[i + 1]);
        fullscreen = false;
    }

    //!
    // @category video
    // @arg <y>
    //
    // Specify the screen height, in pixels. Implies -window.
    //

    i = M_CheckParmWithArgs("-height", 1);

    if (i > 0)
    {
        window_height = atoi(myargv[i + 1]);
        fullscreen = false;
    }

    //!
    // @category video
    // @arg <WxY>
    //
    // Specify the dimensions of the window. Implies -window.
    //

    i = M_CheckParmWithArgs("-geometry", 1);

    if (i > 0)
    {
        int w, h, s;

        s = sscanf(myargv[i + 1], "%ix%i", &w, &h);
        if (s == 2)
        {
            window_width = w;
            window_height = h;
            fullscreen = false;
        }
    }

    //!
    // @category video
    // @arg <x>
    //
    // Specify the display number on which to show the screen.
    //

    i = M_CheckParmWithArgs("-display", 1);

    if (i > 0)
    {
        int display = atoi(myargv[i + 1]);
        if (display >= 0)
        {
            video_display = display;
        }
    }

    //!
    // @category video
    //
    // Don't scale up the screen. Implies -window.
    //

    if (M_CheckParm("-1")) 
    {
        SetScaleFactor(1);
    }

    //!
    // @category video
    //
    // Double up the screen to 2x its normal size. Implies -window.
    //

    if (M_CheckParm("-2")) 
    {
        SetScaleFactor(2);
    }

    //!
    // @category video
    //
    // Double up the screen to 3x its normal size. Implies -window.
    //

    if (M_CheckParm("-3")) 
    {
        SetScaleFactor(3);
    }
}

// Check if we have been invoked as a screensaver by xscreensaver.

void I_CheckIsScreensaver(void)
{
    char *env;

    env = getenv("XSCREENSAVER_WINDOW");

    if (env != NULL)
    {
        screensaver_mode = true;
    }
}

static void I_SetSDLVideoDriver(void) {
    // Allow a default value for the SDL video driver to be specified
    // in the configuration file.
    if (strcmp(video_driver, "") != 0) {
        char *env_string;

        env_string = M_StringJoin("SDL_VIDEODRIVER=", video_driver, NULL);
        putenv(env_string);
        free(env_string);
    }
}

// Check the display bounds of the display referred to by 'video_display' and
// set x and y to a location that places the window in the center of that
// display.
static void CenterWindow(int *x, int *y, int w, int h)
{
    SDL_Rect bounds;

    if (SDL_GetDisplayBounds(video_display, &bounds) < 0)
    {
        fprintf(stderr, "CenterWindow: Failed to read display bounds "
                        "for display #%d!\n", video_display);
        return;
    }

    *x = bounds.x + SDL_max((bounds.w - w) / 2, 0);
    *y = bounds.y + SDL_max((bounds.h - h) / 2, 0);
}

void I_GetWindowPosition(int *x, int *y, int w, int h)
{
    // Check that video_display corresponds to a display that really exists,
    // and if it doesn't, reset it.
    if (video_display < 0 || video_display >= SDL_GetNumVideoDisplays())
    {
        fprintf(stderr,
                "I_GetWindowPosition: We were configured to run on display #%d, "
                "but it no longer exists (max %d). Moving to display 0.\n",
                video_display, SDL_GetNumVideoDisplays() - 1);
        video_display = 0;
    }

    // in fullscreen mode, the window "position" still matters, because
    // we use it to control which display we run fullscreen on.

    if (fullscreen)
    {
        CenterWindow(x, y, w, h);
        return;
    }

    // in windowed mode, the desired window position can be specified
    // in the configuration file.

    if (window_position == NULL || !strcmp(window_position, ""))
    {
        *x = *y = SDL_WINDOWPOS_UNDEFINED;
    }
    else if (!strcmp(window_position, "center"))
    {
        // Note: SDL has a SDL_WINDOWPOS_CENTER, but this is useless for our
        // purposes, since we also want to control which display we appear on.
        // So we have to do this ourselves.
        CenterWindow(x, y, w, h);
    }
    else if (sscanf(window_position, "%i,%i", x, y) != 2)
    {
        // invalid format: revert to default
        fprintf(stderr, "I_GetWindowPosition: invalid window_position setting\n");
        *x = *y = SDL_WINDOWPOS_UNDEFINED;
    }
}

static void I_SetDoomPalette() {
    const char* lump_name = DEH_String("PLAYPAL");
    const byte* doompal = W_CacheLumpName(lump_name, PU_CACHE);
    I_SetPalette(doompal);
    SDL_SetPaletteColors(screenbuffer->format->palette, palette, 0, 256);
}

#if defined(_WIN32)
//
// Workaround for SDL 2.0.14+ alt-tab bug
// (taken from Doom Retro via Prboom-plus and Woof).
//
static void I_WorkaroundAltTabBug() {
    SDL_version ver;
    SDL_GetVersion(&ver);
    if (ver.major == 2 && ver.minor == 0 && (ver.patch == 14 || ver.patch == 16)) {
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "1", SDL_HINT_OVERRIDE);
    }
}
#endif

static void I_CreateTexture() {
    if (texture != NULL) {
        SDL_DestroyTexture(texture);
    }

    // Set the scaling quality for rendering the intermediate texture into
    // the upscaled texture to "nearest", which is gritty and pixelated and
    // resembles software scaling pretty well.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    // Create the intermediate texture that the RGBA surface gets loaded into.
    // The SDL_TEXTUREACCESS_STREAMING flag means that this texture's content
    // is going to change frequently.
    int access = SDL_TEXTUREACCESS_STREAMING;
    int w = SCREENWIDTH;
    int h = SCREENHEIGHT;
    texture = SDL_CreateTexture(renderer, pixel_format, access, w, h);
}

//
// Create the 32-bit RGBA surface. Format of argbbuffer must match the
// screen pixel format because we import the surface data into the texture.
//
static void I_Create32BitSurface() {
    if (argbbuffer != NULL) {
        SDL_FreeSurface(argbbuffer);
        argbbuffer = NULL;
    }

    if (argbbuffer == NULL) {
        unsigned int rmask, gmask, bmask, amask;
        int bpp;
        int w = SCREENWIDTH;
        int h = SCREENHEIGHT;
        SDL_PixelFormatEnumToMasks(pixel_format, &bpp, &rmask, &gmask, &bmask, &amask);

        argbbuffer = SDL_CreateRGBSurface(0, w, h, bpp, rmask, gmask, bmask, amask);
        SDL_FillRect(argbbuffer, NULL, 0);
    }
}

//
// Create the 8-bit paletted surface.
//
static void I_Create8BitSurface() {
    if (screenbuffer != NULL) {
        SDL_FreeSurface(screenbuffer);
        screenbuffer = NULL;
    }

    if (screenbuffer == NULL) {
        Uint32 flags = 0;
        int w = SCREENWIDTH;
        int h = SCREENHEIGHT;
        int depth = 8;
        Uint32 r_mask = 0;
        Uint32 g_mask = 0;
        Uint32 b_mask = 0;
        Uint32 a_mask = 0;

        screenbuffer = SDL_CreateRGBSurface(flags, w, h, depth, r_mask,
                                            g_mask, b_mask, a_mask);
        SDL_FillRect(screenbuffer, NULL, 0);
    }
}

static int I_GetRenderFlags() {
    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(video_display, &mode) != 0) {
        I_Error("Could not get display mode for video display #%d: %s", video_display, SDL_GetError());
    }

    // The SDL_RENDERER_TARGETTEXTURE flag is required to render the
    // intermediate texture into the upscaled texture.
    int renderer_flags = SDL_RENDERER_TARGETTEXTURE;

    // Turn on vsync if we aren't in a -timedemo
    if (!singletics && mode.refresh_rate > 0) {
        renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    }
    if (force_software_renderer) {
        renderer_flags |= SDL_RENDERER_SOFTWARE;
        renderer_flags &= ~SDL_RENDERER_PRESENTVSYNC;
    }

    return renderer_flags;
}

static void I_CreateRender() {
    if (renderer != NULL) {
        SDL_DestroyRenderer(renderer);
        // all associated textures get destroyed
        texture = NULL;
        texture_upscaled = NULL;
    }

    int flags = I_GetRenderFlags();
    renderer = SDL_CreateRenderer(screen, -1, flags);

    // If we could not find a matching render driver,
    // try again without hardware acceleration.
    if (renderer == NULL && !force_software_renderer) {
        flags |= SDL_RENDERER_SOFTWARE;
        flags &= ~SDL_RENDERER_PRESENTVSYNC;

        renderer = SDL_CreateRenderer(screen, -1, flags);

        // If this helped, save the setting for later.
        if (renderer != NULL) {
            force_software_renderer = 1;
        }
    }
    if (renderer == NULL) {
        I_Error("Error creating renderer for screen window: %s", SDL_GetError());
    }

    // Important: Set the "logical size" of the rendering context. At the same
    // time this also defines the aspect ratio that is preserved while scaling
    // and stretching the texture into the window.
    if (aspect_ratio_correct || integer_scaling) {
        SDL_RenderSetLogicalSize(renderer, SCREENWIDTH, actualheight);
    }

    // Force integer scales for resolution-independent rendering.
    SDL_RenderSetIntegerScale(renderer, integer_scaling);

    // Blank out the full screen area in case there is any junk in
    // the borders that won't otherwise be overwritten.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

static int I_GetWindowsFlags() {
    // In windowed mode, the window can be resized while the game is running.
    int window_flags = SDL_WINDOW_RESIZABLE;

    // Set the highdpi flag - this makes a big difference on Macs with
    // retina displays, especially when using small window sizes.
    window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;

    if (fullscreen) {
        if (fullscreen_width == 0 && fullscreen_height == 0) {
            // This window_flags means "Never change the screen resolution!
            // Instead, draw to the entire screen by scaling the texture
            // appropriately".
            window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        } else {
            window_flags |= SDL_WINDOW_FULLSCREEN;
        }
    }

    // Running without window decorations is potentially useful if you're
    // playing in three window mode and want to line up three game windows
    // next to each other on a single desktop.
    // Deliberately not documented because I'm not sure how useful this is yet.
    if (M_ParmExists("-borderless")) {
        window_flags |= SDL_WINDOW_BORDERLESS;
    }

    return window_flags;
}

static void I_GetWindowDimension(int* w, int* h) {
    if (fullscreen) {
        *w = fullscreen_width;
        *h = fullscreen_height;
    } else {
        *w = window_width;
        *h = window_height;
    }
}

//
// Create window and renderer contexts. We set the window title
// later anyway and leave the window position "undefined". If
// "window_flags" contains the fullscreen flag (see above), then
// w and h are ignored.
//
static void I_CreateWindow() {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    int flags = I_GetWindowsFlags();
    I_GetWindowDimension(&w, &h);
    I_GetWindowPosition(&x, &y, w, h);

    screen = SDL_CreateWindow(NULL, x, y, w, h, flags);
    if (screen == NULL) {
        I_Error("Error creating window for video startup: %s", SDL_GetError());
    }
    pixel_format = SDL_GetWindowPixelFormat(screen);
    SDL_SetWindowMinimumSize(screen, SCREENWIDTH, actualheight);

    I_InitWindowTitle();
    I_InitWindowIcon();
}

static void I_SetVideoMode(void) {
    if (screen == NULL) {
        I_CreateWindow();
    }
    I_CreateRender();
    I_Create8BitSurface();
    I_Create32BitSurface();
    I_CreateTexture();
#if defined(_WIN32)
    I_WorkaroundAltTabBug();
#endif
    // Initially create the upscaled texture for rendering to screen
    I_CreateUpscaledTexture(true);
}

//
// Pass through the XSCREENSAVER_WINDOW environment variable to
// SDL_WINDOWID, to embed the SDL window into the Xscreensaver
// window.
//
static void I_EmbedIntoXScreenSaverWindow(char *env) {
    char winenv[30];
    unsigned int winid;

    sscanf(env, "0x%x", &winid);
    M_snprintf(winenv, sizeof(winenv), "SDL_WINDOWID=%u", winid);

    putenv(winenv);
}

void I_InitGraphics(void) {
    SDL_Event dummy;

    char *env = getenv("XSCREENSAVER_WINDOW");
    if (env) {
        I_EmbedIntoXScreenSaverWindow(env);
    }

    I_SetSDLVideoDriver();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        I_Error("Failed to initialize video: %s", SDL_GetError());
    }

    // When in screensaver mode, run full screen and auto detect
    // screen dimensions (don't change video mode)
    if (screensaver_mode) {
        fullscreen = true;
    }

    if (aspect_ratio_correct == 1) {
        actualheight = SCREENHEIGHT_4_3;
    } else {
        actualheight = SCREENHEIGHT;
    }

    // Create the game window; this may switch graphic modes depending
    // on configuration.
    I_AdjustWindowSize();
    I_SetVideoMode();

    // Start with a clear black screen
    // (screen will be flipped after we set the palette)
    SDL_FillRect(screenbuffer, NULL, 0);

    // Set the palette
    I_SetDoomPalette();

    I_UpdateCursorGrab();

    // On some systems, it takes a second or so for the screen to settle
    // after changing modes.  We include the option to add a delay when
    // setting the screen mode, so that the game doesn't start immediately
    // with the player unable to see anything.
    if (fullscreen && !screensaver_mode) {
        SDL_Delay(startup_delay);
    }

    // The actual 320x200 canvas that we draw to. This is the pixel buffer of
    // the 8-bit paletted screen buffer that gets blit on an intermediate
    // 32-bit RGBA screen buffer that gets loaded into a texture that gets
    // finally rendered into our window or full screen in I_FinishUpdate().
    I_VideoBuffer = screenbuffer->pixels;
    V_RestoreBuffer();

    // Clear the screen to black.
    memset(I_VideoBuffer, 0, SCREENWIDTH * SCREENHEIGHT * sizeof(*I_VideoBuffer));

    // clear out any events waiting at the start and center the mouse
    while (SDL_PollEvent(&dummy));

    initialized = true;

    // Call I_ShutdownGraphics on quit
    I_AtExit(I_ShutdownGraphics, true);
}

// Bind all variables controlling video options into the configuration
// file system.
void I_BindVideoVariables(void)
{
    M_BindIntVariable("use_mouse",                 &usemouse);
    M_BindIntVariable("fullscreen",                &fullscreen);
    M_BindIntVariable("video_display",             &video_display);
    M_BindIntVariable("aspect_ratio_correct",      &aspect_ratio_correct);
    M_BindIntVariable("integer_scaling",           &integer_scaling);
    M_BindIntVariable("vga_porch_flash",           &vga_porch_flash);
    M_BindIntVariable("startup_delay",             &startup_delay);
    M_BindIntVariable("fullscreen_width",          &fullscreen_width);
    M_BindIntVariable("fullscreen_height",         &fullscreen_height);
    M_BindIntVariable("force_software_renderer",   &force_software_renderer);
    M_BindIntVariable("max_scaling_buffer_pixels", &max_scaling_buffer_pixels);
    M_BindIntVariable("window_width",              &window_width);
    M_BindIntVariable("window_height",             &window_height);
    M_BindIntVariable("grabmouse",                 &grabmouse);
    M_BindStringVariable("video_driver",           &video_driver);
    M_BindStringVariable("window_position",        &window_position);
    M_BindIntVariable("usegamma",                  &usegamma);
    M_BindIntVariable("png_screenshots",           &png_screenshots);
}

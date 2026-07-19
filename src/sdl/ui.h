/*
 * ui.h -- C interface to the ImGui-based menu bar and console.
 *
 * The RasMol core and the SDL frontend (sdlwin.c) are C; the ImGui
 * implementation (ui_imgui.cpp) is C++.  This header is the only thing they
 * share, so the C++ UI never includes RasMol's C headers -- it talks to the
 * core exclusively through the callbacks below.
 */
#ifndef RASMOL_SDL_UI_H
#define RASMOL_SDL_UI_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hooks the frontend registers so the UI can drive the RasMol core. */
typedef struct UiCallbacks {
    /* Execute a RasMol command line (as if typed at the prompt). */
    void        (*run_command)( const char *cmd );
    /* Current console output text; sets *len to its length. */
    const char *(*console_text)( int *len );
    /* Open a file with the given RasMol format token name, or auto (NULL). */
    void        (*open_file)( const char *path );
    /* Request application exit. */
    void        (*quit)( void );
    /* Multi-line version/build text shown in Help > About (may be NULL). */
    const char  *about;
    /* Query a toggle/mode state (UI_STATE_*), for menu check marks. */
    int         (*get_state)( int key );
    /* Open a save dialog and write the image/model in the given RasMol
       format token (e.g. "bmp", "ppm", "epsf", "pdb"). */
    void        (*save_as)( const char *format );
} UiCallbacks;

/* Keys for UiCallbacks.get_state. */
enum {
    UI_STATE_SLAB, UI_STATE_HYDROGEN, UI_STATE_HETERO, UI_STATE_SPECULAR,
    UI_STATE_SHADOW, UI_STATE_STEREO, UI_STATE_LABELS, UI_STATE_PICKMODE
};

/* Lifecycle. Returns 1 on success. */
int  Ui_Init( SDL_Window *window, SDL_Renderer *renderer, const UiCallbacks *cb );
void Ui_Shutdown( void );

/* Feed every SDL event here (before the frontend interprets it). */
void Ui_ProcessEvent( const SDL_Event *ev );

/* True when ImGui is capturing input, so the frontend should not rotate/zoom. */
int  Ui_WantMouse( void );
int  Ui_WantKeyboard( void );

/* Per-frame: start a frame, build the menus/console widgets, then render.
   Ui_Render issues the ImGui draw calls onto the current SDL renderer; the
   caller clears, draws the molecule texture, calls Ui_Render, then presents. */
void Ui_BeginFrame( void );
void Ui_Build( void );
void Ui_Render( void );

/* Pixel height reserved by the top menu bar (0 until the first frame). */
int  Ui_MenuBarHeight( void );

/* Pixel height reserved by the docked bottom console (0 when hidden). */
int  Ui_ConsoleHeight( void );

/* Toggle / query the console panel visibility (also a menu item). */
void Ui_ToggleConsole( void );

#ifdef __cplusplus
}
#endif

#endif /* RASMOL_SDL_UI_H */

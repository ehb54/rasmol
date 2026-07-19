/*
 * sdlwin.c -- SDL3 windowing frontend for RasMol.
 *
 * Replaces the X11 platform layer (x11win.c + the rasmol.c X11 main) with a
 * single portable backend built on SDL3, so the same code runs on macOS,
 * Linux (Wayland/X11) and Windows.  RasMol's software renderer is unchanged:
 * the core rasterizes into FBuffer (a 32-bit 0x00RRGGBB array in the
 * THIRTYTWOBIT build) and this file simply uploads that buffer to an SDL
 * streaming texture and translates SDL input events into RasMol's "dial"
 * transform model.
 *
 * This is the MVP frontend: window display, left-drag rotate, shift/right-drag
 * translate, wheel zoom, window resize, and a headless -snapshot mode used for
 * automated verification.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef _WIN32
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <signal.h>
#endif

#include <SDL3/SDL.h>

/* This frontend owns the RASMOL/GRAPHICS global definitions (the headers emit
   the actual variables here rather than extern declarations), matching the
   convention in rastxt.c and the X11 rasmol.c. */
#define RASMOL
#define GRAPHICS

#include "rasmol.h"
#include "graphics.h"
#include "molecule.h"
#include "infile.h"
#include "abstree.h"
#include "transfor.h"
#include "cmndline.h"
#include "command.h"
#include "render.h"
#include "repres.h"
#include "pixutils.h"
#include "outfile.h"

/* ------------------------------------------------------------------ */
/* SDL state                                                          */
/* ------------------------------------------------------------------ */
static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture  *g_texture  = NULL;
static int           g_tex_w = 0, g_tex_h = 0;

/* Frontend-owned globals (each RasMol frontend defines its own, as in
   rastxt.c / the X11 rasmol.c). */
static char *FileNamePtr;
static char *ScriptNamePtr;
static int   FileFormat;
static int   ProfCount;

#ifndef _WIN32
static void TermRestore( void );   /* forward decl (used by CloseDisplay) */
#endif

/* ------------------------------------------------------------------ */
/* Graphics contract expected by the RasMol core (see graphics.h)     */
/* ------------------------------------------------------------------ */

int CreateImage( void )
{
    Long size;

    if( FBuffer ) free( FBuffer );
    size = (Long)XRange * YRange * sizeof(Pixel);
    FBuffer = (Pixel*)malloc( size + 32 );
    if( !FBuffer )
        return False;

    /* (re)create the streaming texture to match the frame buffer */
    if( g_renderer )
    {   if( g_texture && (g_tex_w != XRange || g_tex_h != YRange) )
        {   SDL_DestroyTexture( g_texture );
            g_texture = NULL;
        }
        if( !g_texture )
        {   g_texture = SDL_CreateTexture( g_renderer,
                            SDL_PIXELFORMAT_XRGB8888,
                            SDL_TEXTUREACCESS_STREAMING,
                            XRange, YRange );
            g_tex_w = XRange;  g_tex_h = YRange;
            SDL_SetTextureScaleMode( g_texture, SDL_SCALEMODE_NEAREST );
        }
    }
    return( FBuffer != (Pixel*)NULL );
}


void TransferImage( void )
{
    if( !g_renderer || !g_texture )
        return;
    SDL_UpdateTexture( g_texture, NULL, FBuffer, XRange * sizeof(Pixel) );
    SDL_SetRenderDrawColor( g_renderer, 0, 0, 0, 255 );
    SDL_RenderClear( g_renderer );
    SDL_RenderTexture( g_renderer, g_texture, NULL, NULL );
    SDL_RenderPresent( g_renderer );
}


void ClearImage( void )
{
    if( g_renderer )
    {   SDL_SetRenderDrawColor( g_renderer, 0, 0, 0, 255 );
        SDL_RenderClear( g_renderer );
        SDL_RenderPresent( g_renderer );
    }
}


void AllocateColourMap( void )
{
    /* Truecolor build: DefineColourMap() writes packed RGB straight into
       Lut[], so there is no hardware colormap to allocate. */
}


void UpdateScrollBars( void ) { }


int LookUpColour( char *name, int *r, int *g, int *b )
{
    /* Let the core fall back to its built-in colour table. */
    (void)name; (void)r; (void)g; (void)b;
    return False;
}


void SetMouseUpdateStatus( int flag )  { MouseUpdateStatus  = flag; }
void SetMouseCaptureStatus( int flag ) { MouseCaptureStatus = flag; }


void SetCanvasTitle( char *ptr )
{
    if( g_window && ptr )
        SDL_SetWindowTitle( g_window, ptr );
}


void EnableMenus( int flag )   { (void)flag; }
void BeginWait( void )         { }
void EndWait( void )           { }
void AdviseUpdate( int item )  { (void)item; }
int  PrintImage( void )        { return False; }
int  ClipboardImage( void )    { return False; }

void ReDrawWindow( void )      { ReDrawFlag |= RFRefresh; }
void UpdateLanguage( void )    { }


/* Console output + exit handlers the core expects from the frontend. */
void WriteChar( int ch )       { putc( ch, stdout ); }
void WriteString( char *ptr )  { fputs( ptr, stdout ); }
void WriteMsg( char *ptr )     { WriteString( ptr ); WriteChar( '\n' ); }

void CloseDisplay( void );     /* forward decl */

void RasMolExit( void )
{
    CloseDisplay();
    exit( 0 );
}

void RasMolFatalExit( char *msg )
{
    fprintf( stderr, "%s\n", msg );
    CloseDisplay();
    exit( 1 );
}


int OpenDisplay( void )
{
    register int i;

    for( i=0; i<10; i++ )
        DialValue[i] = 0.0;

    XRange = InitWidth  ? InitWidth  : DefaultWide;
    YRange = InitHeight ? InitHeight : DefaultHigh;
    WRange = XRange >> 1;
    HRange = YRange >> 1;
    Range  = MinFun( XRange, YRange );
    ZRange = 20000;

    for( i=0; i<256; i++ )
        ULut[i] = False;
    AllocateColourMap();
    return True;
}


void CloseDisplay( void )
{
#ifndef _WIN32
    TermRestore();
#endif
    if( g_texture )  SDL_DestroyTexture( g_texture );
    if( g_renderer ) SDL_DestroyRenderer( g_renderer );
    if( g_window )   SDL_DestroyWindow( g_window );
    g_texture = NULL; g_renderer = NULL; g_window = NULL;
    SDL_Quit();
}


/* GUI RefreshScreen: apply pending transforms, redraw, and blit.
   Mirrors the interactive path in the X11 frontend. */
void RefreshScreen( void )
{
    if( !UseSlabPlane )
    {   ReDrawFlag &= ~RFTransZ | RFSlab;
    } else ReDrawFlag &= ~RFTransZ;

    if( ReDrawFlag )
    {   if( ReDrawFlag & RFReSize )
            ReSizeScreen();

        if( ReDrawFlag & RFColour )
        {   ClearImage();
            DefineColourMap();
        }

        NextReDrawFlag = 0;
        if( Database )
        {   if( ReDrawFlag & RFApply )
                ApplyTransform();
            DrawFrame();
            TransferImage();
        } else
        {   ClearBuffers();
            TransferImage();
        }
    }
    ReDrawFlag = NextReDrawFlag;
}


/* ------------------------------------------------------------------ */
/* Tcl/Tk interpreter hooks the core references -- unused here.        */
/* ------------------------------------------------------------------ */
int ShowInterpNames( void ) { return False; }
int CheckInterpName( char __huge *name, unsigned long __huge *interpid )
{   (void)name; (void)interpid; return False; }
int SendInterpCommand( char __huge *name, unsigned long interpid,
                       char __huge *command )
{   (void)name; (void)interpid; (void)command; return False; }


/* ------------------------------------------------------------------ */
/* Terminal command console                                           */
/*                                                                    */
/* Classic RasMol runs an interactive "RasMol>" command line in the   */
/* controlling terminal alongside the graphics window.  We reproduce  */
/* that: when stdin is a tty we put it in raw mode and feed bytes to   */
/* the core's line editor (ProcessCharacter/ExecuteCommand); the SDL   */
/* event loop and stdin are multiplexed single-threaded so the        */
/* non-reentrant core is only ever touched from the main thread.      */
/* ------------------------------------------------------------------ */
#ifndef _WIN32
static struct termios g_orig_term;
static int  g_term_raw  = False;   /* terminal currently in raw mode  */
static int  g_console   = False;   /* stdin is an interactive tty     */
static int  g_stdin_eof = False;   /* piped stdin exhausted           */

static void TermRestore( void )
{
    if( g_term_raw )
    {   tcsetattr( STDIN_FILENO, TCSANOW, &g_orig_term );
        g_term_raw = False;
    }
}

static void TermRaw( void )
{
    struct termios raw;

    if( !isatty( STDIN_FILENO ) )
        return;
    tcgetattr( STDIN_FILENO, &g_orig_term );
    raw = g_orig_term;
    raw.c_iflag |= IGNBRK | IGNPAR;
    raw.c_iflag &= ~( BRKINT | PARMRK | INPCK | IXON | IXOFF );
    raw.c_lflag &= ~( ICANON | ISIG | ECHO | ECHOE | ECHOK | ECHONL | NOFLSH );
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
#ifdef VSUSP
    raw.c_cc[VSUSP] = 0;
#endif
    tcsetattr( STDIN_FILENO, TCSANOW, &raw );
    g_term_raw = True;
    g_console  = True;
}

/* Translate a raw input byte, decoding VT100 arrow-key escape sequences
   into the control codes RasMol's line editor expects (history/cursor).
   Returns the code to feed ProcessCharacter, or -1 while mid-sequence. */
static int DecodeByte( int ch )
{
    static int esc = 0;   /* 0: normal, 1: saw ESC, 2: saw ESC[ or ESC O */

    switch( esc )
    {
    case 1:
        esc = ( ch == '[' || ch == 'O' ) ? 2 : 0;
        return -1;
    case 2:
        esc = 0;
        switch( ch )
        {   case 'A': return 0x10;   /* up    -> previous history */
            case 'B': return 0x0e;   /* down  -> next history     */
            case 'C': return 0x06;   /* right -> forward char     */
            case 'D': return 0x02;   /* left  -> back char        */
        }
        return -1;
    default:
        if( ch == 0x1b ) { esc = 1; return -1; }
        return ch;
    }
}

/* Non-blocking: is a byte available on stdin right now? */
static int StdinReady( void )
{
    struct timeval tv = { 0, 0 };
    fd_set fds;
    FD_ZERO( &fds );
    FD_SET( STDIN_FILENO, &fds );
    return select( STDIN_FILENO + 1, &fds, NULL, NULL, &tv ) > 0;
}

/* Drain any pending terminal input into the command interpreter.
   Sets *quit if a command asks to exit, *redraw if the view changed. */
static void ServiceConsole( int *quit, int *redraw )
{
    unsigned char b;
    int ch;

    if( g_stdin_eof )
        return;

    while( StdinReady() )
    {   ssize_t n = read( STDIN_FILENO, &b, 1 );
        if( n == 0 ) { g_stdin_eof = True; break; }   /* EOF: keep window */
        if( n < 0 )  break;

        ch = DecodeByte( b );
        if( ch < 0 )
            continue;

        if( ProcessCharacter( ch ) )
        {   if( ExecuteCommand() )
            {   *quit = True; return;
            }
            /* Render after each command, as classic RasMol does, so a
               command's effect is on screen (and in the frame buffer)
               before the next command runs. */
            RefreshScreen();
            (void)redraw;
            if( !CommandActive )
                ResetCommandLine( 0 );
        }
    }
}

static void ConsoleSignal( int sig )
{
    (void)sig;
    TermRestore();
    CloseDisplay();
    _exit( 0 );
}
#endif /* !_WIN32 */


/* ------------------------------------------------------------------ */
/* Frontend driver                                                    */
/* ------------------------------------------------------------------ */

static void InitCore( void )
{
    Interactive = True;

    FileNamePtr = NULL;
    ScriptNamePtr = NULL;
    InitWidth = InitHeight = InitXPos = InitYPos = 0;
    ProfCount = 0;

    FileFormat = FormatPDB;
    CalcBondsFlag = True;
    CalcSurfFlag = False;
}


static void InitSubsystems( void )
{
    InitialiseCmndLine();
    InitialiseCommand();
    InitialiseTransform();
    InitialiseDatabase();
    InitialiseRenderer();
    InitialisePixUtils();
    InitialiseAbstree();
    InitialiseOutFile();
    InitialiseRepres();
    InitHelpFile();
}


/* Drag mode selected on mouse-button-down. */
enum { DRAG_NONE, DRAG_ROTATE, DRAG_TRANSLATE };

static void WrapDial( int idx )
{
    if( DialValue[idx] >  1.0 ) DialValue[idx] -= 2.0;
    if( DialValue[idx] < -1.0 ) DialValue[idx] += 2.0;
}


/* Render a single frame headlessly and save it as a BMP.  Works with the
   SDL "offscreen"/"dummy" video driver, so it needs no window server.
   Returns 0 on success. */
static int SaveSnapshot( const char *path )
{
    SDL_Surface *surf;

    ReDrawFlag |= RFInitial | RFColour;
    RefreshScreen();

    surf = SDL_RenderReadPixels( g_renderer, NULL );
    if( !surf )
    {   fprintf( stderr, "snapshot: RenderReadPixels failed: %s\n", SDL_GetError() );
        return 1;
    }
    if( !SDL_SaveBMP( surf, path ) )
    {   fprintf( stderr, "snapshot: SaveBMP failed: %s\n", SDL_GetError() );
        SDL_DestroySurface( surf );
        return 1;
    }
    SDL_DestroySurface( surf );
    return 0;
}


int main( int argc, char *argv[] )
{
    const char *filename = NULL;
    const char *snapshot = NULL;
    int drag = DRAG_NONE;
    int i, running, done;

    setvbuf( stdout, NULL, _IONBF, 0 );   /* prompt/echo appears immediately */
    InitCore();

    for( i=1; i<argc; i++ )
    {   if( !strcmp(argv[i],"-snapshot") && i+1<argc )
        {   snapshot = argv[++i];
        } else if( !strcmp(argv[i],"-pdb") && i+1<argc )
        {   FileFormat = FormatPDB; filename = argv[++i];
        } else if( argv[i][0] != '-' )
        {   filename = argv[i];
        }
    }

    if( !SDL_Init( SDL_INIT_VIDEO ) )
    {   fprintf( stderr, "SDL_Init failed: %s\n", SDL_GetError() );
        return 1;
    }

    OpenDisplay();

    if( !SDL_CreateWindowAndRenderer( "RasMol", XRange, YRange,
                                      SDL_WINDOW_RESIZABLE,
                                      &g_window, &g_renderer ) )
    {   fprintf( stderr, "SDL_CreateWindowAndRenderer failed: %s\n", SDL_GetError() );
        return 1;
    }
    CreateImage();

    InitSubsystems();

    if( filename )
    {   strcpy( DataFileName, filename );
        if( FetchFile( FileFormat, True, (char*)filename ) )
        {   DefaultRepresentation();
        } else
            fprintf( stderr, "Error: unable to read '%s'\n", filename );
    }

    if( snapshot )
    {   done = SaveSnapshot( snapshot );
        CloseDisplay();
        return done;
    }

    WriteString( "RasMol Molecular Renderer (SDL frontend)\n" );
    WriteString( "Based on RasMol 2.7.6 by Roger Sayle and "
                 "Herbert J. Bernstein\n\n" );

    ReDrawFlag |= RFInitial | RFColour;
    RefreshScreen();

#ifndef _WIN32
    /* Classic RasMol interactive command line in the controlling terminal. */
    TermRaw();
    signal( SIGINT,  ConsoleSignal );
    signal( SIGTERM, ConsoleSignal );
    if( g_console )
    {   WriteString( "Type RasMol commands here; rotate/zoom with the mouse "
                     "in the window.\n" );
        ResetCommandLine( 1 );
    }
#endif

    running = True;
    while( running )
    {   SDL_Event ev;
        int need = False;
        int poll_stdin = False;
        int got;

#ifndef _WIN32
        poll_stdin = ( g_console || !g_stdin_eof );
#endif
        got = poll_stdin ? SDL_WaitEventTimeout( &ev, 20 )
                         : SDL_WaitEvent( &ev );

        if( got )
        do {
            switch( ev.type )
            {
            case SDL_EVENT_QUIT:
                running = False;
                break;

            case SDL_EVENT_KEY_DOWN:
                /* Window keystrokes are not command input (that is the
                   terminal); only Escape closes the app as a convenience. */
                if( ev.key.key == SDLK_ESCAPE )
                    running = False;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                XRange = ev.window.data1;
                YRange = ev.window.data2;
                WRange = XRange >> 1;
                HRange = YRange >> 1;
                Range  = MinFun( XRange, YRange );
                ReDrawFlag |= RFReSize | RFColour | RFApply;
                need = True;
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if( ev.button.button == SDL_BUTTON_LEFT )
                {   SDL_Keymod mod = SDL_GetModState();
                    drag = ( mod & SDL_KMOD_SHIFT ) ? DRAG_TRANSLATE : DRAG_ROTATE;
                } else if( ev.button.button == SDL_BUTTON_RIGHT )
                    drag = DRAG_TRANSLATE;
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                drag = DRAG_NONE;
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if( drag == DRAG_ROTATE )
                {   DialValue[DialRY] += 2.0 * ev.motion.xrel / XRange;
                    DialValue[DialRX] += 2.0 * ev.motion.yrel / YRange;
                    WrapDial( DialRX );  WrapDial( DialRY );
                    ReDrawFlag |= RFRotateX | RFRotateY;
                    need = True;
                } else if( drag == DRAG_TRANSLATE )
                {   DialValue[DialTX] += 2.0 * ev.motion.xrel / XRange;
                    DialValue[DialTY] -= 2.0 * ev.motion.yrel / YRange;
                    ReDrawFlag |= RFTransX | RFTransY;
                    need = True;
                }
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                DialValue[DialZoom] += 0.1 * ev.wheel.y;
                if( DialValue[DialZoom] >  1.0 ) DialValue[DialZoom] =  1.0;
                if( DialValue[DialZoom] < -1.0 ) DialValue[DialZoom] = -1.0;
                ReDrawFlag |= RFZoom;
                need = True;
                break;

            case SDL_EVENT_WINDOW_EXPOSED:
                ReDrawFlag |= RFRefresh;
                need = True;
                break;
            }
        } while( SDL_PollEvent( &ev ) );

#ifndef _WIN32
        {   int quit = False;
            ServiceConsole( &quit, &need );
            if( quit )
                running = False;
        }
#endif

        if( need && running )
            RefreshScreen();
    }

    CloseDisplay();
    return 0;
}

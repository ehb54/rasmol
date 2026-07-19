/*
 * ui_imgui.cpp -- Dear ImGui menu bar and console for the SDL frontend.
 *
 * Pure UI: builds a RasMol-style pull-down menu bar and an in-window command
 * console.  It never touches the RasMol core directly -- menu items and the
 * console input line are turned into RasMol command strings and handed to the
 * frontend through the UiCallbacks registered in Ui_Init.
 */
#include "ui.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include <string.h>

static UiCallbacks   g_cb;
static SDL_Window   *g_window   = nullptr;
static SDL_Renderer *g_renderer = nullptr;
static bool          g_show_console = true;
static int           g_menubar_h    = 0;
static const float   CONSOLE_HEIGHT = 220.0f;   /* docked bottom panel */

/* ---- helpers ---- */

static void RunCmd( const char *cmd )
{
    if( g_cb.run_command )
        g_cb.run_command( cmd );
}

/* A menu item that issues a RasMol command when chosen. */
static void CmdItem( const char *label, const char *cmd )
{
    if( ImGui::MenuItem( label ) )
        RunCmd( cmd );
}

/* SDL file-dialog callback: load the chosen file via the frontend. */
static void OpenFileCB( void *userdata, const char * const *filelist, int filter )
{
    (void)userdata; (void)filter;
    if( filelist && filelist[0] && g_cb.open_file )
        g_cb.open_file( filelist[0] );
}

static void ShowOpenDialog( void )
{
    static const SDL_DialogFileFilter filters[] = {
        { "Molecular structures", "pdb;ent;cif;mol;mol2;xyz;sdf" },
        { "All files", "*" }
    };
    SDL_ShowOpenFileDialog( OpenFileCB, nullptr, g_window,
                            filters, SDL_arraysize(filters), nullptr, false );
}

/* ---- public API ---- */

extern "C" int Ui_Init( SDL_Window *window, SDL_Renderer *renderer,
                        const UiCallbacks *cb )
{
    g_window   = window;
    g_renderer = renderer;
    g_cb       = *cb;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;   /* don't litter an imgui.ini file */

    if( !ImGui_ImplSDL3_InitForSDLRenderer( window, renderer ) )
        return 0;
    if( !ImGui_ImplSDLRenderer3_Init( renderer ) )
        return 0;
    return 1;
}

extern "C" void Ui_Shutdown( void )
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

extern "C" void Ui_ProcessEvent( const SDL_Event *ev )
{
    ImGui_ImplSDL3_ProcessEvent( ev );
}

extern "C" int Ui_WantMouse( void )
{
    return ImGui::GetIO().WantCaptureMouse ? 1 : 0;
}

extern "C" int Ui_WantKeyboard( void )
{
    return ImGui::GetIO().WantCaptureKeyboard ? 1 : 0;
}

extern "C" int Ui_MenuBarHeight( void )
{
    return g_menubar_h;
}

extern "C" int Ui_ConsoleHeight( void )
{
    return g_show_console ? (int)CONSOLE_HEIGHT : 0;
}

extern "C" void Ui_ToggleConsole( void )
{
    g_show_console = !g_show_console;
}

extern "C" void Ui_BeginFrame( void )
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

static void BuildMenuBar( void )
{
    if( !ImGui::BeginMainMenuBar() )
        return;

    g_menubar_h = (int)ImGui::GetWindowHeight();

    if( ImGui::BeginMenu( "File" ) )
    {   if( ImGui::MenuItem( "Open..." ) ) ShowOpenDialog();
        ImGui::Separator();
        CmdItem( "Close",        "zap" );
        ImGui::Separator();
        if( ImGui::MenuItem( "Quit" ) && g_cb.quit ) g_cb.quit();
        ImGui::EndMenu();
    }

    if( ImGui::BeginMenu( "Display" ) )
    {   CmdItem( "Wireframe",    "backbone off\nspacefill off\nwireframe on" );
        CmdItem( "Backbone",     "wireframe off\nspacefill off\nbackbone on" );
        CmdItem( "Sticks",       "spacefill off\nbackbone off\nwireframe 100" );
        CmdItem( "Spacefill",    "wireframe off\nbackbone off\nspacefill on" );
        CmdItem( "Ball & Stick", "backbone off\nwireframe 60\nspacefill 150" );
        ImGui::Separator();
        CmdItem( "Ribbons",      "cartoons off\nstrands off\nribbons on" );
        CmdItem( "Strands",      "ribbons off\ncartoons off\nstrands on" );
        CmdItem( "Cartoons",     "ribbons off\nstrands off\ncartoons on" );
        ImGui::Separator();
        CmdItem( "Molecular Surface", "surface solvent solid" );
        ImGui::EndMenu();
    }

    if( ImGui::BeginMenu( "Colours" ) )
    {   CmdItem( "Monochrome",   "colour white" );
        CmdItem( "CPK",          "colour cpk" );
        CmdItem( "Shapely",      "colour shapely" );
        CmdItem( "Group",        "colour group" );
        CmdItem( "Chain",        "colour chain" );
        CmdItem( "Temperature",  "colour temperature" );
        CmdItem( "Structure",    "colour structure" );
        CmdItem( "Amino",        "colour amino" );
        CmdItem( "User",         "colour user" );
        ImGui::EndMenu();
    }

    if( ImGui::BeginMenu( "Options" ) )
    {   CmdItem( "Slab Mode On",  "slab on" );
        CmdItem( "Slab Mode Off", "slab off" );
        ImGui::Separator();
        CmdItem( "Specular On",   "set specular on" );
        CmdItem( "Specular Off",  "set specular off" );
        CmdItem( "Shadows On",    "set shadow on" );
        CmdItem( "Shadows Off",   "set shadow off" );
        CmdItem( "Labels On",     "labels on" );
        CmdItem( "Labels Off",    "labels off" );
        ImGui::EndMenu();
    }

    if( ImGui::BeginMenu( "Settings" ) )
    {   CmdItem( "Pick Off",      "set picking off" );
        CmdItem( "Pick Ident",    "set picking ident" );
        CmdItem( "Pick Distance", "set picking distance" );
        CmdItem( "Pick Angle",    "set picking angle" );
        CmdItem( "Pick Torsion",  "set picking torsion" );
        CmdItem( "Pick Label",    "set picking label" );
        ImGui::EndMenu();
    }

    if( ImGui::BeginMenu( "View" ) )
    {   ImGui::MenuItem( "Console", nullptr, &g_show_console );
        ImGui::EndMenu();
    }

    if( ImGui::BeginMenu( "Help" ) )
    {   CmdItem( "Information",   "show information" );
        CmdItem( "Commands",      "help" );
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

static void BuildConsole( void )
{
    if( !g_show_console )
        return;

    /* Dock the console across the bottom of the window so it never overlaps
       the molecule view (which the frontend lays out above it). */
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(  ImVec2( 0.0f, disp.y - CONSOLE_HEIGHT ) );
    ImGui::SetNextWindowSize( ImVec2( disp.x, CONSOLE_HEIGHT ) );
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse;
    if( ImGui::Begin( "RasMol Console", &g_show_console, flags ) )
    {
        const float footer = ImGui::GetStyle().ItemSpacing.y +
                             ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild( "scroll", ImVec2( 0, -footer ), ImGuiChildFlags_None,
                           ImGuiWindowFlags_HorizontalScrollbar );

        int len = 0;
        const char *txt = g_cb.console_text ? g_cb.console_text( &len ) : "";
        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 4, 1 ) );
        ImGui::TextUnformatted( txt, txt + len );
        ImGui::PopStyleVar();

        if( ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f )
            ImGui::SetScrollHereY( 1.0f );
        ImGui::EndChild();

        ImGui::Separator();

        static char input[512] = "";
        ImGui::PushItemWidth( -1 );
        bool reclaim = false;
        if( ImGui::InputText( "##cmd", input, sizeof(input),
                              ImGuiInputTextFlags_EnterReturnsTrue ) )
        {   if( input[0] )
                RunCmd( input );
            input[0] = '\0';
            reclaim = true;
        }
        ImGui::PopItemWidth();
        ImGui::SetItemDefaultFocus();
        if( reclaim )
            ImGui::SetKeyboardFocusHere( -1 );
    }
    ImGui::End();
}

extern "C" void Ui_Build( void )
{
    BuildMenuBar();
    BuildConsole();
}

extern "C" void Ui_Render( void )
{
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData( ImGui::GetDrawData(), g_renderer );
}

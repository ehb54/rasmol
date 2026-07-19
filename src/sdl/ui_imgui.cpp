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
static bool          g_open_about   = false;
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

/* PickMode values (mirror render.h). */
enum { PM_None=0, PM_Ident=1, PM_Dist=2, PM_Angle=3, PM_Torsn=4,
       PM_Label=5, PM_Monit=6, PM_Centr=7, PM_Coord=9, PM_Bond=13 };

static int UiState( int key )
{
    return g_cb.get_state ? g_cb.get_state( key ) : 0;
}

/* A checkbox menu item reflecting a core toggle; sends cmd_on/cmd_off. */
static void ToggleItem( const char *label, int key,
                        const char *cmd_on, const char *cmd_off )
{
    bool on = UiState( key ) != 0;
    if( ImGui::MenuItem( label, nullptr, on ) )
        RunCmd( on ? cmd_off : cmd_on );
}

/* A radio menu item, checked when PickMode matches. */
static void PickItem( const char *label, int mode, const char *cmd )
{
    bool on = UiState( UI_STATE_PICKMODE ) == mode;
    if( ImGui::MenuItem( label, nullptr, on ) )
        RunCmd( cmd );
}

/* An Export item that opens a save dialog for the given format token. */
static void ExportItem( const char *label, const char *fmt )
{
    if( ImGui::MenuItem( label ) && g_cb.save_as )
        g_cb.save_as( fmt );
}

/* The current exclusive Display representation (-1 = none chosen yet). */
static int g_display = -1;

/* A Display item: switching to it clears the other representations first
   (RasMol representations are additive, so the menu enforces one at a time,
   as classic RasMol does). Checkmarks the active one. */
static void DispItem( const char *label, int id, const char *enable )
{
    if( ImGui::MenuItem( label, nullptr, g_display == id ) )
    {   char cmd[256];
        g_display = id;
        snprintf( cmd, sizeof(cmd),
                  "spacefill off\nwireframe off\nbackbone off\n"
                  "ribbons off\nstrands off\ncartoons off\nsurface off\n%s",
                  enable );
        RunCmd( cmd );
    }
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
        if( ImGui::MenuItem( "Save As..." ) && g_cb.save_as ) g_cb.save_as( "pdb" );
        CmdItem( "Close",        "zap" );
        ImGui::Separator();
        if( ImGui::MenuItem( "Exit" ) && g_cb.quit ) g_cb.quit();
        ImGui::EndMenu();
    }

    if( ImGui::BeginMenu( "Display" ) )
    {   DispItem( "Wireframe",    0, "wireframe on" );
        DispItem( "Backbone",     1, "backbone 80" );
        DispItem( "Sticks",       2, "wireframe 100" );
        DispItem( "Spacefill",    3, "spacefill on" );
        DispItem( "Ball & Stick", 4, "spacefill 120\nwireframe 40" );
        DispItem( "Ribbons",      5, "ribbons on" );
        DispItem( "Strands",      6, "strands on" );
        DispItem( "Cartoons",     7, "cartoons on" );
        DispItem( "Molecular Surface", 8, "surface solvent solid" );
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
        CmdItem( "User",         "colour user" );
        CmdItem( "Model",        "colour model" );
        CmdItem( "Alt",          "colour altl" );
        ImGui::EndMenu();
    }

    if( ImGui::BeginMenu( "Options" ) )
    {   ToggleItem( "Slab Mode",    UI_STATE_SLAB,     "slab on",         "slab off" );
        ToggleItem( "Hydrogens",    UI_STATE_HYDROGEN,
                    "set hydrogen true\nselect all\nwireframe on",
                    "set hydrogen false\nrestrict not hydrogen" );
        ToggleItem( "Hetero Atoms", UI_STATE_HETERO,
                    "set hetero true\nselect all\nwireframe on",
                    "set hetero false\nrestrict not hetero" );
        ToggleItem( "Specular",     UI_STATE_SPECULAR, "set specular on", "set specular off" );
        ToggleItem( "Shadows",      UI_STATE_SHADOW,   "set shadow on",   "set shadow off" );
        ToggleItem( "Stereo",       UI_STATE_STEREO,   "stereo on",       "stereo off" );
        ToggleItem( "Labels",       UI_STATE_LABELS,   "labels on",       "labels off" );
        ImGui::EndMenu();
    }

    if( ImGui::BeginMenu( "Settings" ) )
    {   PickItem( "Pick Off",      PM_None,  "set picking off" );
        PickItem( "Pick Ident",    PM_Ident, "set picking ident" );
        PickItem( "Pick Distance", PM_Dist,  "set picking distance" );
        PickItem( "Pick Monitor",  PM_Monit, "set picking monitor" );
        PickItem( "Pick Angle",    PM_Angle, "set picking angle" );
        PickItem( "Pick Torsion",  PM_Torsn, "set picking torsion" );
        PickItem( "Pick Label",    PM_Label, "set picking label" );
        PickItem( "Pick Centre",   PM_Centr, "set picking centre" );
        PickItem( "Pick Coord",    PM_Coord, "set picking coord" );
        PickItem( "Pick Bond",     PM_Bond,  "set picking bond" );
        ImGui::EndMenu();
    }

    if( ImGui::BeginMenu( "Export" ) )
    {   ExportItem( "IRIS RGB...",     "iris" );
        ExportItem( "PPM...",          "ppm" );
        ExportItem( "Sun Raster...",   "sun" );
        ExportItem( "PostScript...",   "epsf" );
        ExportItem( "PICT...",         "pict" );
        ExportItem( "Vector PS...",    "vectps" );
        ExportItem( "Molscript...",    "molscript" );
        ExportItem( "Kinemage...",     "kinemage" );
        ExportItem( "POVRay 3...",     "povray" );
        ExportItem( "VRML...",         "vrml" );
        ExportItem( "Ramachandran...", "ramachan" );
        ExportItem( "Raster3D...",     "raster3d" );
        ExportItem( "RasMol Script...","script" );
        ImGui::EndMenu();
    }

    if( ImGui::BeginMenu( "View" ) )
    {   ImGui::MenuItem( "Console", nullptr, &g_show_console );
        ImGui::EndMenu();
    }

    if( ImGui::BeginMenu( "Help" ) )
    {   CmdItem( "Information",   "show information" );
        CmdItem( "Commands",      "help" );
        ImGui::Separator();
        if( ImGui::MenuItem( "About RasMol..." ) )
            g_open_about = true;
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

static void BuildAbout( void )
{
    if( g_open_about )
    {   ImGui::OpenPopup( "About RasMol" );
        g_open_about = false;
    }

    /* Centre the modal over the window. */
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f ) );

    if( ImGui::BeginPopupModal( "About RasMol", NULL,
                                ImGuiWindowFlags_AlwaysAutoResize ) )
    {   const char *txt = g_cb.about ? g_cb.about : "RasMol";
        ImGui::TextUnformatted( txt );
        ImGui::Separator();
        if( ImGui::Button( "OK", ImVec2( 120, 0 ) ) )
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
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
    BuildAbout();
}

extern "C" void Ui_Render( void )
{
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData( ImGui::GetDrawData(), g_renderer );
}

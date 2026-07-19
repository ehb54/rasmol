/*
 * rasmol_text_stubs.c -- link shims for the text / headless RasMol frontend.
 *
 * The text frontend (rastxt.c) has no windowing layer, so it must supply the
 * GUI callbacks the core expects.  When RasMol is built without CBFlib
 * (NO_CBFLIB), it must also supply the CIF handle layer that infile.c calls;
 * with CBFlib enabled, cif.c provides the real implementations and the CIF
 * stubs here compile out.
 *
 * Consequences of the NO_CBFLIB path:
 *   - CIF/mmCIF *input* is unavailable (LoadCIFMolecule fails gracefully).
 *     PDB input is unaffected -- that is SOMO's path.
 * Provide real CBFlib integration (build with -DUSE-side enabled) to restore
 * CIF support.
 */

#include <stdio.h>

/* ---- GUI callbacks not present in a text frontend (always needed) ---- */
void ReDrawWindow( void )   { }
void UpdateLanguage( void ) { }

#ifdef NO_CBFLIB
/* ---- CBFlib-backed CIF handle layer (normally in cif.c) ----
 * The CIF code path is not exercised in the headless build; each stub returns
 * a nonzero (error) status so an accidental call fails cleanly.
 */
int cif_make_handle( void *h )                  { (void)h; return 1; }
int cif_free_handle( void *h )                  { (void)h; return 1; }
int cif_read_file( void *h, FILE *f )           { (void)h; (void)f; return 1; }
int cif_datablock_name( void *h, void *n )      { (void)h; (void)n; return 1; }
int cif_rewind_datablock( void *h )             { (void)h; return 1; }
int cif_find_column( void *h, void *c )         { (void)h; (void)c; return 1; }
int cif_select_column( void *h, unsigned int c ){ (void)h; (void)c; return 1; }
int cif_select_row( void *h, unsigned int r )   { (void)h; (void)r; return 1; }
int cif_column_number( void *h, void *c )       { (void)h; (void)c; return 1; }
int cif_count_rows( void *h, void *r )          { (void)h; (void)r; return 1; }
int cif_find_tag( void *h, void *t )            { (void)h; (void)t; return 1; }
int cif_findtag( void *h, void *t )             { (void)h; (void)t; return 1; }
int cif_get_value( void *h, void *v )           { (void)h; (void)v; return 1; }
int cif_ctonum( void *s, void *d )              { (void)s; (void)d; return 1; }
#endif /* NO_CBFLIB */

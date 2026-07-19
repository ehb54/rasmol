/*
 * rasmol_cif_stubs.c -- no-op CIF handle layer for builds without CBFlib.
 *
 * infile.c calls the cif_* wrapper layer unconditionally; the real
 * implementations live in cif.c and require CBFlib.  When RasMol is built with
 * NO_CBFLIB these stubs satisfy the link and make CIF/mmCIF *input* fail
 * gracefully.  PDB input is unaffected -- that is SOMO's path.  This file is
 * compiled into the core library only when CBFlib is disabled; enabling CBFlib
 * replaces it with cif.c.
 */

#include <stdio.h>

#ifdef NO_CBFLIB
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

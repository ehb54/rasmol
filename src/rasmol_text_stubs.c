/*
 * rasmol_text_stubs.c -- GUI callbacks for the headless text frontend.
 *
 * The text frontend (rastxt.c) has no windowing layer, so it supplies no-op
 * implementations of the callbacks the core invokes from its language/menu
 * code.  The CIF handle layer needed when building without CBFlib lives in
 * rasmol_cif_stubs.c (compiled into the core library).
 */

/* GUI callbacks invoked by langsel_*.c; meaningless without a window. */
void ReDrawWindow( void )   { }
void UpdateLanguage( void ) { }

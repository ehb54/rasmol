/*
 * meshio.c -- read triangulated surface meshes from files (see meshio.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "meshio.h"

#define MESH_LINE 4096

/* ---------------------------------------------------------------------
 * Buffered reading: whitespace-separated numbers (BEST) or whole lines.
 * --------------------------------------------------------------------- */

typedef struct {
    FILE *fp;
    char buf[65536];
    size_t pos, len;
    long line;                 /* current line number, from 1 */
} Reader;

static int ReadOpen( Reader *r, const char *path )
{
    r->fp = fopen(path,"rb");
    r->pos = r->len = 0;
    r->line = 1;
    return r->fp != NULL;
}

static void ReadClose( Reader *r )
{
    if( r->fp ) fclose(r->fp);
    r->fp = NULL;
}

static int ReadChar( Reader *r )
{
    if( r->pos >= r->len )
    {   r->len = fread(r->buf,1,sizeof(r->buf),r->fp);
        r->pos = 0;
        if( !r->len ) return EOF;
    }
    return (unsigned char)r->buf[r->pos++];
}

/* The next whitespace-separated token; 0 at the end of the file. */
static int ReadToken( Reader *r, char *tok, int max )
{
    int c, n = 0;

    do
    {   c = ReadChar(r);
        if( c == '\n' ) r->line++;
    } while( c != EOF && isspace(c) );
    if( c == EOF ) return 0;

    while( c != EOF && !isspace(c) )
    {   if( n < max-1 ) tok[n++] = (char)c;
        c = ReadChar(r);
    }
    if( c == '\n' ) r->line++;
    tok[n] = '\0';
    return 1;
}

/* A number; Fortran's D exponent is accepted. */
static int ReadDouble( Reader *r, double *value )
{
    char tok[64], *end;
    int i;

    if( !ReadToken(r,tok,sizeof(tok)) ) return 0;
    for( i=0; tok[i]; i++ )
        if( tok[i]=='D' || tok[i]=='d' ) tok[i] = 'E';
    *value = strtod(tok,&end);
    return end != tok && *end == '\0';
}

/* The next line without its line end; 0 at the end of the file.
   A line longer than the buffer is an error (-1). */
static int ReadLine( Reader *r, char *line, int max )
{
    int c, n = 0;

    c = ReadChar(r);
    if( c == EOF ) return 0;
    while( c != EOF && c != '\n' )
    {   if( n >= max-1 ) return -1;
        if( c != '\r' ) line[n++] = (char)c;
        c = ReadChar(r);
    }
    line[n] = '\0';
    r->line++;
    return 1;
}

/* Reads up to max numbers from the start of a line; returns how many. */
static int LineNumbers( const char *line, double *v, int max )
{
    const char *p = line;
    char *end;
    int n = 0;

    while( n < max )
    {   while( *p && isspace((unsigned char)*p) ) p++;
        if( !*p ) break;
        v[n] = strtod(p,&end);
        if( end == p ) break;
        n++;
        p = end;
    }
    return n;
}

static int BlankLine( const char *line )
{
    while( *line && isspace((unsigned char)*line) ) line++;
    return !*line;
}

/* ---------------------------------------------------------------------
 * Growable arrays and joining triangle lists into shared vertices.
 * --------------------------------------------------------------------- */

typedef struct {
    double *v;
    size_t n, cap;
} DoubleVec;

static int DoublePush( DoubleVec *a, const double *v, int count )
{
    int i;

    if( a->n + count > a->cap )
    {   size_t cap = a->cap ? 2*a->cap : 9*1024;
        double *p;
        while( cap < a->n + count ) cap *= 2;
        p = (double*)realloc(a->v,cap*sizeof(double));
        if( !p ) return 0;
        a->v = p;
        a->cap = cap;
    }
    for( i=0; i<count; i++ ) a->v[a->n++] = v[i];
    return 1;
}

static unsigned long long HashBytes( const void *data, size_t len )
{
    const unsigned char *c = (const unsigned char*)data;
    unsigned long long h = 1469598103934665603ULL;
    size_t i;

    for( i=0; i<len; i++ )
    {   h ^= c[i];
        h *= 1099511628211ULL;
    }
    return h;
}

/* Turns 9*ntri coordinates (soup) into shared vertices: vertices whose
   coordinates are identical become one. */
static int JoinVertices( MeshData *m, double *soup, long ntri, char *err, int errlen )
{
    size_t hsize = 1, hmask, h;
    long *table;
    long k, nv = 0;

    m->ntri = ntri;
    m->tri = (long*)malloc(3*(size_t)ntri*sizeof(long));
    m->xyz = (double*)malloc(9*(size_t)ntri*sizeof(double));
    while( hsize < 4*(size_t)ntri ) hsize <<= 1;   /* > 3*ntri: never full */
    hmask = hsize-1;
    table = (long*)malloc(hsize*sizeof(long));
    if( !m->tri || !m->xyz || !table )
    {   free(table);
        snprintf(err,errlen,"out of memory for %ld triangles",ntri);
        return 1;
    }
    for( h=0; h<hsize; h++ ) table[h] = -1;

    for( k=0; k<3*ntri; k++ )
    {   double *p = soup+3*k;
        int i;
        for( i=0; i<3; i++ )
            if( p[i] == 0.0 ) p[i] = 0.0;   /* one key for -0 and +0 */
        h = HashBytes(p,3*sizeof(double)) & hmask;
        while( table[h] >= 0 )
        {   double *q = m->xyz+3*table[h];
            if( q[0]==p[0] && q[1]==p[1] && q[2]==p[2] ) break;
            h = (h+1) & hmask;
        }
        if( table[h] < 0 )
        {   m->xyz[3*nv]   = p[0];
            m->xyz[3*nv+1] = p[1];
            m->xyz[3*nv+2] = p[2];
            table[h] = nv++;
        }
        m->tri[k] = table[h];
    }
    free(table);
    m->nvert = nv;
    if( nv < 3*ntri )
    {   double *p = (double*)realloc(m->xyz,3*(size_t)nv*sizeof(double));
        if( p ) m->xyz = p;
    }
    return 0;
}

/* ---------------------------------------------------------------------
 * The formats.
 * --------------------------------------------------------------------- */

static int ReadBEST( const char *path, MeshData *m, char *err, int errlen )
{
    Reader r;
    double v[3], *soup;
    long ntri, i;
    int ok;

    if( !ReadOpen(&r,path) )
    {   snprintf(err,errlen,"cannot open '%s'",path);
        return 1;
    }
    if( !ReadDouble(&r,&v[0]) || !ReadDouble(&r,&v[1]) || !ReadDouble(&r,&v[2])
        || v[0] < 1 || v[0] != floor(v[0]) )
    {   ReadClose(&r);
        snprintf(err,errlen,"'%s': first line is not \"N area volume\"",path);
        return 1;
    }
    ntri = (long)v[0];
    m->filearea = v[1];
    m->filevolume = v[2];

    soup = (double*)malloc(9*(size_t)ntri*sizeof(double));
    if( !soup )
    {   ReadClose(&r);
        snprintf(err,errlen,"out of memory for %ld triangles",ntri);
        return 1;
    }
    for( i=0; i<9*ntri; i++ )
        if( !ReadDouble(&r,&soup[i]) )
        {   long line = r.line;
            ReadClose(&r);
            free(soup);
            snprintf(err,errlen,"'%s' line %ld: expected %ld triangles, read %ld",
                     path,line,ntri,i/9);
            return 1;
        }
    ReadClose(&r);
    ok = JoinVertices(m,soup,ntri,err,errlen);
    free(soup);
    return ok;
}

static double BodUnits( const char *unit )
{
    char u[16];
    int i;

    for( i=0; unit[i] && i<15; i++ ) u[i] = (char)toupper((unsigned char)unit[i]);
    u[i] = '\0';
    if( !strcmp(u,"A") || !strcmp(u,"ANGSTROM") || !strcmp(u,"ANGSTROMS") ) return 1.0;
    if( !strcmp(u,"NM") ) return 10.0;
    if( !strcmp(u,"UM") ) return 1.0e4;
    if( !strcmp(u,"CM") ) return 1.0e8;
    if( !strcmp(u,"M") )  return 1.0e10;
    if( !strcmp(u,"L") )  return 1.0;      /* ZENO's unitless length */
    return -1.0;
}

static int ReadBOD( const char *path, MeshData *m, char *err, int errlen )
{
    static const char *other[] = { "SPHERE", "S", "ST", "CUBE", "CUBOID",
        "CYLINDER", "TORUS", "VOXELS", "PILLAR", "SOLID_TORUS", NULL };
    Reader r;
    DoubleVec soup = { NULL, 0, 0 };
    char line[MESH_LINE], word[32];
    double v[9], scale = 1.0;
    long ntri;
    int status, i, j, ok;

    if( !ReadOpen(&r,path) )
    {   snprintf(err,errlen,"cannot open '%s'",path);
        return 1;
    }
    while( (status = ReadLine(&r,line,sizeof(line))) != 0 )
    {   const char *p = line;
        if( status < 0 )
        {   ReadClose(&r);
            free(soup.v);
            snprintf(err,errlen,"'%s' line %ld is too long",path,r.line);
            return 1;
        }
        while( *p && isspace((unsigned char)*p) ) p++;
        if( !*p || *p=='#' ) continue;
        for( j=0; *p && !isspace((unsigned char)*p) && j<31; j++ )
            word[j] = (char)toupper((unsigned char)*p++);
        word[j] = '\0';

        if( !strcmp(word,"TRIANGLE") || !strcmp(word,"T") )
        {   if( LineNumbers(p,v,9) != 9 )
            {   ReadClose(&r);
                free(soup.v);
                snprintf(err,errlen,"'%s' line %ld: TRIANGLE needs 9 numbers",path,r.line-1);
                return 1;
            }
            if( !DoublePush(&soup,v,9) )
            {   ReadClose(&r);
                free(soup.v);
                snprintf(err,errlen,"out of memory reading '%s'",path);
                return 1;
            }
        } else if( !strcmp(word,"UNITS") )
        {   while( *p && isspace((unsigned char)*p) ) p++;
            for( j=0; *p && !isspace((unsigned char)*p) && j<31; j++ ) word[j] = *p++;
            word[j] = '\0';
            scale = BodUnits(word);
            if( scale < 0.0 )
            {   ReadClose(&r);
                free(soup.v);
                snprintf(err,errlen,"'%s': unknown UNITS '%s'",path,word);
                return 1;
            }
        } else
        {   for( i=0; other[i]; i++ )
                if( !strcmp(word,other[i]) ) break;
            if( other[i] ) m->skipped++;
        }
    }
    ReadClose(&r);

    ntri = (long)(soup.n/9);
    if( !ntri )
    {   free(soup.v);
        snprintf(err,errlen,"'%s' has no TRIANGLE lines",path);
        return 1;
    }
    if( scale != 1.0 )
    {   size_t k;
        for( k=0; k<soup.n; k++ ) soup.v[k] *= scale;
    }
    ok = JoinVertices(m,soup.v,ntri,err,errlen);
    free(soup.v);
    return ok;
}

/* Reads "nvert" lines of vertices with normals (MSMS .vert, msroll .c3p). */
static int ReadVertices( Reader *r, const char *path, MeshData *m, char *err, int errlen )
{
    char line[MESH_LINE];
    double v[6], len;
    long i;
    int status;

    m->xyz = (double*)malloc(3*(size_t)m->nvert*sizeof(double));
    m->nrm = (double*)malloc(3*(size_t)m->nvert*sizeof(double));
    if( !m->xyz || !m->nrm )
    {   snprintf(err,errlen,"out of memory for %ld vertices",m->nvert);
        return 1;
    }
    for( i=0; i<m->nvert; i++ )
    {   status = ReadLine(r,line,sizeof(line));
        if( status <= 0 || LineNumbers(line,v,6) != 6 )
        {   snprintf(err,errlen,"'%s' line %ld: expected vertex %ld of %ld (x y z nx ny nz)",
                     path,r->line-(status>0),i+1,m->nvert);
            return 1;
        }
        m->xyz[3*i] = v[0];  m->xyz[3*i+1] = v[1];  m->xyz[3*i+2] = v[2];
        len = sqrt(v[3]*v[3]+v[4]*v[4]+v[5]*v[5]);
        if( len <= 0.0 ) { v[3] = v[4] = 0.0;  v[5] = len = 1.0; }
        m->nrm[3*i] = v[3]/len;  m->nrm[3*i+1] = v[4]/len;  m->nrm[3*i+2] = v[5]/len;
    }
    return 0;
}

/* Reads "ntri" triangle lines; the 1-based vertex numbers are in columns
   first..first+2. */
static int ReadTriangles( Reader *r, const char *path, MeshData *m, int first,
                          char *err, int errlen )
{
    char line[MESH_LINE];
    double v[9];
    long i, k;
    int status, j;

    m->tri = (long*)malloc(3*(size_t)m->ntri*sizeof(long));
    if( !m->tri )
    {   snprintf(err,errlen,"out of memory for %ld triangles",m->ntri);
        return 1;
    }
    for( i=0; i<m->ntri; i++ )
    {   status = ReadLine(r,line,sizeof(line));
        if( status <= 0 || LineNumbers(line,v,first+3) != first+3 )
        {   snprintf(err,errlen,"'%s' line %ld: expected triangle %ld of %ld",
                     path,r->line-(status>0),i+1,m->ntri);
            return 1;
        }
        for( j=0; j<3; j++ )
        {   k = (long)v[first+j];
            if( k < 1 || k > m->nvert || (double)k != v[first+j] )
            {   snprintf(err,errlen,"'%s' line %ld: vertex number %g out of range 1..%ld",
                         path,r->line-1,v[first+j],m->nvert);
                return 1;
            }
            m->tri[3*i+j] = k-1;
        }
    }
    return 0;
}

/* The path with its extension replaced (or added). */
static void SwapExtension( const char *path, const char *ext, char *out, int max )
{
    const char *dot = strrchr(path,'.');
    const char *sep = strrchr(path,'/');
    const char *bsl = strrchr(path,'\\');
    int n;

    if( bsl > sep ) sep = bsl;
    n = (dot && (!sep || dot > sep)) ? (int)(dot-path) : (int)strlen(path);
    snprintf(out,max,"%.*s%s",n,path,ext);
}

/* msroll's .c3v next to the .c3p: "molecular area = A" and
   "solvent-excluded volume = V". */
static void ReadC3V( const char *path, MeshData *m )
{
    char name[1024], line[MESH_LINE];
    const char *eq;
    Reader r;

    SwapExtension(path,".c3v",name,sizeof(name));
    if( !ReadOpen(&r,name) ) return;
    while( ReadLine(&r,line,sizeof(line)) > 0 )
    {   if( !(eq = strchr(line,'=')) ) continue;
        if( !strncmp(line,"molecular area",14) )
            m->filearea = atof(eq+1);
        else if( !strncmp(line,"solvent-excluded volume",23) )
            m->filevolume = atof(eq+1);
    }
    ReadClose(&r);
}

static int ReadC3P( const char *path, MeshData *m, char *err, int errlen )
{
    char line[MESH_LINE];
    double v[3];
    long i, nedge;
    Reader r;
    int status;

    if( !ReadOpen(&r,path) )
    {   snprintf(err,errlen,"cannot open '%s'",path);
        return 1;
    }
    if( ReadLine(&r,line,sizeof(line)) <= 0 || LineNumbers(line,v,3) != 3
        || v[0] < 3 || v[2] < 1 || v[1] < 0 )
    {   ReadClose(&r);
        snprintf(err,errlen,"'%s': first line is not \"nvert nedge ntri\"",path);
        return 1;
    }
    m->nvert = (long)v[0];
    nedge = (long)v[1];
    m->ntri = (long)v[2];
    if( ReadVertices(&r,path,m,err,errlen) )
    {   ReadClose(&r);
        return 1;
    }
    for( i=0; i<nedge; i++ )
        if( (status = ReadLine(&r,line,sizeof(line))) <= 0 )
        {   ReadClose(&r);
            snprintf(err,errlen,"'%s' ends in the edge list (%ld of %ld)",path,i,nedge);
            return 1;
        }
    status = ReadTriangles(&r,path,m,3,err,errlen);
    ReadClose(&r);
    if( !status ) ReadC3V(path,m);
    return status;
}

/* Skips MSMS comment lines and reads the count line. */
static int MSMSHeader( Reader *r, char *line, int max, long *count )
{
    double v[1];
    int status;

    while( (status = ReadLine(r,line,max)) > 0 )
        if( line[0] != '#' && !BlankLine(line) ) break;
    if( status <= 0 || LineNumbers(line,v,1) != 1 || v[0] < 1 ) return 0;
    *count = (long)v[0];
    return 1;
}

static int ReadMSMS( const char *path, MeshData *m, char *err, int errlen )
{
    char vert[1024], face[1024], line[MESH_LINE];
    Reader r;
    int status;

    SwapExtension(path,".vert",vert,sizeof(vert));
    SwapExtension(path,".face",face,sizeof(face));

    if( !ReadOpen(&r,vert) )
    {   snprintf(err,errlen,"cannot open '%s'",vert);
        return 1;
    }
    if( !MSMSHeader(&r,line,sizeof(line),&m->nvert) || m->nvert < 3 )
    {   ReadClose(&r);
        snprintf(err,errlen,"'%s': no vertex count after the # lines",vert);
        return 1;
    }
    status = ReadVertices(&r,vert,m,err,errlen);
    ReadClose(&r);
    if( status ) return 1;

    if( !ReadOpen(&r,face) )
    {   snprintf(err,errlen,"cannot open '%s'",face);
        return 1;
    }
    if( !MSMSHeader(&r,line,sizeof(line),&m->ntri) )
    {   ReadClose(&r);
        snprintf(err,errlen,"'%s': no face count after the # lines",face);
        return 1;
    }
    status = ReadTriangles(&r,face,m,0,err,errlen);
    ReadClose(&r);
    return status;
}

/* ---------------------------------------------------------------------
 * Choosing the format: by extension, otherwise by the first two lines.
 * --------------------------------------------------------------------- */

static int HasExtension( const char *path, const char *ext )
{
    size_t lp = strlen(path), le = strlen(ext), i;

    if( lp <= le ) return 0;
    for( i=0; i<le; i++ )
        if( tolower((unsigned char)path[lp-le+i]) != ext[i] ) return 0;
    return 1;
}

static int SniffFormat( const char *path, char *err, int errlen )
{
    char line[MESH_LINE];
    double v[12];
    int n1 = -1, n2 = -1, status;
    Reader r;

    if( !ReadOpen(&r,path) )
    {   snprintf(err,errlen,"cannot open '%s'",path);
        return 0;
    }
    while( (status = ReadLine(&r,line,sizeof(line))) > 0 )
    {   const char *p = line;
        if( BlankLine(line) ) continue;
        while( isspace((unsigned char)*p) ) p++;
        if( *p == '#' )
        {   ReadClose(&r);
            snprintf(err,errlen,"'%s' looks like MSMS; name it .vert or .face",path);
            return 0;
        }
        if( isalpha((unsigned char)*p) )
        {   ReadClose(&r);
            return MESH_BOD;
        }
        if( n1 < 0 ) n1 = LineNumbers(line,v,12);
        else
        {   n2 = LineNumbers(line,v,12);
            break;
        }
    }
    ReadClose(&r);
    if( n1 == 3 && n2 == 9 ) return MESH_BEST;
    if( n1 == 3 && n2 >= 10 ) return MESH_C3P;
    snprintf(err,errlen,"'%s' is not a BEST, .c3p, MSMS or .bod mesh",path);
    return 0;
}

const char *MeshFormatName( int format )
{
    switch( format )
    {   case MESH_BEST: return "BEST";
        case MESH_C3P:  return "msroll c3p";
        case MESH_MSMS: return "MSMS";
        case MESH_BOD:  return "ZENO bod";
    }
    return "unknown";
}

void MeshFree( MeshData *m )
{
    free(m->xyz);
    free(m->nrm);
    free(m->tri);
    m->xyz = m->nrm = NULL;
    m->tri = NULL;
    m->nvert = m->ntri = 0;
}

int MeshRead( const char *path, MeshData *m, char *err, int errlen )
{
    int status;

    memset(m,0,sizeof(*m));
    m->filearea = m->filevolume = -1.0;

    if( HasExtension(path,".vert") || HasExtension(path,".face") )
        m->format = MESH_MSMS;
    else if( HasExtension(path,".c3p") )
        m->format = MESH_C3P;
    else if( HasExtension(path,".bod") )
        m->format = MESH_BOD;
    else if( HasExtension(path,".best") )
        m->format = MESH_BEST;
    else if( !(m->format = SniffFormat(path,err,errlen)) )
        return 1;

    switch( m->format )
    {   case MESH_BEST: status = ReadBEST(path,m,err,errlen); break;
        case MESH_C3P:  status = ReadC3P(path,m,err,errlen);  break;
        case MESH_MSMS: status = ReadMSMS(path,m,err,errlen); break;
        default:        status = ReadBOD(path,m,err,errlen);  break;
    }
    if( status ) MeshFree(m);
    return status;
}

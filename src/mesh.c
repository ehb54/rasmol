/*
 * mesh.c -- triangle meshes read from files, shown as maps (see mesh.h).
 *
 * A mesh is stored the way a contoured map is: MapPoint vertices (RasMol
 * units, outward normals x4095, a colour each), MapTangle triangles and
 * MapBond edges.  DisplayMeshMaps() draws the maps marked MapMeshFileFlag;
 * the grid map functions in repres.c skip them.  It differs from those
 * functions in three ways that matter for meshes of a million triangles:
 *
 *  - each vertex is transformed and lit once per frame, not once for every
 *    triangle corner and edge end that uses it;
 *  - on a closed mesh, a triangle whose three vertices face away from the
 *    viewer is not drawn, unless the slab plane is on (then the inside of
 *    the mesh shows through the cut, and is lit as if it faced the viewer);
 *  - a triangle that falls inside one pixel is plotted as that pixel.
 */

#include "rasmol.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>

#include "molecule.h"
#include "graphics.h"
#include "repres.h"
#include "render.h"
#include "command.h"
#include "cmndline.h"
#include "abstree.h"
#include "transfor.h"
#include "pixutils.h"
#include "langsel.h"
#include "maps.h"
#include "multiple.h"
#include "meshio.h"
#include "mesh.h"


/*==========================*/
/*  Loading                 */
/*==========================*/

static int MeshLoadError( const char *msg )
{
    CommandError((char*)NULL);
    WriteString("Mesh: ");
    WriteString((char*)msg);
    WriteString("\n");
    return 1;
}

int LoadMeshMap( char *path, int info )
{
    MeshData mesh;
    MapInfo mapinfo, *other;
    MapPoint mp;
    MapTangle mt;
    MapBond mb;
    char err[1200], buffer[1400];
    double *fn, *nrm;
    double area = 0.0, volume = 0.0, sign;
    unsigned long long *ekey;
    unsigned char *ecount;
    size_t hsize, hmask, h;
    long i, nedge = 0, nboundary = 0, nbad = 0;
    int shade, col, j, k;

    if( MeshRead(path,&mesh,err,sizeof(err)) )
        return MeshLoadError(err);

    /* Area, enclosed volume and area-weighted face normals, in the file's
       frame.  The sign of the volume tells which way the triangles turn. */
    fn = (double*)calloc(3*(size_t)mesh.nvert,sizeof(double));
    if( !fn )
    {   MeshFree(&mesh);
        return MeshLoadError("out of memory");
    }
    for( i=0; i<mesh.ntri; i++ )
    {   double *a = mesh.xyz+3*mesh.tri[3*i];
        double *b = mesh.xyz+3*mesh.tri[3*i+1];
        double *c = mesh.xyz+3*mesh.tri[3*i+2];
        double u0 = b[0]-a[0], u1 = b[1]-a[1], u2 = b[2]-a[2];
        double w0 = c[0]-a[0], w1 = c[1]-a[1], w2 = c[2]-a[2];
        double f0 = u1*w2-u2*w1, f1 = u2*w0-u0*w2, f2 = u0*w1-u1*w0;

        area += 0.5*sqrt(f0*f0+f1*f1+f2*f2);
        volume += ( a[0]*(b[1]*c[2]-b[2]*c[1]) + a[1]*(b[2]*c[0]-b[0]*c[2])
                  + a[2]*(b[0]*c[1]-b[1]*c[0]) )/6.0;
        for( k=0; k<3; k++ )
        {   long v = mesh.tri[3*i+k];
            fn[3*v] += f0;  fn[3*v+1] += f1;  fn[3*v+2] += f2;
        }
    }
    sign = (volume < 0.0) ? -1.0 : 1.0;
    volume = fabs(volume);

    if( mesh.nrm )
    {   /* Keep the file's normals, unless most of them point inwards. */
        long vote = 0;
        for( i=0; i<mesh.nvert; i++ )
        {   double d = sign*( fn[3*i]*mesh.nrm[3*i] + fn[3*i+1]*mesh.nrm[3*i+1]
                            + fn[3*i+2]*mesh.nrm[3*i+2] );
            vote += (d > 0.0) - (d < 0.0);
        }
        if( vote < 0 )
            for( i=0; i<3*mesh.nvert; i++ ) mesh.nrm[i] = -mesh.nrm[i];
        nrm = mesh.nrm;
    } else
    {   for( i=0; i<mesh.nvert; i++ )
        {   double len = sqrt( fn[3*i]*fn[3*i] + fn[3*i+1]*fn[3*i+1]
                             + fn[3*i+2]*fn[3*i+2] );
            if( len > 0.0 )
            {   fn[3*i] *= sign/len;  fn[3*i+1] *= sign/len;  fn[3*i+2] *= sign/len;
            }
        }
        nrm = fn;
    }

    /* The new map: selected, and the only selected one. */
    if( !MapInfoPtr ) InitialiseMaps();
    for( j=0; j<(int)MapInfoPtr->size; j++ )
    {   vector_get_elementptr((GenericVec __far *)MapInfoPtr,(void __far * __far *)&other,j);
        other->flag &= ~MapSelectFlag;
    }

    memset(&mapinfo,0,sizeof(mapinfo));
    mapinfo.MapLevel = MapLevel;
    mapinfo.MapSpacing = (MapSpacing > 0) ? MapSpacing : 250L;
    mapinfo.MapSpread = MapSpread;
    mapinfo.flag = MapSelectFlag|MapSurfFlag|MapMeshFileFlag;
    mapinfo.MapRGBCol[0] = MapRGBCol[0];
    mapinfo.MapRGBCol[1] = MapRGBCol[1];
    mapinfo.MapRGBCol[2] = MapRGBCol[2];
    mapinfo.MapMeshRad = MapMeshRad;
    mapinfo.MapPointRad = MapPointRad;
    mapinfo.MapLabel = MapLabel;
    MapLabel = NULL;
    mapinfo.MapFile = (char*)_fmalloc(strlen(path)+1);
    if( mapinfo.MapFile ) strcpy(mapinfo.MapFile,path);
    mapinfo.MeshArea = area;
    mapinfo.MeshVolume = volume;
    mapinfo.MeshFileArea = mesh.filearea;
    mapinfo.MeshFileVolume = mesh.filevolume;
    mapinfo.MeshFormat = mesh.format;

    if( !mapinfo.MapFile
        || vector_create((GenericVec __far **)&mapinfo.MapPointsPtr,sizeof(MapPoint),mesh.nvert)
        || vector_create((GenericVec __far **)&mapinfo.MapTanglePtr,sizeof(MapTangle),mesh.ntri)
        || vector_create((GenericVec __far **)&mapinfo.MapBondsPtr,sizeof(MapBond),(3*mesh.ntri)/2+16) )
    {   if( mapinfo.MapPointsPtr ) vector_free((GenericVec __far * __far *)&mapinfo.MapPointsPtr);
        if( mapinfo.MapTanglePtr ) vector_free((GenericVec __far * __far *)&mapinfo.MapTanglePtr);
        if( mapinfo.MapFile ) _ffree(mapinfo.MapFile);
        free(fn);
        MeshFree(&mesh);
        return MeshLoadError("out of memory");
    }

    shade = DefineShade(mapinfo.MapRGBCol[0],mapinfo.MapRGBCol[1],mapinfo.MapRGBCol[2]);
    col = Shade2Colour(shade);

    /* Vertices in RasMol's frame: 250 units per Angstrom, z reversed, and
       centred as the molecule's atoms are. */
    mp.next = 0;
    mp.col = col;
    mp.flag = SelectFlag;
    mp.coordnum = 0;
    for( i=0; i<mesh.nvert; i++ )
    {   mp.xpos = (Long)rint( 250.0*mesh.xyz[3*i])   - OrigCX;
        mp.ypos = (Long)rint( 250.0*mesh.xyz[3*i+1]) - OrigCY;
        mp.zpos = (Long)rint(-250.0*mesh.xyz[3*i+2]) - OrigCZ;
        mp.Un[0] = (Long)rint( 4095.0*nrm[3*i]);
        mp.Un[1] = (Long)rint( 4095.0*nrm[3*i+1]);
        mp.Un[2] = (Long)rint(-4095.0*nrm[3*i+2]);
        vector_add_element((GenericVec __far *)mapinfo.MapPointsPtr,(void __far *)&mp);
        Shade[shade].refcount++;
    }
    Shade[shade].refcount += 2;   /* the triangles and the edges, as map_points */
    free(fn);

    /* Triangles, and each edge once (counting the triangles on it). */
    hsize = 1;
    while( hsize < 4*(size_t)mesh.ntri ) hsize <<= 1;
    hmask = hsize-1;
    ekey = (unsigned long long*)calloc(hsize,sizeof(unsigned long long));
    ecount = (unsigned char*)calloc(hsize,1);
    if( !ekey || !ecount )
    {   /* no edges: the mesh can still be drawn as a surface or dots */
        free(ekey);
        free(ecount);
        ekey = NULL;
        ecount = NULL;
    }
    mt.col = col;  mt.flag = SelectFlag;
    mb.col = col;  mb.flag = SelectFlag;
    for( i=0; i<mesh.ntri; i++ )
    {   mt.points[0] = (size_t)mesh.tri[3*i];
        mt.points[1] = (size_t)mesh.tri[3*i+1];
        mt.points[2] = (size_t)mesh.tri[3*i+2];
        vector_add_element((GenericVec __far *)mapinfo.MapTanglePtr,(void __far *)&mt);
        if( !ekey ) continue;
        for( k=0; k<3; k++ )
        {   unsigned long long a = (unsigned long long)mesh.tri[3*i+k];
            unsigned long long b = (unsigned long long)mesh.tri[3*i+(k+1)%3];
            unsigned long long key;
            if( a > b ) { key = a;  a = b;  b = key; }
            key = ((a<<32)|b) + 1;
            h = (size_t)((key*0x9E3779B97F4A7C15ULL) >> 20) & hmask;
            while( ekey[h] && ekey[h] != key ) h = (h+1) & hmask;
            if( !ekey[h] )
            {   ekey[h] = key;
                mb.src = (size_t)a;
                mb.dst = (size_t)b;
                vector_add_element((GenericVec __far *)mapinfo.MapBondsPtr,(void __far *)&mb);
                nedge++;
            }
            if( ecount[h] < 3 ) ecount[h]++;
        }
    }
    if( ekey )
    {   for( h=0; h<hsize; h++ )
        {   if( ecount[h] == 1 ) nboundary++;
            else if( ecount[h] > 2 ) nbad++;
        }
    }
    free(ekey);
    free(ecount);

    mapinfo.MeshBoundary = nboundary;
    mapinfo.MeshNonManifold = nbad;
    if( ekey && !nboundary && !nbad ) mapinfo.flag |= MapMeshClosedFlag;

    vector_add_element((GenericVec __far *)MapInfoPtr,(void __far *)&mapinfo);

    if( info )
    {   sprintf(buffer,"Mesh %s (%s): %ld triangles, %ld vertices, %ld edges, ",
                path,MeshFormatName(mesh.format),mesh.ntri,mesh.nvert,nedge);
        WriteString(buffer);
        if( mapinfo.flag & MapMeshClosedFlag )
            sprintf(buffer,"closed\n");
        else sprintf(buffer,"not closed (%ld edges on one triangle, %ld on more than two)\n",
                     nboundary,nbad);
        WriteString(buffer);
        sprintf(buffer,"Mesh area %.3f A^2, volume %.3f A^3\n",area,volume);
        WriteString(buffer);
        if( mesh.skipped )
        {   sprintf(buffer,"Mesh: %ld bodies other than triangles are not shown\n",mesh.skipped);
            WriteString(buffer);
        }
    }
    MeshFree(&mesh);

    MapReRadius();
    ReRadius();
    ReDrawFlag |= RFInitial|RFColour;
    return 0;
}


/*==========================*/
/*  map show, write script  */
/*==========================*/

void ShowMeshMap( MapInfo *mapinfo )
{
    char buffer[1400];

    if( !mapinfo->MapPointsPtr || !mapinfo->MapTanglePtr )
    {   WriteString("mesh:   deleted\n");
        return;
    }
    sprintf(buffer,"mesh:   %s (%s)\n",mapinfo->MapFile?mapinfo->MapFile:"",
            MeshFormatName(mapinfo->MeshFormat));
    WriteString(buffer);
    sprintf(buffer,"mesh:   %lu triangles, %lu vertices, %lu edges, ",
            (unsigned long)mapinfo->MapTanglePtr->size,
            (unsigned long)mapinfo->MapPointsPtr->size,
            (unsigned long)(mapinfo->MapBondsPtr?mapinfo->MapBondsPtr->size:0));
    WriteString(buffer);
    if( mapinfo->flag & MapMeshClosedFlag )
        sprintf(buffer,"closed\n");
    else sprintf(buffer,"not closed (%ld edges on one triangle, %ld on more than two)\n",
                 (long)mapinfo->MeshBoundary,(long)mapinfo->MeshNonManifold);
    WriteString(buffer);
    sprintf(buffer,"mesh:   area %.3f A^2, volume %.3f A^3",
            mapinfo->MeshArea,mapinfo->MeshVolume);
    WriteString(buffer);
    if( mapinfo->MeshFileArea >= 0.0 )
    {   sprintf(buffer,"; the file states area %.3f, volume %.3f",
                mapinfo->MeshFileArea,mapinfo->MeshFileVolume);
        WriteString(buffer);
    }
    WriteString("\n");
}

void WriteScriptMeshMap( FILE *fp, MapInfo *mapinfo )
{
    if( !mapinfo->MapFile || !mapinfo->MapPointsPtr ) return;

    fprintf(fp,"load mesh \"%s\"\n",mapinfo->MapFile);
    if( mapinfo->flag & MapSurfFlag )       fprintf(fp,"map surface\n");
    else if( mapinfo->flag & MapMeshFlag )  fprintf(fp,"map mesh\n");
    else if( mapinfo->flag & MapPointFlag ) fprintf(fp,"map dots\n");
    if( mapinfo->flag & MapColourAtom )     fprintf(fp,"map colour atom\n");
    else if( mapinfo->flag & MapColourPot ) fprintf(fp,"map colour potential\n");
    else fprintf(fp,"map colour [%d,%d,%d]\n",
                 mapinfo->MapRGBCol[0],mapinfo->MapRGBCol[1],mapinfo->MapRGBCol[2]);
}


/*==========================*/
/*  Drawing                 */
/*==========================*/

/* Per-vertex screen position, lighting (0..ColourMask) and facing, for the
   mesh being drawn. */
static int *MeshX, *MeshY, *MeshZ, *MeshI;
static signed char *MeshFront;
static size_t MeshCap;

static int MeshScratch( size_t n )
{
    if( n <= MeshCap ) return 1;
    free(MeshX); free(MeshY); free(MeshZ); free(MeshI); free(MeshFront);
    MeshX = (int*)malloc(n*sizeof(int));
    MeshY = (int*)malloc(n*sizeof(int));
    MeshZ = (int*)malloc(n*sizeof(int));
    MeshI = (int*)malloc(n*sizeof(int));
    MeshFront = (signed char*)malloc(n);
    if( !MeshX || !MeshY || !MeshZ || !MeshI || !MeshFront )
    {   free(MeshX); free(MeshY); free(MeshZ); free(MeshI); free(MeshFront);
        MeshX = MeshY = MeshZ = MeshI = NULL;
        MeshFront = NULL;
        MeshCap = 0;
        return 0;
    }
    MeshCap = n;
    return 1;
}

/* A triangle whose corners differ in colour: as DisplayMapTangles does,
   it is drawn as up to six triangles of one colour each, meeting at the
   centre and at the midpoints of the edges whose ends differ. */
static void ClipSplitTangle( Poly *t, const int *col, const int *in )
{
    Poly p;
    int cx, cy, cz, ic, mx, my, mz, im, k, n;

    cx = (t->v[0].x+t->v[1].x+t->v[2].x)/3;
    cy = (t->v[0].y+t->v[1].y+t->v[2].y)/3;
    cz = (t->v[0].z+t->v[1].z+t->v[2].z)/3;
    ic = (in[0]+in[1]+in[2]+2)/3;
    p.count = 3;
    for( k=0; k<3; k++ )
    {   n = (k+1)%3;
        p.v[0] = t->v[k];
        p.v[0].inten = col[k]+in[k];
        p.v[2].x = cx;  p.v[2].y = cy;  p.v[2].z = cz;
        p.v[2].inten = col[k]+ic;
        if( col[k] == col[n] )
        {   p.v[1] = t->v[n];
            p.v[1].inten = col[k]+in[n];
            ClipPolygon(&p);
        } else
        {   mx = (t->v[k].x+t->v[n].x)/2;
            my = (t->v[k].y+t->v[n].y)/2;
            mz = (t->v[k].z+t->v[n].z)/2;
            im = (in[k]+in[n]+1)/2;
            p.v[1].x = mx;  p.v[1].y = my;  p.v[1].z = mz;
            p.v[1].inten = col[k]+im;
            ClipPolygon(&p);
            p.v[0] = t->v[n];
            p.v[0].inten = col[n]+in[n];
            p.v[1].inten = col[n]+im;
            p.v[2].inten = col[n]+ic;
            ClipPolygon(&p);
        }
    }
}

void DisplayMeshMaps( void )
{
    MapInfo mapinfo;
    MapPointVec __far *P;
    MapTangleVec __far *T;
    MapBondVec __far *B;
    Poly tangle;
    int Cenx, Ceny, Cenz, cull, irad, inten, j, k;
    size_t i, np;

    if( !MapInfoPtr ) return;

    Cenx = (int)rint(CenX*MatX[0]+CenY*MatX[1]+CenZ*MatX[2]);
    Ceny = (int)rint(CenX*MatY[0]+CenY*MatY[1]+CenZ*MatY[2]);
    Cenz = (int)rint(CenX*MatZ[0]+CenY*MatZ[1]+CenZ*MatZ[2]);

    for( j=0; j<(int)MapInfoPtr->size; j++ )
    {   vector_get_element((GenericVec __far *)MapInfoPtr,(void __far *)&mapinfo,j);
        if( !(mapinfo.flag & MapMeshFileFlag) ) continue;
        P = mapinfo.MapPointsPtr;
        if( !P || !P->size || !(mapinfo.flag & (MapPointFlag|MapMeshFlag|MapSurfFlag)) )
            continue;
        np = P->size;
        if( !MeshScratch(np) ) return;

        for( i=0; i<np; i++ )
        {   MapPoint __far *p = &P->array[i];
            Real x = (Real)p->xpos, y = (Real)p->ypos, z = (Real)p->zpos;
            Long n0, n1, n2;

            MeshX[i] = (int)rint(x*MatX[0]+y*MatX[1]+z*MatX[2])+XOffset-Cenx;
            MeshY[i] = (int)rint(x*MatY[0]+y*MatY[1]+z*MatY[2])+YOffset-Ceny;
            MeshZ[i] = (int)rint(x*MatZ[0]+y*MatZ[1]+z*MatZ[2])+ZOffset-Cenz;
            n0 = (Long)(rint(p->Un[0]*RotX[0]+p->Un[1]*RotX[1]+p->Un[2]*RotX[2])/32.);
            n1 = (Long)(rint(p->Un[0]*RotY[0]+p->Un[1]*RotY[1]+p->Un[2]*RotY[2])/32.);
            n2 = (Long)(rint(p->Un[0]*RotZ[0]+p->Un[1]*RotZ[1]+p->Un[2]*RotZ[2])/32.);
            MeshFront[i] = (n2 >= 0);
            if( n2 < 0 ) { n0 = -n0;  n1 = -n1;  n2 = -n2; }
            inten = (int)LightDot(n0,n1,n2);
            if( inten > 0 )
            {   inten = ((inten*colconst(128))>>ColBits);
                if( inten > ColourMask ) inten = ColourMask;
            } else inten = 0;
            MeshI[i] = inten;
        }

        if( mapinfo.flag & MapPointFlag )
        {   irad = (int)rint(Scale*(Real)(mapinfo.MapPointRad));
            for( i=0; i<np; i++ )
                if( XValid(MeshX[i]) && YValid(MeshY[i]) && ZBValid(MeshX[i],MeshZ[i]) )
                {   if( irad <= 0 )
                        PlotDeepPoint(MeshX[i],MeshY[i],MeshZ[i],P->array[i].col);
                    else ClipSphere(MeshX[i],MeshY[i],MeshZ[i],irad,P->array[i].col);
                }
        }

        B = mapinfo.MapBondsPtr;
        if( B && (mapinfo.flag & MapMeshFlag) )
        {   irad = (int)rint(Scale*(Real)(mapinfo.MapMeshRad));
            for( i=0; i<B->size; i++ )
            {   size_t s = B->array[i].src, d = B->array[i].dst;
                int cs = P->array[s].col, cd = P->array[d].col;
                if( irad > 0 )
                    ClipCylinder(MeshX[s],MeshY[s],MeshZ[s],MeshX[d],MeshY[d],MeshZ[d],
                                 cs,cd,irad,' ',' ');
                else if( (mapinfo.flag&MapMeshDashFlag) == MapMeshDashFlag )
                    ClipDashVector(MeshX[s],MeshY[s],MeshZ[s],MeshX[d],MeshY[d],MeshZ[d],
                                   cs,cd,' ');
                else ClipTwinVector(MeshX[s],MeshY[s],MeshZ[s],MeshX[d],MeshY[d],MeshZ[d],
                                    cs,cd,' ');
            }
        }

        T = mapinfo.MapTanglePtr;
        if( T && (mapinfo.flag & MapSurfFlag) )
        {   cull = (mapinfo.flag & MapMeshClosedFlag) && !UseSlabPlane;
            tangle.count = 3;
            for( i=0; i<T->size; i++ )
            {   size_t v[3];
                int col[3], in[3];

                v[0] = T->array[i].points[0];
                v[1] = T->array[i].points[1];
                v[2] = T->array[i].points[2];
                if( cull && !MeshFront[v[0]] && !MeshFront[v[1]] && !MeshFront[v[2]] )
                    continue;
                for( k=0; k<3; k++ )
                {   col[k] = P->array[v[k]].col;
                    in[k] = MeshI[v[k]];
                }
                if( col[0]==col[1] && col[0]==col[2]
                    && MeshX[v[0]]==MeshX[v[1]] && MeshX[v[0]]==MeshX[v[2]]
                    && MeshY[v[0]]==MeshY[v[1]] && MeshY[v[0]]==MeshY[v[2]] )
                {   int z = MeshZ[v[0]];
                    if( MeshZ[v[1]] > z ) z = MeshZ[v[1]];
                    if( MeshZ[v[2]] > z ) z = MeshZ[v[2]];
                    ClipPoint(MeshX[v[0]],MeshY[v[0]],z,col[0]+in[0]);
                    continue;
                }
                for( k=0; k<3; k++ )
                {   tangle.v[k].x = MeshX[v[k]];
                    tangle.v[k].y = MeshY[v[k]];
                    tangle.v[k].z = MeshZ[v[k]];
                    tangle.v[k].inten = col[k]+in[k];
                }
                if( col[0]==col[1] && col[0]==col[2] )
                    ClipPolygon(&tangle);
                else ClipSplitTangle(&tangle,col,in);
            }
        }
    }
}

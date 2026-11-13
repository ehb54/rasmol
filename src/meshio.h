/*
 * meshio.h -- read triangulated surface meshes from files.
 *
 * Reads the closed surface meshes used for boundary-element and ZENO
 * hydrodynamics, so that RasMol can draw them with the molecule:
 *
 *   BEST  "N area volume", then N lines of the nine coordinates of each
 *         triangle (written by rcoal as <name>_<N>, and by sesmesh)
 *   C3P   msroll's .c3p: "nvert nedge ntri", then vertices with normals,
 *         edges, and triangles (three edge numbers, then three 1-based
 *         vertex numbers); the area and volume come from the .c3v next to it
 *   MSMS  .vert (vertices with normals) and .face (1-based vertex numbers)
 *   BOD   ZENO's .bod; TRIANGLE lines are read, other bodies are skipped
 *
 * Plain C with no RasMol dependencies.  Triangle lists (BEST, BOD) are joined
 * into shared vertices where the coordinates are identical.  Coordinates are
 * returned in Angstroms; only .bod files state their units.
 */
#ifndef RASMOL_MESHIO_H
#define RASMOL_MESHIO_H

#define MESH_BEST 1
#define MESH_C3P  2
#define MESH_MSMS 3
#define MESH_BOD  4

typedef struct {
    int format;            /* MESH_BEST, MESH_C3P, MESH_MSMS or MESH_BOD */
    long nvert;            /* number of vertices                         */
    long ntri;             /* number of triangles                        */
    double *xyz;           /* 3*nvert coordinates, Angstroms             */
    double *nrm;           /* 3*nvert unit normals from the file, or NULL */
    long *tri;             /* 3*ntri vertex numbers, from 0              */
    double filearea;       /* area stated in the file, or -1             */
    double filevolume;     /* volume stated in the file, or -1           */
    long skipped;          /* .bod bodies that are not triangles         */
} MeshData;

/* Reads the mesh in path.  Returns 0, or 1 with a message in err. */
int MeshRead( const char *path, MeshData *mesh, char *err, int errlen );
void MeshFree( MeshData *mesh );
const char *MeshFormatName( int format );

#endif

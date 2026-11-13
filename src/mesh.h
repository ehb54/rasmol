/*
 * mesh.h -- triangle meshes read from files, shown as maps.
 *
 * "load mesh <file>" reads a surface mesh (see meshio.h) into a new map of
 * the current molecule.  The map has no grid (MapPtr is NULL) and is marked
 * MapMeshFileFlag, so the map commands that only change its appearance work
 * on it -- map surface, map mesh, map dots, map colour, map select, map zap,
 * map show -- while the commands that recontour a grid leave it alone.
 */
#ifndef RASMOL_MESH_H
#define RASMOL_MESH_H

int LoadMeshMap( char *path, int info );
void DisplayMeshMaps( void );
void ShowMeshMap( MapInfo *mapinfo );
void WriteScriptMeshMap( FILE *fp, MapInfo *mapinfo );

#endif

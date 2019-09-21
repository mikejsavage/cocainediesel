/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2002-2013 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#pragma once

#include "qcommon/types.h"
#include "qcommon/qcommon.h"
#include "qcommon/patch.h"

typedef struct mempool_s mempool_t;

typedef u16 elem_t;

typedef vec_t instancePoint_t[8]; // quaternion for rotation + xyz pos + uniform scale

#include "r_math.h"

#define SUBDIVISIONS_MIN        3
#define SUBDIVISIONS_MAX        16
#define SUBDIVISIONS_DEFAULT    5

#define BACKFACE_EPSILON        4

#define ON_EPSILON              0.03125 // 1/32 to keep floating point happy

#define Z_NEAR                  4.0f
#define Z_BIAS                  64.0f

#define POLYOFFSET_FACTOR       -2.0f
#define POLYOFFSET_UNITS        -1.0f

#define SIDE_FRONT              0
#define SIDE_BACK               1
#define SIDE_ON                 2

#define RF_BIT( x )             ( 1ULL << ( x ) )

#define RF_NONE                 0
#define RF_DRAWFLAT             ( 1 << 0 )
#define RF_SOFT_PARTICLES       ( 1 << 1 )

#define MAX_REF_ENTITIES        ( MAX_ENTITIES + 48 ) // must not exceed 2048 because of sort key packing

#define BLUENOISE_TEXTURE_SIZE 128

#include "r_public.h"
#include "r_surface.h"
#include "r_model.h"
#include "r_program.h"

extern const elem_t r_boxedges[24];

//===================================================================

// cached for this frame for zero LOD
typedef struct {
	int mod_type;
	bool rotated;
	float radius;

	vec3_t mins, maxs;
	vec3_t absmins, absmaxs;
} entSceneCache_t;

typedef struct refScreenTexSet_s {
	image_t         *screenTex;
	image_t         *screenTexCopy;
	image_t         *screenPPCopies[2];
	image_t         *screenDepthTex;
	image_t         *screenDepthTexCopy;
	int multisampleTarget;                // multisample fbo
} refScreenTexSet_t;

typedef struct refinst_s {
	unsigned int clipFlags;
	unsigned int renderFlags;

	int renderTarget;                       // target framebuffer object
	bool multisampleDepthResolved;

	int scissor[4];
	int viewport[4];

	//
	// view origin
	//
	vec3_t viewOrigin;
	mat3_t viewAxis;
	cplane_t frustum[6];

	float nearClip, farClip;
	float polygonFactor, polygonUnits;

	vec3_t visMins, visMaxs;
	vec3_t pvsMins, pvsMaxs;
	float visFarClip;
	float hdrExposure;

	int viewcluster, viewarea;

	vec3_t pvsOrigin;
	cplane_t clipPlane;

	mat4_t objectMatrix;
	mat4_t cameraMatrix;

	mat4_t modelviewMatrix;
	mat4_t projectionMatrix;

	mat4_t cameraProjectionMatrix;                  // cameraMatrix * projectionMatrix
	mat4_t modelviewProjectionMatrix;               // modelviewMatrix * projectionMatrix

	refdef_t refdef;

	unsigned int numEntities;
	int *entities;
	uint8_t *entpvs;

	struct refinst_s *parent;

	refScreenTexSet_t *st;                  // points to either either a 8bit or a 16bit float set

	drawList_t      *meshlist;              // meshes to be rendered

	uint8_t			*pvs, *areabits;
} refinst_t;

extern cvar_t *r_drawentities;
extern cvar_t *r_drawworld;
extern cvar_t *r_speeds;
extern cvar_t *r_sRGB;

extern cvar_t *r_subdivisions;
extern cvar_t *r_showtris;
extern cvar_t *r_showtris2D;
extern cvar_t *r_leafvis;

extern cvar_t *r_lighting_specular;
extern cvar_t *r_lighting_glossintensity;
extern cvar_t *r_lighting_glossexponent;
extern cvar_t *r_lighting_ambientscale;
extern cvar_t *r_lighting_directedscale;

extern cvar_t *r_outlines_world;
extern cvar_t *r_outlines_scale;
extern cvar_t *r_outlines_cutoff;

extern cvar_t *r_soft_particles;
extern cvar_t *r_soft_particles_scale;

extern cvar_t *r_hdr;
extern cvar_t *r_hdr_gamma;
extern cvar_t *r_hdr_exposure;

extern cvar_t *r_samples;

extern cvar_t *r_gamma;
extern cvar_t *r_texturefilter;
extern cvar_t *r_mode;
extern cvar_t *r_screenshot_fmtstr;

extern cvar_t *r_drawflat;
extern cvar_t *r_wallcolor;
extern cvar_t *r_floorcolor;

//====================================================================

void R_NormToLatLong( const vec_t *normal, uint8_t latlong[2] );
void R_LatLongToNorm( const uint8_t latlong[2], vec3_t out );
void R_LatLongToNorm4( const uint8_t latlong[2], vec4_t out );

//====================================================================

void Mod_LoadQ3BrushModel( model_t *mod, void *buffer, int buffer_size, const bspFormatDesc_t *format );
void Mod_LoadCompressedBSP( model_t *mod, void *buffer, int buffer_size, const bspFormatDesc_t *format );

//
// r_cmds.c
//
void R_TakeScreenShot( const char *path, const char *name, const char *fmtString, int x, int y, int w, int h, bool silent );
void R_ScreenShot_f( void );
void R_ImageList_f( void );
void R_ShaderList_f( void );
void R_ShaderDump_f( void );

//
// r_cull.c
//
void R_SetupFrustum( const refdef_t *rd, float nearClip, float farClip, cplane_t *frustum );
bool R_CullBoxCustomPlanes( const cplane_t *p, unsigned nump, const vec3_t mins, const vec3_t maxs, unsigned int clipflags );
bool R_CullSphereCustomPlanes( const cplane_t *p, unsigned nump, const vec3_t centre, const float radius, unsigned int clipflags );
bool R_CullBox( const vec3_t mins, const vec3_t maxs, const unsigned int clipflags );
bool R_CullSphere( const vec3_t centre, const float radius, const unsigned int clipflags );
bool R_VisCullBox( const vec3_t mins, const vec3_t maxs );
bool R_VisCullSphere( const vec3_t origin, float radius );
bool R_CullModelEntity( const entity_t *e, bool pvsCull );

//
// r_gltf
//

struct GLTFMesh;
void Mod_LoadGLTFModel( model_t * mod, void * buffer, int buffer_size, const bspFormatDesc_t * bsp_format );
void R_CacheGLTFModelEntity( const entity_t * e );
void R_AddGLTFModelToDrawList( const entity_t * e );
void R_DrawGLTFMesh( const entity_t * e, const shader_t * shader, const GLTFMesh * mesh );

//
// r_main.c
//
extern mempool_t *r_mempool;

#define R_Malloc( size ) R_Malloc_( size, __FILE__, __LINE__ )
#define R_Realloc( data, size ) Mem_Realloc( data, size )
#define R_Free( data ) Mem_Free( data )
#define R_AllocPool( parent, name ) Mem_AllocPool( parent, name )
#define R_FreePool( pool ) Mem_FreePool( pool )
#define R_MallocExt( pool, size, align, z ) _Mem_AllocExt( pool, size, align, z, 0, 0, __FILE__, __LINE__ )

ATTRIBUTE_MALLOC void * R_Malloc_( size_t size, const char *filename, int fileline );
char *R_CopyString_( const char *in, const char *filename, int fileline );
#define     R_CopyString( in ) R_CopyString_( in,__FILE__,__LINE__ )

int R_LoadFile_( const char *path, int flags, void **buffer, const char *filename, int fileline );
void R_FreeFile_( void *buffer, const char *filename, int fileline );

#define     R_LoadFile( path,buffer ) R_LoadFile_( path,0,buffer,__FILE__,__LINE__ )
#define     R_LoadCacheFile( path,buffer ) R_LoadFile_( path,FS_CACHE,buffer,__FILE__,__LINE__ )
#define     R_FreeFile( buffer ) R_FreeFile_( buffer,__FILE__,__LINE__ )

bool R_IsRenderingToScreen( void );
void R_BeginFrame();
void R_EndFrame( void );
void R_SetGamma( float gamma );
void R_SetWallFloorColors( const vec3_t wallColor, const vec3_t floorColor );
void R_SetupPVSFromCluster( int cluster, int area );
void R_SetupPVS( const refdef_t *fd );
void R_SetCameraAndProjectionMatrices( const mat4_t cam, const mat4_t proj );
void R_SetupViewMatrices( const refdef_t *rd );
void R_RenderView( const refdef_t *fd );
const msurface_t *R_GetDebugSurface( void );
const char *R_WriteSpeedsMessage( char *out, size_t size );
void R_RenderDebugSurface( const refdef_t *fd );
void R_Finish( void );
void R_Flush( void );

void R_Begin2D( bool multiSamples );
void R_SetupGL2D( void );
void R_End2D( void );

int R_MultisampleSamples( int samples );
int R_RegisterMultisampleTarget( refScreenTexSet_t *st, int samples, bool useFloat, bool sRGB );

void R_BlitTextureToScrFbo( const refdef_t *fd, image_t *image, int dstFbo,
	int program_type, const vec4_t color, int blendMask, int numShaderImages, image_t **shaderImages,
	int iParam0 );

void R_BatchSpriteSurf( const entity_t *e, const shader_t *shader, drawSurfaceType_t *drawSurf, bool mergable );

struct mesh_vbo_s *R_InitNullModelVBO( void );
void R_DrawNullSurf( const entity_t *e, const shader_t *shader, const drawSurfaceType_t *drawSurf );

void R_CacheSpriteEntity( const entity_t *e );

struct mesh_vbo_s *R_InitPostProcessingVBO( void );

void R_TransformForWorld( void );
void R_TransformForEntity( const entity_t *e );
void R_TranslateForEntity( const entity_t *e );

void R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2,
							  const vec4_t color, const shader_t *shader );
void R_DrawRotatedStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2,
									 float angle, const vec4_t color, const shader_t *shader );

void R_ClearRefInstStack( void );
refinst_t  *R_PushRefInst( void );
void R_PopRefInst( void );

void R_BindFrameBufferObject( int object );

void R_Scissor( int x, int y, int w, int h );
void R_ResetScissor( void );

//
// r_mesh.c
//
void R_InitDrawList( drawList_t *list );
void R_ClearDrawList( drawList_t *list );
void *R_AddSurfToDrawList( drawList_t *list, const entity_t *e, const shader_t *shader, float dist, unsigned int order, const void *drawSurf );
void R_ReserveDrawListWorldSurfaces( drawList_t *list );

void R_InitDrawLists( void );

void R_SortDrawList( drawList_t *list );
void R_DrawSurfaces( drawList_t *list );
void R_DrawOutlinedSurfaces( drawList_t *list );

void R_CopyOffsetElements( const elem_t *inelems, int numElems, int vertsOffset, elem_t *outelems );
void R_CopyOffsetTriangles( const elem_t *inelems, int numElems, int vertsOffset, elem_t *outelems );
void R_BuildTrifanElements( int vertsOffset, int numVerts, elem_t *elems );
void R_BuildTangentVectors( int numVertexes, vec4_t *xyzArray, vec4_t *normalsArray, vec2_t *stArray,
							int numTris, elem_t *elems, vec4_t *sVectorsArray );

//
// r_poly.c
//
void R_BatchPolySurf( const entity_t *e, const shader_t *shader, drawSurfacePoly_t *poly, bool mergable );
void R_DrawPolys( void );
bool R_SurfPotentiallyFragmented( const msurface_t *surf );
int R_GetClippedFragments( const vec3_t origin, float radius, vec3_t axis[3], int maxfverts,
								   vec4_t *fverts, int maxfragments, fragment_t *fragments );

//
// r_register.c
//
bool R_Init();
void R_BindGlobalVAO();
void R_BeginRegistration( void );
void R_EndRegistration( void );
void R_Shutdown( bool verbose );

//
// r_scene.c
//
extern drawList_t r_worldlist;

void R_AddDebugCorners( const vec3_t corners[8], const vec4_t color );
void R_AddDebugBounds( const vec3_t mins, const vec3_t maxs, const vec4_t color );
void R_ClearScene( void );
void R_AddEntityToScene( const entity_t *ent );
void R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void R_AddPolyToScene( const poly_t *poly );
void R_RenderScene( const refdef_t *fd );
void R_BlurScreen( void );

//
// r_surf.c
//
#define MAX_SURF_QUERIES        0x1E0

void R_DrawWorldNode( void );
bool R_SurfNoDraw( const msurface_t *surf );
bool R_SurfNoDlight( const msurface_t *surf );
void R_CacheBrushModelEntity( const entity_t *e );
bool R_AddBrushModelToDrawList( const entity_t *e );
float R_BrushModelBBox( const entity_t *e, vec3_t mins, vec3_t maxs, bool *rotated );
void R_BatchBSPSurf( const entity_t *e, const shader_t *shader, drawSurfaceBSP_t *drawSurf, bool mergable );
void R_FlushBSPSurfBatch( void );
void R_WalkBSPSurf( const entity_t *e, const shader_t *shader, drawSurfaceBSP_t *drawSurf, walkDrawSurf_cb_cb cb, void *ptr );

//
// r_sky.c
//

struct SkyDrawSurf;
void R_InitSky();
void R_AddSkyToDrawList( const refdef_t * rd );
void R_DrawSkyMesh( const entity_t * e, const shader_t * shader, const SkyDrawSurf * draw_surf );

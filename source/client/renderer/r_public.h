#pragma once

#include "qcommon/qcommon.h"
#include "cgame/ref.h"

typedef struct {
	bool ( *Init )( bool verbose );

	void ( *Shutdown )( bool verbose );

	void ( *RegisterWorldModel )( const char *model );
	struct model_s *( *RegisterModel )( const char *name );
	struct shader_s *( *RegisterPic )( const char *name );
	struct shader_s *( *RegisterAlphaMask )( const char *name, int width, int height, const uint8_t * data );
	struct shader_s *( *RegisterSkin )( const char *name );

	void ( *ReplaceRawSubPic )( struct shader_s *shader, int x, int y, int width, int height, uint8_t *data );

	void ( *ClearScene )( void );
	void ( *AddEntityToScene )( const entity_t *ent );
	void ( *AddLightToScene )( const vec3_t org, float intensity, float r, float g, float b );
	void ( *AddPolyToScene )( const poly_t *poly );
	void ( *RenderScene )( const refdef_t *fd );

	/**
	 * BlurScreen performs fullscreen blur of the default framebuffer and blits it to screen
	 */
	void ( *BlurScreen )( void );

	void ( *DrawStretchPic )( int x, int y, int w, int h, float s1, float t1, float s2, float t2,
							  const float *color, const struct shader_s *shader );
	void ( *DrawRotatedStretchPic )( int x, int y, int w, int h, float s1, float t1, float s2, float t2,
									 float angle, const vec4_t color, const struct shader_s *shader );

	void ( *Scissor )( int x, int y, int w, int h );
	void ( *ResetScissor )( void );

	bool ( *LerpTag )( orientation_t *orient, const struct model_s *mod, int oldframe, int frame, float lerpfrac, const char *name );

	int ( *GetClippedFragments )( const vec3_t origin, float radius, vec3_t axis[3], int maxfverts, vec4_t *fverts,
								  int maxfragments, fragment_t *fragments );

	void ( *TransformVectorToScreen )( const refdef_t *rd, const vec3_t in, vec2_t out );
	bool ( *TransformVectorToScreenClamped )( const refdef_t *rd, const vec3_t target, int border, vec2_t out );

	void ( *BeginFrame )( void );
	void ( *EndFrame )( void );

	void ( *AppActivate )( bool active, bool minimize );
} ref_export_t;

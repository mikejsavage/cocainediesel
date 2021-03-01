#include <Ultralight/Ultralight.h>
#include <Ultralight/platform/GPUDriver.h>
#include <AppCore/Platform.h>

#include <glsl/shader_fill_frag.h>
#include <glsl/shader_fill_path_frag.h>
#include <glsl/shader_v2f_c4f_t2f_t2f_d28f_vert.h>
#include <glsl/shader_v2f_c4f_t2f_vert.h>

#include "qcommon/base.h"
#include "qcommon/types.h"
#include "qcommon/array.h"
#include "client/renderer/renderer.h"
#include "cgame/cg_local.h"

#include "glad/glad.h"

namespace ul = ultralight;

static Shader ultralight_shaders[ 2 ];

static GLuint CL_Ultralight_CompileShader( GLenum type, const char * source ) {
	GLuint shader = glCreateShader( type );
	glShaderSource( shader, 1, &source, NULL );
	glCompileShader( shader );
	
	GLint status;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &status );
	if( status == GL_FALSE ) {
		char buf[ 1024 ];
		glGetShaderInfoLog( shader, sizeof( buf ), NULL, buf );
		Com_Printf( S_COLOR_YELLOW "Shader compilation failed: %s\n", buf );
		assert( false );
	}

	return shader;
}

static void CL_Ultralight_LoadProgram( u32 type ) {
	GLuint vs, fs;
	if( type == ul::kShaderType_Fill ) {
		vs = CL_Ultralight_CompileShader( GL_VERTEX_SHADER, shader_v2f_c4f_t2f_t2f_d28f_vert().c_str() );
		fs = CL_Ultralight_CompileShader( GL_FRAGMENT_SHADER, shader_fill_frag().c_str() );
	}
	else if( type == ul::kShaderType_FillPath ) {
		vs = CL_Ultralight_CompileShader( GL_VERTEX_SHADER, shader_v2f_c4f_t2f_vert().c_str() );
		fs = CL_Ultralight_CompileShader( GL_FRAGMENT_SHADER, shader_fill_path_frag().c_str() );
	}

	GLuint program = glCreateProgram();
	glAttachShader( program, vs );
	glAttachShader( program, fs );

	glBindAttribLocation( program, 0, "in_Position" );
	glBindAttribLocation( program, 1, "in_Color" );
	glBindAttribLocation( program, 2, "in_TexCoord" );
	
	if( type == ul::kShaderType_Fill ) {
		glBindAttribLocation( program, 3, "in_ObjCoord" );
		glBindAttribLocation( program, 4, "in_Data0" );
		glBindAttribLocation( program, 5, "in_Data1" );
		glBindAttribLocation( program, 6, "in_Data2" );
		glBindAttribLocation( program, 7, "in_Data3" );
		glBindAttribLocation( program, 8, "in_Data4" );
		glBindAttribLocation( program, 9, "in_Data5" );
		glBindAttribLocation( program, 10, "in_Data6" );
	}

	glLinkProgram( program );

	GLint status;
	glGetProgramiv( program, GL_LINK_STATUS, &status );
	if( status == GL_FALSE ) {
		char buf[ 1024 ];
		glGetProgramInfoLog( program, sizeof( buf ), NULL, buf );
		Com_Printf( S_COLOR_YELLOW "Shader linking failed: %s\n", buf );
		assert( false );
	}
	glUseProgram( program );

	Shader shader;

	GLint count, maxlen;
	glGetProgramiv( program, GL_ACTIVE_UNIFORMS, &count );
	glGetProgramiv( program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxlen );

	size_t num_textures = 0;
	for( GLint i = 0; i < count; i++ ) {
		char name[ 128 ];
		GLint size, len;
		GLenum type;
		glGetActiveUniform( program, i, sizeof( name ), &len, &size, &type, name );

		if( type == GL_SAMPLER_2D ) {
			glUniform1i( glGetUniformLocation( program, name ), num_textures );
			shader.textures[ num_textures ] = Hash64( name, len );
			num_textures++;
		}
	}

	glGetProgramiv( program, GL_ACTIVE_UNIFORM_BLOCKS, &count );
	glGetProgramiv( program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &maxlen );

	for( GLint i = 0; i < count; i++ ) {
		char name[ 128 ];
		GLint len;
		glGetActiveUniformBlockName( program, i, sizeof( name ), &len, name );
		glUniformBlockBinding( program, i, i );
		shader.uniforms[ i ] = Hash64( name, len );
	}

	glUseProgram( 0 );
	shader.program = program;
	ultralight_shaders[ type ] = shader;
}

static void CL_Ultralight_LoadShaders() {
	CL_Ultralight_LoadProgram( ul::kShaderType_Fill );
	CL_Ultralight_LoadProgram( ul::kShaderType_FillPath );
}

Mat4 UltralightToDiesel( ul::Matrix4x4 mat ) {
	return Mat4(
		mat.data[ 0 ], mat.data[ 4 ], mat.data[ 8 ], mat.data[ 12 ],
		mat.data[ 1 ], mat.data[ 5 ], mat.data[ 9 ], mat.data[ 13 ],
		mat.data[ 2 ], mat.data[ 6 ], mat.data[ 10 ], mat.data[ 14 ],
		mat.data[ 3 ], mat.data[ 7 ], mat.data[ 11 ], mat.data[ 15 ]
	);
}

Vec4 UltralightToDiesel( ul::vec4 v ) {
	return Vec4( v.x, v.y, v.z, v.w );
}

class GPUDriverGL : public ul::GPUDriver {
public:
	GPUDriverGL() : 
		command_list( sys_allocator ),
		textures( sys_allocator ),
		framebuffers( sys_allocator ),
		meshes( sys_allocator ) {};
	~GPUDriverGL() {
		for( Texture tex : textures ) {
			DeleteTexture( tex );
		}
		for( Framebuffer fb : framebuffers ) {
			DeleteFramebuffer( fb );
		}
		for( Mesh mesh : meshes ) {
			DeleteMesh( mesh );
		}
		FREE( sys_allocator, command_list.ptr() );
		FREE( sys_allocator, textures.ptr() );
		FREE( sys_allocator, framebuffers.ptr() );
		FREE( sys_allocator, meshes.ptr() );
	};

	void BeginSynchronize() override {}
	void EndSynchronize() override {}

	u32 NextTextureId() override {
		return next_texture_id++;
	}
	void CreateTexture( u32 texture_id, ul::Ref< ul::Bitmap > bitmap ) override {
		if( bitmap->IsEmpty() ) {
			// framebuffer texture, i think
			// return;
		}
		TextureConfig config;
		config.filter = TextureFilter_Linear;
		config.wrap = TextureWrap_Clamp;

		config.width = bitmap->width();
		config.height = bitmap->height();
		config.data = bitmap->LockPixels();

		if( bitmap->format() == ul::kBitmapFormat_A8_UNORM ) {
			config.format = TextureFormat_R_U8;
		}
		else if( bitmap->format() == ul::kBitmapFormat_BGRA8_UNORM_SRGB ) {
			config.format = TextureFormat_BGRA_U8_sRGB; // TODO: should be BGRA
		}

		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		glPixelStorei( GL_UNPACK_ROW_LENGTH, bitmap->row_bytes() / bitmap->bpp() );

		Texture tex = NewTexture( config );
		bitmap->UnlockPixels();

		textures.resize( Max2( textures.size(), size_t( texture_id + 1 ) ) );
		textures[ texture_id ] = tex;

		Com_GGPrint( "CreateTexture id: {}", tex.texture );
	}
	void DestroyTexture( u32 texture_id ) override {
		DeleteTexture( textures[ texture_id ] );
	}
	void UpdateTexture( u32 texture_id, ul::Ref< ul::Bitmap > bitmap ) override {
		DestroyTexture( texture_id );
		CreateTexture( texture_id, bitmap );
	}

	u32 NextRenderBufferId() override {
		return next_render_buffer_id++;
	}
	void CreateRenderBuffer( u32 render_buffer_id, const ul::RenderBuffer & buffer ) override {
		FramebufferConfig fb;

		Texture tex = textures[ buffer.texture_id ];

		// TextureConfig config;
		// config.width = buffer.width;
		// config.height = buffer.height;
		// config.format = TextureFormat_RGBA_U8_sRGB;

		// Texture tex = NewTexture( config );
		// textures.resize( Max2( textures.size(), size_t( buffer.texture_id + 1 ) ) );
		// textures[ buffer.texture_id ] = tex;

		Framebuffer fbo = NewFramebuffer( &tex, NULL, NULL );
		framebuffers.resize( Max2( framebuffers.size(), size_t( render_buffer_id + 1 ) ) );
		framebuffers[ render_buffer_id ] = fbo;

		Com_GGPrint( "CreateRenderBuffer id: {}", tex.texture );
	}
	void DestroyRenderBuffer( u32 render_buffer_id ) override {
		DeleteFramebuffer( framebuffers[ render_buffer_id ] );
	}
	void ClearRenderBuffer( u32 render_buffer_id ) {
		glBindFramebuffer( GL_FRAMEBUFFER, framebuffers[ render_buffer_id ].fbo );
		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		glClear( GL_COLOR_BUFFER_BIT );
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );    
	}

	u32 NextGeometryId() override {
		return next_geometry_id++;
	}
	void CreateGeometry( u32 geometry_id, const ul::VertexBuffer & vertices, const ul::IndexBuffer & indices ) override {
		Mesh mesh;

		mesh.primitive_type = PrimitiveType_Triangles;
		mesh.indices_format = IndexFormat_U32;
		
		mesh.positions = NewVertexBuffer( vertices.data, vertices.size );

		glGenVertexArrays( 1, &mesh.vao );
		glBindVertexArray( mesh.vao );

		glBindBuffer( GL_ARRAY_BUFFER, mesh.positions.vbo );
		if( vertices.format == ul::kVertexBufferFormat_2f_4ub_2f_2f_28f ) {
			u32 stride = 140;

			SetupAttribute( 0, VertexFormat_Floatx2, stride, 0 );
			SetupAttribute( 1, VertexFormat_U8x4_Norm, stride, 8 );
			SetupAttribute( 2, VertexFormat_Floatx2, stride, 12 );
			SetupAttribute( 3, VertexFormat_Floatx2, stride, 20 );

			for( u32 i = 0; i < 7; i++ ) {
				SetupAttribute( 4 + i, VertexFormat_Floatx4, stride, 28 + ( i * sizeof( Vec4 ) ) );
			}
		}
		else if( vertices.format == ul::kVertexBufferFormat_2f_4ub_2f ) {
			u32 stride = 20;

			SetupAttribute( 0, VertexFormat_Floatx2, stride, 0 );
			SetupAttribute( 1, VertexFormat_U8x4_Norm, stride, 8 );
			SetupAttribute( 2, VertexFormat_Floatx2, stride, 12 );
		}

		mesh.indices = NewIndexBuffer( indices.data, indices.size );

		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, mesh.indices.ebo );

		glBindVertexArray( 0 );

		meshes.resize( Max2( meshes.size(), size_t( geometry_id + 1 ) ) );
		meshes[ geometry_id ] = mesh;
	}
	void DestroyGeometry( u32 geometry_id ) override {
		Mesh mesh = meshes[ geometry_id ];
		DeleteMesh( mesh );
	}
	void UpdateGeometry( u32 geometry_id, const ul::VertexBuffer & vertices, const ul::IndexBuffer & indices ) override {
		DestroyGeometry( geometry_id );
		CreateGeometry( geometry_id, vertices, indices );
	}
	void DrawGeometry( u32 geometry_id, u32 indices_count, u32 indices_offset, const ul::GPUState & state ) {
		PipelineState pipeline;
		pipeline.pass = frame_static.ultralight_pass;
		pipeline.shader = &ultralight_shaders[ state.shader_type ];
		pipeline.depth_func = DepthFunc_Disabled;
		pipeline.blend_func = BlendFunc_Blend;
		pipeline.cull_face = CullFace_Disabled;
		pipeline.write_depth = false;

		// TODO: set viewport from state.viewport_width/height

		if( state.enable_scissor ) {
			pipeline.scissor.x = state.scissor_rect.x();
			pipeline.scissor.y = state.scissor_rect.y();
			pipeline.scissor.w = state.scissor_rect.width();
			pipeline.scissor.h = state.scissor_rect.height();
		}

		pipeline.blend_func = state.enable_blend ? BlendFunc_Blend : BlendFunc_Disabled;
	
		float time = cls.monotonicTime * 0.001f;
		float scale = 1.0f;
		pipeline.set_uniform( "u_State", UploadUniformBlock( Vec4( time, state.viewport_width, state.viewport_height, scale ) ) );

		ul::Matrix transform;
		transform.Set( state.transform );
		ul::Matrix result;
		result.SetOrthographicProjection( state.viewport_width, state.viewport_height, false );
		result.Transform( transform );
		pipeline.set_uniform( "u_Transform", UploadUniformBlock( UltralightToDiesel( result.GetMatrix4x4() ) ) );

		Vec4 uniform_scalar[ 2 ];
		uniform_scalar[ 0 ] = Vec4( state.uniform_scalar[ 0 ], state.uniform_scalar[ 1 ], state.uniform_scalar[ 2 ], state.uniform_scalar[ 3 ] );
		uniform_scalar[ 1 ] = Vec4( state.uniform_scalar[ 4 ], state.uniform_scalar[ 5 ], state.uniform_scalar[ 6 ], state.uniform_scalar[ 7 ] );
		pipeline.set_uniform( "u_Scalar4", UploadUniformBlock( uniform_scalar[ 0 ], uniform_scalar[ 1 ] ) );

		Vec4 uniform_vectors[ 8 ];
		for( u32 i = 0; i < 8; i++ ) {
			uniform_vectors[ i ] = UltralightToDiesel( state.uniform_vector[ i ] );
		}
		pipeline.set_uniform( "u_Vector", UploadUniformBlock(
			uniform_vectors[ 0 ],
			uniform_vectors[ 1 ],
			uniform_vectors[ 2 ],
			uniform_vectors[ 3 ],
			uniform_vectors[ 4 ],
			uniform_vectors[ 5 ],
			uniform_vectors[ 6 ],
			uniform_vectors[ 7 ]
		) );

		pipeline.set_uniform( "u_ClipSize", UploadUniformBlock( u32( state.clip_size ) ) );

		Mat4 clip[ 8 ];
		for( u32 i = 0; i < 8; i++ ) {
			clip[ i ] = UltralightToDiesel( state.clip[ i ] );
		}
		pipeline.set_uniform( "u_Clip", UploadUniformBlock(
			clip[ 0 ],
			clip[ 1 ],
			clip[ 2 ],
			clip[ 3 ],
			clip[ 4 ],
			clip[ 5 ],
			clip[ 6 ],
			clip[ 7 ]
		) );

		if( state.texture_1_id != 0 ) pipeline.set_texture( "Texture1", &textures[ state.texture_1_id ] );
		if( state.texture_2_id != 0 ) pipeline.set_texture( "Texture2", &textures[ state.texture_2_id ] );
		if( state.texture_3_id != 0 ) pipeline.set_texture( "Texture3", &textures[ state.texture_3_id ] );

		DrawMesh( meshes[ geometry_id ], pipeline, indices_count, indices_offset * sizeof( u32 ) );
	}

	void UpdateCommandList( const ul::CommandList & list ) {
		if( list.size ) {
			command_list.resize( list.size );
			memcpy( command_list.ptr(), list.commands, sizeof( ul::Command ) * list.size );
		}
	}

	void Draw() {
		ZoneScoped;

		if( command_list.size() == 0 ) {
			return;
		}

		for( ul::Command cmd : command_list ) {
			if( cmd.command_type == ul::kCommandType_ClearRenderBuffer ) {
				ClearRenderBuffer( cmd.gpu_state.render_buffer_id );
			}
			else if( cmd.command_type == ul::kCommandType_DrawGeometry ) {
				DrawGeometry( cmd.geometry_id, cmd.indices_count, cmd.indices_offset, cmd.gpu_state );
			}
		}
		command_list.clear();
	}

	u32 next_texture_id = 1;
	u32 next_render_buffer_id = 1;
	u32 next_geometry_id = 1;
	DynamicArray< ul::Command > command_list;

	DynamicArray< Texture > textures;
	DynamicArray< Framebuffer > framebuffers;
	DynamicArray< Mesh > meshes;
};

ul::RefPtr< ul::Renderer > renderer;
ul::RefPtr< ul::View > view;
GPUDriverGL driver;

void CL_Ultralight_Init() {
	ZoneScoped;

	CL_Ultralight_LoadShaders();

	ul::Config config;

	config.resource_path = "./resources/";
	config.use_gpu_renderer = true;
	config.device_scale = 1.0;
	config.enable_images = true;
	config.force_repaint = true;
	config.enable_javascript = true;
	config.font_hinting = ul::FontHinting::kFontHinting_Smooth;

	ul::Platform::instance().set_gpu_driver( &driver );
	ul::Platform::instance().set_config( config );
	ul::Platform::instance().set_font_loader( ul::GetPlatformFontLoader() );
	ul::Platform::instance().set_file_system( ul::GetPlatformFileSystem( "." ) );
	ul::Platform::instance().set_logger( ul::GetDefaultLogger( "ultralight.log" ) );

	renderer = ul::Renderer::Create();
	view = renderer->CreateView( frame_static.viewport_width, frame_static.viewport_height, true, nullptr );
	view->LoadURL( "https://cocainediesel.fun" );
	view->Focus();
}

void CL_Ultralight_Shutdown() {
	ZoneScoped;

	view->Release();
	view = nullptr;

	renderer->Release();
	renderer = nullptr;

	DeleteShader( ultralight_shaders[ 0 ] );
	DeleteShader( ultralight_shaders[ 1 ] );
}

void CL_Ultralight_Frame() {
	ZoneScoped;

	view->Resize( frame_static.viewport_width, frame_static.viewport_height );
	renderer->Update();
	renderer->Render();
	renderer->LogMemoryUsage();
	driver.Draw();
}

void CL_Ultralight_MouseMove( u32 x, u32 y ) {
	ul::MouseEvent evt;
	evt.type = ul::MouseEvent::kType_MouseMoved;
	evt.x = x;
	evt.y = y;
	evt.button = ul::MouseEvent::kButton_None;
	
	view->FireMouseEvent( evt );
}

void CL_Ultralight_MouseScroll( u32 x, u32 y ) {
	ul::ScrollEvent evt;
	evt.type = ul::ScrollEvent::kType_ScrollByPage;
	evt.delta_x = x;
	evt.delta_y = y;
	
	view->FireScrollEvent( evt );
}

void CL_Ultralight_MouseDown( u32 x, u32 y, s32 button ) {
	ul::MouseEvent evt;
	evt.type = ul::MouseEvent::kType_MouseDown;
	evt.x = x;
	evt.y = y;
	evt.button = ul::MouseEvent::Button( button );
	
	view->FireMouseEvent( evt );
}

void CL_Ultralight_MouseUp( u32 x, u32 y, s32 button ) {
	ul::MouseEvent evt;
	evt.type = ul::MouseEvent::kType_MouseUp;
	evt.x = x;
	evt.y = y;
	evt.button = ul::MouseEvent::Button( button );
	
	view->FireMouseEvent( evt );
}

void CL_Ultralight_KeyDown( s32 virtual_key, s32 native_key, s32 mods ) {
	ul::KeyEvent evt;
	evt.type = ul::KeyEvent::kType_KeyDown;
	evt.virtual_key_code = virtual_key;
	evt.native_key_code = native_key;
	evt.modifiers = mods;

	view->FireKeyEvent( evt );
}

void CL_Ultralight_KeyUp( s32 virtual_key, s32 native_key, s32 mods ) {
	ul::KeyEvent evt;
	evt.type = ul::KeyEvent::kType_KeyUp;
	evt.virtual_key_code = virtual_key;
	evt.native_key_code = native_key;
	evt.modifiers = mods;

	view->FireKeyEvent( evt );
}

void CL_Ultralight_Char( u32 codepoint ) {
	ul::KeyEvent evt;
	evt.type = ul::KeyEvent::kType_Char;
	ul::String text = ul::String32( ( const char32_t * ) &codepoint, 1 );
	evt.text = text;
	evt.unmodified_text = text;

	view->FireKeyEvent( evt );
}

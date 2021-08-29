#include <Ultralight/Ultralight.h>
#include <Ultralight/platform/GPUDriver.h>
#include <AppCore/Platform.h>

#include <Ultralight/JavaScript.h>
#include <JavaScriptCore/JSRetainPtr.h>

#include "glsl/shader_fill_frag.h"
#include "glsl/shader_fill_path_frag.h"
#include "glsl/shader_v2f_c4f_t2f_t2f_d28f_vert.h"
#include "glsl/shader_v2f_c4f_t2f_vert.h"

#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
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
	void init() {
		command_list.init( sys_allocator );
		textures.init( sys_allocator );
		framebuffers.init( sys_allocator );
		meshes.init( sys_allocator );
	}

	void shutdown() {
		for( Texture tex : textures ) {
			DeleteTexture( tex );
		}
		for( Framebuffer fb : framebuffers ) {
			DeleteFramebuffer( fb );
		}
		for( Mesh mesh : meshes ) {
			DeleteMesh( mesh );
		}
		command_list.shutdown();
		textures.shutdown();
		framebuffers.shutdown();
		meshes.shutdown();
	}

	void BeginSynchronize() override {}
	void EndSynchronize() override {}

	u32 NextTextureId() override {
		return next_texture_id++;
	}
	void CreateTexture( u32 texture_id, ul::Ref< ul::Bitmap > bitmap ) override {
		ZoneScoped;
		TextureConfig config;
		config.filter = TextureFilter_Linear;
		config.wrap = TextureWrap_Clamp;

		config.width = bitmap->width();
		config.height = bitmap->height();
		config.data = bitmap->LockPixels();

		if( bitmap->IsEmpty() ) {
			config.format = TextureFormat_RGBA_U8; // fbo texture
		}
		else if( bitmap->format() == ul::kBitmapFormat_A8_UNORM ) {
			config.format = TextureFormat_R_U8;
		}
		else if( bitmap->format() == ul::kBitmapFormat_BGRA8_UNORM_SRGB ) {
			config.format = TextureFormat_BGRA_U8_sRGB;
		}

		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		glPixelStorei( GL_UNPACK_ROW_LENGTH, bitmap->row_bytes() / bitmap->bpp() );

		Texture tex = NewTexture( config );
		bitmap->UnlockPixels();

		// defaults
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );

		textures.resize( Max2( textures.size(), size_t( texture_id + 1 ) ) );
		textures[ texture_id ] = tex;
	}
	void DestroyTexture( u32 texture_id ) override {
		ZoneScoped;
		DeleteTexture( textures[ texture_id ] );
	}
	void UpdateTexture( u32 texture_id, ul::Ref< ul::Bitmap > bitmap ) override {
		ZoneScoped;

		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		glPixelStorei( GL_UNPACK_ROW_LENGTH, bitmap->row_bytes() / bitmap->bpp() );

		WriteTexture( textures[ texture_id ], bitmap->LockPixels() );
		bitmap->UnlockPixels();

		// defaults
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	}

	u32 NextRenderBufferId() override {
		return next_render_buffer_id++;
	}
	void CreateRenderBuffer( u32 render_buffer_id, const ul::RenderBuffer & buffer ) override {
		ZoneScoped;
		FramebufferConfig fb;

		Texture tex = textures[ buffer.texture_id ];

		Framebuffer fbo = NewFramebuffer( &tex, NULL, NULL );
		framebuffers.resize( Max2( framebuffers.size(), size_t( render_buffer_id + 1 ) ) );
		framebuffers[ render_buffer_id ] = fbo;
	}
	void DestroyRenderBuffer( u32 render_buffer_id ) override {
		ZoneScoped;
		DeleteFramebuffer( framebuffers[ render_buffer_id ] );
	}
	void ClearRenderBuffer( u32 render_buffer_id ) {
		ZoneScoped;
		PipelineState pipeline;
		pipeline.pass = frame_static.ultralight_pass;
		pipeline.shader = &ultralight_shaders[ 0 ];
		pipeline.depth_func = DepthFunc_Disabled;
		pipeline.blend_func = BlendFunc_Disabled;
		pipeline.cull_face = CullFace_Disabled;
		pipeline.write_depth = false;

		pipeline.target = framebuffers[ render_buffer_id ];
		pipeline.clear_target = true;
		DrawFullscreenMesh( pipeline );
	}

	u32 NextGeometryId() override {
		return next_geometry_id++;
	}
	void CreateGeometry( u32 geometry_id, const ul::VertexBuffer & vertices, const ul::IndexBuffer & indices ) override {
		ZoneScoped;
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
		ZoneScoped;
		Mesh mesh = meshes[ geometry_id ];
		DeleteMesh( mesh );
	}
	void UpdateGeometry( u32 geometry_id, const ul::VertexBuffer & vertices, const ul::IndexBuffer & indices ) override {
		ZoneScoped;
		Mesh & mesh = meshes[ geometry_id ];
		glBindBuffer( GL_ARRAY_BUFFER, mesh.positions.vbo );
		glBufferData( GL_ARRAY_BUFFER, vertices.size, vertices.data, GL_DYNAMIC_DRAW );
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, mesh.indices.ebo );
		glBufferData( GL_ELEMENT_ARRAY_BUFFER, indices.size, indices.data, GL_STATIC_DRAW );
	}
	void DrawGeometry( u32 geometry_id, u32 indices_count, u32 indices_offset, const ul::GPUState & state ) {
		ZoneScoped;
		PipelineState pipeline;
		pipeline.pass = frame_static.ultralight_pass;
		pipeline.shader = &ultralight_shaders[ state.shader_type ];
		pipeline.depth_func = DepthFunc_Disabled;
		pipeline.cull_face = CullFace_Disabled;
		pipeline.write_depth = false;

		pipeline.viewport_width = state.viewport_width;
		pipeline.viewport_height = state.viewport_height;

		pipeline.target = framebuffers[ state.render_buffer_id ];

		if( state.enable_scissor ) {
			pipeline.scissor.x = state.scissor_rect.x();
			pipeline.scissor.y = state.scissor_rect.y();
			pipeline.scissor.w = state.scissor_rect.width();
			pipeline.scissor.h = state.scissor_rect.height();
		}

		pipeline.blend_func = state.enable_blend ? BlendFunc_Straight : BlendFunc_Disabled;

		float time = cls.monotonicTime * 0.001f;
		float scale = 1.0f;
		pipeline.set_uniform( "u_State", UploadUniformBlock( Vec4( time, state.viewport_width, state.viewport_height, scale ) ) );

		ul::Matrix transform;
		transform.Set( state.transform );
		ul::Matrix result;
		bool flip_y = state.render_buffer_id != 0;
		pipeline.flip_y = flip_y;
		result.SetOrthographicProjection( state.viewport_width, state.viewport_height, flip_y );
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
		ZoneScoped;
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
	NonRAIIDynamicArray< ul::Command > command_list;

	NonRAIIDynamicArray< Texture > textures;
	NonRAIIDynamicArray< Framebuffer > framebuffers;
	NonRAIIDynamicArray< Mesh > meshes;
};

static ul::RefPtr< ul::Renderer > renderer;
static ul::RefPtr< ul::View > view;
static GPUDriverGL driver;
static bool show_inspector;

static const char * GG( Allocator * a, JSContextRef ctx, JSValueRef str ) {
	JSStringRef str2 = JSValueToStringCopy( ctx, str, NULL );
	assert( str2 != NULL );

	size_t buf_size = JSStringGetMaximumUTF8CStringSize( str2 );
	char * buf = ALLOC_MANY( a, char, buf_size );
	JSStringGetUTF8CString( str2, buf, buf_size );

	JSStringRelease( str2 );

	return buf;
}

static JSValueRef HelloFromJS( JSContextRef ctx, JSObjectRef function, JSObjectRef self, size_t num_args, const JSValueRef * args, JSValueRef * exception ) {
	assert( num_args == 2 );
	assert( JSValueIsString( ctx, args[ 0 ] ) );
	assert( JSValueIsString( ctx, args[ 1 ] ) );

	TempAllocator temp = cls.frame_arena.temp();

	const char * cvar = GG( &temp, ctx, args[ 0 ] );
	const char * value = GG( &temp, ctx, args[ 1 ] );

	Com_GGPrint( "Set {} = {}", cvar, value );

	return JSValueMakeNull( ctx );
}

static void SetObjectKey( JSContextRef ctx, JSObjectRef obj, const char * key, JSValueRef value ) {
	JSRetainPtr< JSStringRef > keylol = adopt( JSStringCreateWithUTF8CString( key ) );
	JSObjectSetPropertyForKey( ctx, obj, JSValueMakeString( ctx, keylol.get() ), value, kJSPropertyAttributeReadOnly, NULL );
}

static void SetObjectFunction( JSContextRef ctx, JSObjectRef obj, const char * name, JSObjectCallAsFunctionCallback func ) {
	JSRetainPtr< JSStringRef > namelol = adopt( JSStringCreateWithUTF8CString( name ) );
	JSObjectRef funclol = JSObjectMakeFunctionWithCallback( ctx, namelol.get(), func );
	JSObjectSetPropertyForKey( ctx, obj, JSValueMakeString( ctx, namelol.get() ), funclol, kJSPropertyAttributeReadOnly, NULL );
}

static void RegisterEngineAPI() {
	ul::Ref< ul::JSContext > context = view->LockJSContext();
	JSContextRef ctx = context.get();

	JSObjectRef engine = JSObjectMake( ctx, NULL, NULL );

	SetObjectFunction( ctx, engine, "SetCvar", HelloFromJS );

	SetObjectKey( ctx, JSContextGetGlobalObject( ctx ), "engine", engine );
}

static void ToggleInspector() {
	show_inspector = !show_inspector;
}

void CL_Ultralight_Init() {
	ZoneScoped;

	show_inspector = false;

	Cmd_AddCommand( "toggleinspector", ToggleInspector );

	CL_Ultralight_LoadShaders();

	ul::Config config;

	config.resource_path = "./resources/";
	config.use_gpu_renderer = true;
	config.device_scale = 1.0;
	config.enable_images = true;
	config.force_repaint = true;
	config.enable_javascript = true;
	config.font_hinting = ul::FontHinting::kFontHinting_Normal;

	driver.init();

	TempAllocator temp = cls.frame_arena.temp();

	ul::Platform::instance().set_gpu_driver( &driver );
	ul::Platform::instance().set_config( config );
	ul::Platform::instance().set_font_loader( ul::GetPlatformFontLoader() );
	ul::Platform::instance().set_file_system( ul::GetPlatformFileSystem( temp( "{}/base/ui", RootDirPath() ) ) );
	ul::Platform::instance().set_logger( ul::GetDefaultLogger( "ultralight.log" ) );

	renderer = ul::Renderer::Create();
	view = renderer->CreateView( frame_static.viewport_width, frame_static.viewport_height, true, nullptr );
	view->LoadURL( "file:///menu.html#ultralight" );
	view->Focus();

	RegisterEngineAPI();
}

void CL_Ultralight_Shutdown() {
	ZoneScoped;

	view = nullptr;
	renderer = nullptr;
	driver.shutdown();

	DeleteShader( ultralight_shaders[ 0 ] );
	DeleteShader( ultralight_shaders[ 1 ] );

	Cmd_RemoveCommand( "toggleinspector" );
}

static void DrawMenu( JSContextRef ctx ) {
	JSRetainPtr<JSStringRef> str = adopt(JSStringCreateWithUTF8CString("OnFrame"));
	JSValueRef func = JSEvaluateScript(ctx, str.get(), 0, 0, 0, 0);

	if (JSValueIsObject(ctx, func)) {
		JSObjectRef funcObj = JSValueToObject(ctx, func, 0);
		assert( funcObj != NULL );
		if (JSObjectIsFunction(ctx, funcObj)) {
			JSObjectRef args = JSObjectMake( ctx, NULL, NULL );

			JSRetainPtr<JSStringRef> msg = adopt(JSStringCreateWithUTF8CString("Howdy!"));
			SetObjectKey( ctx, args, "howdy", JSValueMakeString( ctx, msg.get() ) );

			JSValueRef exception = 0;
			JSObjectCallAsFunction( ctx, funcObj, 0, 1, &args, &exception );

			if( exception ) {
				JSObjectRef asdf = JSValueToObject( ctx, exception, 0 );
				assert( asdf != NULL );

				JSRetainPtr< JSStringRef > message_literal_string = adopt( JSStringCreateWithUTF8CString( "message" ) );

				JSValueRef shit = JSObjectGetPropertyForKey( ctx, asdf, JSValueMakeString( ctx, message_literal_string.get() ), NULL );

				JSStringRef fdsa = JSValueToStringCopy( ctx, shit, NULL );
				if( fdsa != NULL ) {
					TempAllocator temp = cls.frame_arena.temp();
					size_t buf_size = JSStringGetMaximumUTF8CStringSize( fdsa );
					char * buf = ALLOC_MANY( &temp, char, buf_size );
					JSStringGetUTF8CString( fdsa, buf, buf_size );
					Com_GGPrint( "shits fucked: {}", buf );
				}
			}
		}
	}
}

void UltralightBeginFrame() {
	ZoneScoped;
	view->Resize( frame_static.viewport_width, frame_static.viewport_height );
	view->inspector()->Resize( frame_static.viewport_width, frame_static.viewport_height / 3 );
}

static void RenderUltralightView( ul::RefPtr< ul::View > view, bool is_inspector ) {
	ul::Ref<ul::JSContext> context = view->LockJSContext();
	JSContextRef ctx = context.get();

	if( !is_inspector ) {
		DrawMenu( ctx ); // TODO
	}

	ZoneScoped;

	{
		ZoneScopedN( "renderer->Update()" );
		renderer->Update();
	}
	{
		ZoneScopedN( "renderer->Render()" );
		renderer->Render();
	}
	// renderer->LogMemoryUsage();
	{
		ZoneScopedN( "driver.Draw()" );
		driver.Draw();
	}

	{
		ZoneScopedN( "render fbo to screen" );
		PipelineState pipeline;
		pipeline.pass = frame_static.ultralight_pass;
		pipeline.shader = &shaders.standard_vertexcolors;
		pipeline.depth_func = DepthFunc_Disabled;
		pipeline.blend_func = BlendFunc_Straight;
		pipeline.write_depth = false;

		pipeline.set_uniform( "u_View", frame_static.ortho_view_uniforms );
		pipeline.set_uniform( "u_Model", frame_static.identity_model_uniforms );
		pipeline.set_uniform( "u_Material", frame_static.identity_material_uniforms );
		pipeline.set_texture( "u_BaseTexture", &driver.textures[ view->render_target().texture_id ] );

		Vec3 positions[] = {
			Vec3( 0, 0, 0 ),
			Vec3( view->width(), 0, 0 ),
			Vec3( 0, view->height(), 0 ),
			Vec3( view->width(), view->height(), 0 ),
		};

		Vec2 half_pixel = 0.5f / frame_static.viewport;
		Vec2 uvs[] = {
			Vec2( half_pixel.x, half_pixel.y ),
			Vec2( 1.0f - half_pixel.x, half_pixel.y ),
			Vec2( half_pixel.x, 1.0f - half_pixel.y ),
			Vec2( 1.0f - half_pixel.x, 1.0f - half_pixel.y ),
		};
		constexpr RGBA8 colors[] = { rgba8_white, rgba8_white, rgba8_white, rgba8_white };

		u16 base_index = DynamicMeshBaseIndex();
		u16 indices[] = { 0, 2, 1, 3, 1, 2 };
		for( u16 & idx : indices ) {
			idx += base_index;
		}

		DynamicMesh mesh = { };
		mesh.positions = positions;
		mesh.uvs = uvs;
		mesh.colors = colors;
		mesh.indices = indices;
		mesh.num_vertices = 4;
		mesh.num_indices = 6;

		DrawDynamicMesh( pipeline, mesh );
	}
}

void UltralightEndFrame() {
	RenderUltralightView( view, false );

	if( show_inspector ) {
		RenderUltralightView( view->inspector(), true );
	}
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

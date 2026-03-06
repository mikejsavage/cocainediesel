#include "imgui/imgui.h"

#include "qcommon/base.h"
#include "qcommon/time.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/dev_tools.h"
#include "client/renderer/renderer.h"
#include "client/renderer/skybox.h"
#include "cgame/cg_local.h"

struct ModelViewerState {
	char * selected_model;
	ImGuiTextFilter filter;
	float distance;
	EulerDegrees2 angles;

	bool animate;
	bool animation_paused;

	struct Animation {
		u8 index, lower_index;
		Time start_time;
	};

	Animation animation1;
	Animation animation2;
	Time blend_end_time;
	bool use_animation2;

	Animation additive_animation;
};

static u8 AnimationSelector( const char * name, const GLTFRenderData * gltf, u8 index ) {
	TempAllocator temp = cls.frame_arena.temp();
	if( ImGui::BeginCombo( name, temp( "{}", gltf->animations[ index ].name ) ) ) {
		for( u8 i = 0; i < gltf->animations.n; i++ ) {
			if( ImGui::Selectable( temp( "{}", gltf->animations[ i ].name ), i == index ) )
				index = i;
			if( i == index )
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	return index;
}

DevToolCleanupCallback DrawModelViewer() {
	constexpr Time blend_duration = Milliseconds( 250 );

	static ModelViewerState state;

	TempAllocator temp = cls.frame_arena.temp();

	ImGui::SetNextWindowPos( ImVec2() );
	ImGui::SetNextWindowSize( ImVec2( frame_static.viewport_width * 0.2f, frame_static.viewport_height ) );
	ImGui::Begin( "modelviewer", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_Interactive );

	if( ImGui::IsWindowAppearing() ) {
		state = {
			.selected_model = CopyString( sys_allocator, "players/rigg/model" ),
			.distance = 50.0f,
			.angles = EulerDegrees2( 20.0f, 135.0f ),
			.animate = true,
			.animation1 = { .start_time = cls.monotonicTime },
			.animation2 = { .start_time = cls.monotonicTime },
			.additive_animation = { .start_time = Minutes( -5 ) },
		};
	}

	ImGui::AlignTextToFramePadding();
	ImGui::Text( "Model" );
	ImGui::SameLine();

	if( ImGui::BeginCombo( "##model", state.selected_model ) ) {
		state.filter.Draw( "##filter" );
		if( ImGui::IsWindowAppearing() ) {
			ImGui::SetKeyboardFocusHere( -1 );
		}

		for( Span< const char > path : AssetPaths() ) {
			if( EndsWith( path, ".glb" ) && state.filter.PassFilter( path.begin(), path.end() ) ) {
				Span< const char > model = StripExtension( path );
				bool selected = StrEqual( model, state.selected_model );
				if( ImGui::Selectable( temp( "{}", model ), selected ) ) {
					Free( sys_allocator, state.selected_model );
					state.selected_model = ( *sys_allocator )( "{}", model );
				}
			}
		}

		ImGui::EndCombo();
	}

	Optional< ModelRenderData > maybe_model = FindModelRenderData( StringHash( state.selected_model ) );

	ImGui::SeparatorText( "Camera" );
	{
		ImGui::AlignTextToFramePadding();
		ImGui::Text( "Distance" );
		ImGui::SameLine();
		ImGui::SliderFloat( "##distance", &state.distance, 0.0f, 500.0f, "%.0f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat );

		ImGui::AlignTextToFramePadding();
		ImGui::Text( "Pitch" );
		ImGui::SameLine();
		ImGui::SliderFloat( "##pitch", &state.angles.pitch, -180.0f, 180.0f, "%.0f", ImGuiSliderFlags_NoRoundToFormat );

		ImGui::AlignTextToFramePadding();
		ImGui::Text( "Yaw" );
		ImGui::SameLine();
		ImGui::SliderFloat( "##yaw", &state.angles.yaw, 0.0f, 360.0f, "%.0f", ImGuiSliderFlags_NoRoundToFormat );
	}

	Vec3 camera_forward;
	AngleVectors( EulerDegrees3( state.angles ), &camera_forward, NULL, NULL );
	RendererSetView( state.distance * -camera_forward, EulerDegrees3( state.angles ), 90.0f );

	if( maybe_model.exists && maybe_model.value.type == ModelType_GLTF ) {
		const GLTFRenderData * gltf = maybe_model.value.gltf;

		ImGui::SeparatorText( "Animation" );
		ImGui::Checkbox( "Animate", &state.animate );

		u8 bipedal_split_node;
		bool bipedal = FindNodeByName( gltf, "mixamorig1:Spine", &bipedal_split_node );

		state.animation1.index = AnimationSelector( bipedal ? "Upper animation 1" : "Animation 1", gltf, state.animation1.index );
		if( bipedal ) {
			state.animation1.lower_index = AnimationSelector( "Lower animation 1", gltf, state.animation1.lower_index );
		}

		if( ImGui::Button( state.use_animation2 ? "Blend up" : "Blend down" ) ) {
			state.use_animation2 = !state.use_animation2;
			state.blend_end_time = cls.monotonicTime + blend_duration;
		}
		{
			Time blend_remaining = cls.monotonicTime < state.blend_end_time ? state.blend_end_time - cls.monotonicTime : Time { };
			float blend_factor = ToSeconds( blend_remaining ) / ToSeconds( blend_duration );
			if( state.use_animation2 ) {
				blend_factor = 1.0f - blend_factor;
			}
			ImGui::SameLine();
			ImGui::ProgressBar( blend_factor, Vec2( 0.0f ), "" );
		}

		state.animation2.index = AnimationSelector( bipedal ? "Upper animation 2" : "Animation 2", gltf, state.animation2.index );
		if( bipedal ) {
			state.animation2.lower_index = AnimationSelector( "Lower animation 2", gltf, state.animation2.lower_index );
		}

		state.additive_animation.index = AnimationSelector( "Additive animation", gltf, state.additive_animation.index );
		if( ImGui::Button( "Add" ) ) {
			state.additive_animation.start_time = cls.monotonicTime;
		}

		if( ImGui::Button( state.animation_paused ? "|>" : "||" ) ) {
			state.animation_paused = !state.animation_paused;
			state.animation1.start_time = cls.monotonicTime;
			state.animation2.start_time = cls.monotonicTime;
		}
		if( state.animation_paused ) {
			state.animation1.start_time = cls.monotonicTime;
			state.animation2.start_time = cls.monotonicTime;
		}
	}

	if( maybe_model.exists ) {
		DrawModelConfig config = {
			.draw_model = { .enabled = true },
			.draw_shadows = { .enabled = true },
		};

		MatrixPalettes pose = { };
		if( state.animate && maybe_model.value.type == ModelType_GLTF ) {
			const GLTFRenderData * gltf = maybe_model.value.gltf;
			u8 bipedal_split_node;
			bool bipedal = FindNodeByName( gltf, "mixamorig1:Spine", &bipedal_split_node );

			Span< const Transform > local1, local2;

			if( state.animation1.index < gltf->animations.n ) {
				float t1 = ToSeconds( cls.monotonicTime - state.animation1.start_time );
				float upper_duration = gltf->animations[ state.animation1.index ].duration;
				float upper_t = fmodf( t1, upper_duration );
				ImGui::ProgressBar( upper_t / upper_duration, Vec2( 0.0f ), "" );
				Span< Transform > upper = SampleAnimation( &temp, gltf, upper_t, state.animation1.index );

				if( state.additive_animation.index < gltf->animations.n ) {
					float add_duration = gltf->animations[ state.animation2.index ].duration;
					float add_t = ToSeconds( cls.monotonicTime - state.additive_animation.start_time );
					if( add_t < add_duration ) {
						Span< Transform > additive = SampleAnimation( &temp, gltf, add_t, state.additive_animation.index );
						upper = AddPoses( &temp, upper, additive );
					}
				}

				Span< Transform > lower = upper;
				if( bipedal && state.animation1.lower_index < gltf->animations.n ) {
					float lower_duration = gltf->animations[ state.animation1.lower_index ].duration;
					float lower_t = fmodf( t1, lower_duration );
					ImGui::ProgressBar( lower_t / lower_duration, Vec2( 0.0f ), "" );
					lower = SampleAnimation( &temp, gltf, lower_t, state.animation1.lower_index );
					MergeLowerUpperPoses( lower, upper, gltf, bipedal_split_node );
				}

				local1 = lower;
			}

			if( state.animation2.index < gltf->animations.n ) {
				float t2 = ToSeconds( cls.monotonicTime - state.animation2.start_time );
				float upper_duration = gltf->animations[ state.animation2.index ].duration;
				float upper_t = fmodf( t2, upper_duration );
				ImGui::ProgressBar( upper_t / upper_duration, Vec2( 0.0f ), "" );
				Span< Transform > upper = SampleAnimation( &temp, gltf, upper_t, state.animation2.index );
				Span< Transform > lower = upper;

				if( bipedal && state.animation2.lower_index < gltf->animations.n ) {
					float lower_duration = gltf->animations[ state.animation2.lower_index ].duration;
					float lower_t = fmodf( t2, lower_duration );
					ImGui::ProgressBar( lower_t / lower_duration, Vec2( 0.0f ), "" );
					lower = SampleAnimation( &temp, gltf, lower_t, state.animation2.lower_index );
					MergeLowerUpperPoses( lower, upper, gltf, bipedal_split_node );
				}

				local2 = lower;
			}

			Time blend_remaining = cls.monotonicTime < state.blend_end_time ? state.blend_end_time - cls.monotonicTime : Time { };
			float blend_factor = ToSeconds( blend_remaining ) / ToSeconds( blend_duration );
			if( state.use_animation2 ) {
				blend_factor = 1.0f - blend_factor;
			}
			pose = ComputeMatrixPalettes( &temp, gltf, LerpPoses( &temp, local1, blend_factor, local2 ) );
		}

		DrawModel( config, maybe_model.value, Mat3x4::Identity(), CG_TeamColorVec4( Team_One ), pose );
	}

	DrawSkybox( cls.shadertoy_time );
	DrawModelInstances();

	if( ImGui::Button( "Exit" ) ) {
		UI_ShowMainMenu();
	}

	ImGui::End();

	return []() {
		Free( sys_allocator, state.selected_model );
	};
}

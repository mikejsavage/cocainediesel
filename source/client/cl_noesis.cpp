#include "client/client.h"

#include "glad/glad.h"
#include "tracy/TracyOpenGL.hpp"

#define NS_STATIC_LIBRARY
#include "noesis/NsApp/ThemeProviders.h"
#include "noesis/NsApp/LocalXamlProvider.h"
#include "noesis/NsCore/Noesis.h"
#include "noesis/NsCore/RegisterComponent.h"
#include "noesis/NsRender/GLFactory.h"
#include "noesis/NsGui/IntegrationAPI.h"
#include "noesis/NsGui/IRenderer.h"
#include "noesis/NsGui/IView.h"
#include "noesis/NsGui/Page.h"
#include "noesis/NsGui/TextBlock.h"
#include "noesis/NsGui/TextBox.h"
#include "noesis/NsGui/Track.h"
#include "noesis/NsGui/Slider.h"
#include "noesis/NsGui/Button.h"

Noesis::IView * _view;
Noesis::Ptr<Noesis::Page> _xaml;

struct Retarded : public Noesis::BaseComponent {
	Noesis::String blah;
	NS_IMPLEMENT_INLINE_REFLECTION( Retarded, Noesis::BaseComponent ) {
		NsProp( "Blah", &Retarded::blah );
	}
};

Retarded mwaga;

void InitNoesis() {
	Noesis::SetLogHandler( []( const char*, uint32_t, uint32_t level, const char*, const char* msg ) {
		// [TRACE] [DEBUG] [INFO] [WARNING] [ERROR]
		const char * prefixes[] = { "T", "D", "I", "W", "E" };
		Com_Printf( "[NOESIS/%s] %s\n", prefixes[ level ], msg );
	} );

	Noesis::GUI::Init( "Cocaine Diesel", "B2CSqPonkqim52NhsjCdVmyiDElqlSzYnoyIbChoj8KIcLi3" );
	NoesisApp::SetThemeProviders();
	Noesis::GUI::LoadApplicationResources( "Theme/NoesisTheme.DarkBlue.xaml" );

	Noesis::RegisterComponent< Retarded >();

	Noesis::GUI::SetXamlProvider( Noesis::MakePtr< NoesisApp::LocalXamlProvider >( "" ) );
	_xaml = Noesis::GUI::LoadXaml<Noesis::Page>( "gui/MainWindow.xaml" );

	Noesis::Button* btn = _xaml->FindName< Noesis::Button >( "start" );
	btn->Click() += []( Noesis::BaseComponent * sender, const Noesis::RoutedEventArgs & args ) {
		Cbuf_AddText( "map carfentanil\n" );
	};

	_view = Noesis::GUI::CreateView( _xaml ).GiveOwnership();
	_view->SetFlags( Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD );
	_view->GetRenderer()->Init( NoesisApp::GLFactory::CreateDevice( true ) );
}

void ShutdownNoesis() {
	_view->GetRenderer()->Shutdown();
	Noesis::GUI::Shutdown();
}

void NoesisFrame( int width, int height ) {
	ZoneScoped;

	Noesis::TextBox* text = _xaml->FindName<Noesis::TextBox>( "text" );
	Noesis::Slider* slider = _xaml->FindName<Noesis::Slider>( "slider" );

	char buf[ 128 ];
	// snprintf( buf, sizeof( buf ), "%lld: %f %s", cls.monotonicTime, slider->GetTrack()->GetValue(), text->GetText() );
	snprintf( buf, sizeof( buf ), "%lld: %f %s", cls.monotonicTime, 1.0f, text->GetText() );
	mwaga.blah = buf;

	Noesis::TextBlock* test = _xaml->FindName<Noesis::TextBlock>( "test" );
	test->SetText( buf );

	_view->SetSize( width, height );

	_view->Update( cls.monotonicTime * 0.001 );

	TracyGpuZone( "Noesis GPU" );
	_view->GetRenderer()->UpdateRenderTree();
	_view->GetRenderer()->RenderOffscreen();

	_view->GetRenderer()->Render();
}

#include "client/client.h"

#include "glad/glad.h"
#include "tracy/TracyOpenGL.hpp"

#define NS_STATIC_LIBRARY
#include "noesis/NsApp/ThemeProviders.h"
#include "noesis/NsApp/NotifyPropertyChangedBase.h"
#include "noesis/NsCore/Noesis.h"
#include "noesis/NsCore/RegisterComponent.h"
#include "noesis/NsRender/GLFactory.h"
#include "noesis/NsGui/IntegrationAPI.h"
#include "noesis/NsGui/IRenderer.h"
#include "noesis/NsGui/IView.h"
#include "noesis/NsGui/Grid.h"
#include "noesis/NsGui/TextBlock.h"
#include "noesis/NsGui/TextBox.h"
#include "noesis/NsGui/Track.h"
#include "noesis/NsGui/Slider.h"
#include "noesis/NsGui/Button.h"

Noesis::IView * _view;
Noesis::Ptr<Noesis::Grid> _xaml;

class Retarded : public NoesisApp::NotifyPropertyChangedBase {
public:
	Retarded() { }

	void Set( const char * str ) {
		blah = str;
		OnPropertyChanged( "Blah" );
	}

private:
	Noesis::String blah;

	NS_IMPLEMENT_INLINE_REFLECTION( Retarded, BaseComponent ) {
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

	_xaml = Noesis::GUI::ParseXaml<Noesis::Grid>(R"ASDF(
	  <Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
	  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
		  <Viewbox>
			  <StackPanel Margin="50">
				  <Button x:Name="start" Content="Cocaine Diesel"/>
				  <TextBox x:Name="text" Width="100" Height="25" Text="asdf"/>
				  <Slider x:Name="slider" Width="100" Height="30" TickPlacement="TopLeft" />
				  <Rectangle x:name="rect" Height="50" RenderTransformOrigin="0.5,0.5">
					  <Rectangle.RenderTransform>
						<TranslateTransform X="0" Y="0"/>
					  </Rectangle.RenderTransform>

					  <Rectangle.Fill>
						  <RadialGradientBrush>
							  <GradientStop Offset="0" Color="#ffffffff"/>
							  <GradientStop Offset="1" Color="#80ffffff"/>
						  </RadialGradientBrush>
					  </Rectangle.Fill>
				  </Rectangle>
				  <Button Content="Start Animation" Margin="10" HorizontalAlignment="Left" VerticalAlignment="Top">
    <Button.Triggers>
      <EventTrigger RoutedEvent="Button.Click">
        <BeginStoryboard>
          <Storyboard TargetName="rect"
            TargetProperty="(UIElement.RenderTransform).(TranslateTransform.X)">
            <DoubleAnimation Duration="0:0:1" From="0" To="200"/>
          </Storyboard>
        </BeginStoryboard>
      </EventTrigger>
    </Button.Triggers>
  </Button>
				  <TextBlock x:Name="test" Foreground="white"/>
				  <TextBlock Foreground="white" Text="{Binding Blah, UpdateSourceTrigger=PropertyChanged}"/>
			  </StackPanel>
		  </Viewbox>
	  </Grid>
  )ASDF");

	_xaml->SetDataContext( &mwaga );

	Noesis::Button* btn = _xaml->FindName< Noesis::Button >( "start" );
	btn->Click() += []( Noesis::BaseComponent * sender, const Noesis::RoutedEventArgs & args ) {
		Cbuf_AddText( "map carfentanil\n" );
	};

	_view = Noesis::GUI::CreateView( _xaml ).GiveOwnership();
	_view->SetFlags( Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD );
	_view->GetRenderer()->Init( NoesisApp::GLFactory::CreateDevice() );
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
	mwaga.Set( buf );

	Noesis::TextBlock* test = _xaml->FindName<Noesis::TextBlock>( "test" );
	test->SetText( buf );

	_view->SetSize( width, height );

	_view->Update( cls.monotonicTime * 0.001 );

	TracyGpuZone( "Noesis GPU" );
	_view->GetRenderer()->UpdateRenderTree();
	_view->GetRenderer()->RenderOffscreen();

	_view->GetRenderer()->Render();
}

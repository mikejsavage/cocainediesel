#include "client/client.h"

#include "glad/glad.h"
#include "tracy/TracyOpenGL.hpp"

#define NS_STATIC_LIBRARY
#include "noesis/NsCore/Noesis.h"
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

static void OnMessage(const char* filename, uint32_t line, uint32_t level, const char* channel, const char* message)
{
	Com_Printf( "%s\n", message );
	// if (strcmp(channel, "") == 0)
	// {
	// 	// [TRACE] [DEBUG] [INFO] [WARNING] [ERROR]
	// 	const char* prefixes[] = { "T", "D", "I", "W", "E" };
	// 	const char* prefix = level < NS_COUNTOF(prefixes) ? prefixes[level] : " ";
	// 	fprintf(stderr, "[NOESIS/%s] %s\n", prefix, message);
	// }
}

void InitNoesis()
{
	Noesis::GUI::Init( NULL, OnMessage, NULL );

	_xaml = Noesis::GUI::ParseXaml<Noesis::Grid>(R"ASDF(
	  <Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
	  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
		  <Viewbox>
			  <StackPanel Margin="50">
				  <Button x:Name="start" Content="Cocaine Diesel" Margin="0,30,0,0"/>
				  <TextBox x:Name="text" Width="100" Height="25" />
				  <Slider x:Name="slider" Width="100" Height="30" Margin="8,4,8,2" TickPlacement="TopLeft" />
				  <Rectangle x:name="rect" Height="50" Margin="-10,100,-10,0" RenderTransformOrigin="0.5,0.5">
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
				  <TextBlock x:Name="test" Text="hello world"/>
			  </StackPanel>
		  </Viewbox>
	  </Grid>
  )ASDF");

	Noesis::Button* btn = _xaml->FindName<Noesis::Button>( "start" );
	btn->Click() += []( Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& args)
	{
		Cbuf_AddText( "map carfentanil\n" );
	};

	_view = Noesis::GUI::CreateView( _xaml ).GiveOwnership();
	_view->SetIsPPAAEnabled( true );

	Noesis::Ptr<Noesis::RenderDevice> device = NoesisApp::GLFactory::CreateDevice();
	_view->GetRenderer()->Init( device );
}

void ShutdownNoesis() {
	_view->GetRenderer()->Shutdown();
	Noesis::GUI::Shutdown();
}

void NoesisFrame( int width, int height ) {
	ZoneScoped;

	Noesis::TextBox* text = _xaml->FindName<Noesis::TextBox>( "text" );
	Noesis::Slider* slider = _xaml->FindName<Noesis::Slider>( "slider" );

	Noesis::TextBlock* test = _xaml->FindName<Noesis::TextBlock>( "test" );
	char buf[128];
	snprintf( buf, sizeof( buf ), "%lld: %f %s", cls.monotonicTime, slider->GetTrack()->GetValue(), text->GetText() );
	test->SetText( buf );

	_view->SetSize( width, height );

	_view->Update( cls.monotonicTime * 0.001 );

	TracyGpuZone( "Noesis GPU" );
	_view->GetRenderer()->UpdateRenderTree();
	_view->GetRenderer()->RenderOffscreen();

	_view->GetRenderer()->Render();
}

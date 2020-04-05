////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_IRENDERER_H__
#define __GUI_IRENDERER_H__


#include <NsCore/Noesis.h>
#include <NsCore/Interface.h>


namespace Noesis
{

class RenderDevice;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// This interface renders the UI tree. Using IView and IRenderer in different threads is allowed.
/// In fact, IRenderer is designed to be used in the render thread of your application.
////////////////////////////////////////////////////////////////////////////////////////////////////
NS_INTERFACE IRenderer: public Interface
{
    /// Initializes the renderer with the given render device
    virtual void Init(RenderDevice* device) = 0;

    /// Free allocated render resources and render tree
    virtual void Shutdown() = 0;

    /// Determines the visible region. By default it is set to cover the view dimensions
    virtual void SetRenderRegion(float x, float y, float width, float height) = 0;

    /// Applies last changes happened in the view. This function does not interact with the
    /// render device. Returns 'false' to indicate that no changes were applied and subsequent
    /// RenderOffscreen() and Render() calls could be avoided if last render was preserved
    virtual bool UpdateRenderTree() = 0;

    /// Indicates if offscreen textures are needed at the current render tree state. When this
    /// function returns true, it is mandatory to call RenderOffscreen() before Render()
    virtual bool NeedsOffscreen() const = 0;

    /// Generates offscreen textures. This function fills internal textures and must be invoked
    /// before binding the main render target. This is especially critical in tiled architectures.
    virtual void RenderOffscreen() = 0;

    /// Renders UI in the active render target and viewport dimensions
    virtual void Render(bool flipY = false) = 0;

    NS_IMPLEMENT_INLINE_REFLECTION_(IRenderer, Interface)
};

}

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __RENDER_GLFACTORY_H__
#define __RENDER_GLFACTORY_H__


#include <NsCore/Noesis.h>
#include <NsRender/GLRenderDeviceApi.h>
#include <NsRender/RenderDevice.h>


typedef unsigned int GLuint;


namespace Noesis
{

template<class T> class Ptr;

}

namespace NoesisApp
{

struct NS_RENDER_GLRENDERDEVICE_API GLFactory
{
    static Noesis::Ptr<Noesis::RenderDevice> CreateDevice();
    static Noesis::Ptr<Noesis::Texture> WrapTexture(GLuint object, uint32_t width, uint32_t height,
        uint32_t levels, bool isInverted);
};

}

#endif

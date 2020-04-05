////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_KERNEL_H__
#define __CORE_KERNEL_H__


#include <NsCore/Noesis.h>
#include <NsCore/KernelApi.h>
#include <NsCore/NSTLForwards.h>


namespace Noesis
{

class ReflectionRegistry;
class ComponentFactory;
class Symbol;
template<class T> class Delegate;
template<class T> class Ptr;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// First initialized object and the last one being shutdown.
/// Acts as a singleton that can be accessed by NsGetKernel()
////////////////////////////////////////////////////////////////////////////////////////////////////
class Kernel
{
public:
    /// Checks if kernel is initialized
    virtual bool IsInitialized() const = 0;

    /// Initializes the kernel. An optional memory allocator can be passed to control allocations
    virtual void Init() = 0;

    /// Shuts down the kernel
    virtual void Shutdown() = 0;

    /// Returns the thread where the kernel was initialized
    virtual unsigned int GetMainThreadId() const = 0;

    /// Access to Kernel Modules
    //@{
    virtual ReflectionRegistry* GetReflectionRegistry() const = 0;
    virtual ComponentFactory* GetComponentFactory() const = 0;
    //@}

    /// Remote console commands
    typedef Delegate<void (const char* params)> CommandDelegate;
    virtual void RegisterCommand(const char* command, const CommandDelegate& callback) = 0;
};

}

/// Returns kernel instance
NS_CORE_KERNEL_API Noesis::Kernel* NsGetKernel();

#ifdef NS_PROFILE
    #define NS_REGISTER_COMMAND(n, d) NsGetKernel()->RegisterCommand(n, d)
#else
    #define NS_REGISTER_COMMAND(n, d) NS_NOOP
#endif

#endif

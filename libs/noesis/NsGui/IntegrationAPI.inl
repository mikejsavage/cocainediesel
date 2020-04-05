
////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsCore/DynamicCast.h>


namespace Noesis
{
namespace GUI
{

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> Ptr<T> LoadXaml(const char* filename)
{
    Ptr<T> root;

    Ptr<BaseComponent> xaml = LoadXaml(filename);
    if (xaml != 0)
    {
        root = DynamicPtrCast<T>(xaml);
        if (root == 0)
        {
            NS_ERROR("LoadXaml('%s'): invalid requested root type", filename);
        }
    }

    return root;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> Ptr<T> ParseXaml(const char* xamlText)
{
    Ptr<T> root;

    Ptr<BaseComponent> xaml = ParseXaml(xamlText);
    if (xaml != 0)
    {
        root = DynamicPtrCast<T>(xaml);
        if (root == 0)
        {
            NS_ERROR("ParseXaml: invalid requested root type");
        }
    }

    return root;
}

}
}

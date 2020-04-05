////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_CONTENTPRESENTER_H__
#define __GUI_CONTENTPRESENTER_H__


#include <NsCore/Noesis.h>
#include <NsGui/FrameworkElement.h>


namespace Noesis
{

class DataTemplate;
class DataTemplateSelector;
class TextBlock;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Displays the content of ContentControl.
///
/// http://msdn.microsoft.com/en-us/library/system.windows.controls.contentpresenter.aspx
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_GUI_CORE_API ContentPresenter: public FrameworkElement
{
public:
    ContentPresenter();
    ~ContentPresenter();

    /// Gets or sets content
    //@{
    BaseComponent* GetContent() const;
    void SetContent(BaseComponent* content);
    void SetContent(const char* content);
    //@}

    /// Gets or sets the base name to use during automatic aliasing.
    //@{
    const char* GetContentSource() const;
    void SetContentSource(const char* source);
    //@}

    /// Gets or sets the template used to display the content of the control.
    //@{
    DataTemplate* GetContentTemplate() const;
    void SetContentTemplate(DataTemplate* value);
    //@}

    /// Gets or sets the DataTemplateSelector, which allows the application writer to provide
    /// custom logic for choosing the template that is used to display the content of the control.
    //@{
    DataTemplateSelector* GetContentTemplateSelector() const;
    void SetContentTemplateSelector(DataTemplateSelector* selector);
    //@}

public:
    /// Dependency properties
    //@{
    static const DependencyProperty* ContentProperty;
    static const DependencyProperty* ContentSourceProperty;
    static const DependencyProperty* ContentTemplateProperty;
    static const DependencyProperty* ContentTemplateSelectorProperty;
    //@}

protected:
    // Inheritors must call this function before measuring the content presenter
    void RefreshVisualTree();

    // Invoked when Content property changes
    virtual void OnContentChanged(BaseComponent* oldContent, BaseComponent* newContent);

    /// From DependencyProperty
    //@{
    void OnInit() override;
    bool OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) override;
    //@}

    /// From FrameworkElement
    //@{
    void CloneOverride(FrameworkElement* clone, FrameworkTemplate* template_) const override;
    Size MeasureOverride(const Size& availableSize) override;
    //@}

private:
    friend class ItemsControl;
    void PrepareContainer(BaseComponent* item, DataTemplate* itemTemplate,
        DataTemplateSelector* itemTemplateSelector);
    void ClearContainer(BaseComponent* item);

    void InvalidateVisualTree(bool disconnectChild = true);

    bool RemoveOldParent(Visual* child) const;

    TextBlock* GetDefaultTemplate() const;
    TextBlock* EnsureDefaultTemplate();
    void RemoveDefaultTemplate();

    bool UsingDefaultTemplate() const;
    bool UsingContentTemplate() const;

private:
    bool mRefreshVisualTree;

    NS_DECLARE_REFLECTION(ContentPresenter, FrameworkElement)
};

}

#endif

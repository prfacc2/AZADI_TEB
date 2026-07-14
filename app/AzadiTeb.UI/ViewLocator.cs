using System;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using AzadiTeb.UI.ViewModels;

namespace AzadiTeb.UI;

/// <summary>
/// Convention-based View resolution: FooViewModel -> FooView.
/// Keeps the MVVM wiring declarative and zero-boilerplate.
/// </summary>
public class ViewLocator : IDataTemplate
{
    public Control? Build(object? data)
    {
        if (data is null)
            return null;

        var name = data.GetType().FullName!
            .Replace("ViewModels", "Views")
            .Replace("ViewModel", "View");

        var type = Type.GetType(name);
        if (type != null)
            return (Control)Activator.CreateInstance(type)!;

        return new TextBlock { Text = "View not found: " + name };
    }

    public bool Match(object? data) => data is ViewModelBase;
}

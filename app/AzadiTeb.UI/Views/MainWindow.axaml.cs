using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace AzadiTeb.UI.Views;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();

        if (this.FindControl<Button>("CloseBtn") is { } close)
            close.Click += (_, _) => Close();
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
}

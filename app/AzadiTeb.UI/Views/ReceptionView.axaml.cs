using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using AzadiTeb.UI.Services;
using AzadiTeb.UI.ViewModels;

namespace AzadiTeb.UI.Views;

public partial class ReceptionView : UserControl
{
    public ReceptionView()
    {
        InitializeComponent();

        // Smart Jalali auto-slashing on the birth-date field, exactly like the
        // native form: user types digits, we insert the / separators live.
        if (this.FindControl<TextBox>("BirthDateBox") is { } bd)
        {
            bd.TextChanged += (_, _) =>
            {
                if (DataContext is not ReceptionViewModel vm) return;
                var formatted = PersianTools.SmartJalali(bd.Text ?? "");
                if (formatted != (bd.Text ?? ""))
                {
                    vm.BirthDate = formatted;
                    bd.CaretIndex = formatted.Length;
                }
            };
        }

        // Enter on the national-id field triggers the insurance lookup.
        if (this.FindControl<TextBox>("NationalIdBox") is { } nid)
        {
            nid.KeyDown += (_, e) =>
            {
                if (e.Key == Avalonia.Input.Key.Enter &&
                    DataContext is ReceptionViewModel vm &&
                    vm.LookupCitizenCommand.CanExecute(null))
                {
                    vm.LookupCitizenCommand.Execute(null);
                    e.Handled = true;
                }
            };
        }
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
}

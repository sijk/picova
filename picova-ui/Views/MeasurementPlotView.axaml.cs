using Avalonia;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace PicovaUI.Views
{
    public partial class MeasurementPlotView : UserControl
    {
        public MeasurementPlotView()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }
    }
}
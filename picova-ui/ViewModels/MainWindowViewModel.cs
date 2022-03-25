using System;
using System.Reactive.Linq;
using Avalonia.Threading;

namespace PicovaUI.ViewModels
{
    public class MainWindowViewModel : ViewModelBase
    {
        private readonly IO.MeasurementReader reader;

        public MeasurementPlotViewModel MeasurementPlot { get; }

        public MainWindowViewModel()
        {
            MeasurementPlot = new MeasurementPlotViewModel();

            reader = new IO.MeasurementReader("/dev/cu.usbmodem14201");
            reader.Measurements
                .Buffer(TimeSpan.FromMilliseconds(100))
                .ObserveOn(AvaloniaScheduler.Instance)
                .Subscribe(MeasurementPlot.AddMeasurements);
        }
    }
}

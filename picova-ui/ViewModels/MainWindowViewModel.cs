using System;
using System.Collections.ObjectModel;
using System.IO.Ports;
using System.Linq;
using System.Reactive;
using System.Reactive.Linq;
using Avalonia.Threading;
using ReactiveUI;
using ReactiveUI.Fody.Helpers;

namespace PicovaUI.ViewModels
{
    public class MainWindowViewModel : ViewModelBase
    {
        private readonly IO.MeasurementReader reader = new();

        public ReadOnlyCollection<string> SerialPorts => new(SerialPort.GetPortNames());
        [Reactive] public string? SelectedPort { get; set; }
        [ObservableAsProperty] public string RunLabel { get; } = string.Empty;
        public ReactiveCommand<Unit, Unit> Run { get; }
        public MeasurementPlotViewModel MeasurementPlot { get; } = new();

        public MainWindowViewModel()
        {
            var portSelected = this.WhenAnyValue(vm => vm.SelectedPort).Select(p => p != null);
            Run = ReactiveCommand.Create(() => {}, portSelected);

            Run.Subscribe(_ => 
            {
                if (!reader.Connected)
                    reader.Connect(SelectedPort!);
                else
                    reader.Disconnect();
            });

            this.WhenAnyValue(vm => vm.reader.Connected)
                .Select(con => con ? "Stop" : "Run")
                .ToPropertyEx(this, vm => vm.RunLabel);

            reader.Measurements
                .Buffer(TimeSpan.FromMilliseconds(100))
                .ObserveOn(AvaloniaScheduler.Instance)
                .Where(_ => reader.Connected)
                .Subscribe(MeasurementPlot.AddMeasurements);
        }
    }
}

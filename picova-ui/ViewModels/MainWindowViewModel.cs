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
        [ObservableAsProperty] public bool Running { get; }
        public MeasurementPlotViewModel MeasurementPlot { get; } = new();

        public double WindowSeconds
        {
            get => MeasurementPlot.TimeWindow.TotalSeconds;
            set
            {
                MeasurementPlot.TimeWindow = TimeSpan.FromSeconds(value);
                this.RaisePropertyChanged(nameof(WindowSeconds));
            }
        }

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
                .ToPropertyEx(this, vm => vm.Running);

            this.WhenAnyValue(vm => vm.Running)
                .Select(running => running ? "Stop" : "Run")
                .ToPropertyEx(this, vm => vm.RunLabel);

            reader.Measurements
                .Buffer(TimeSpan.FromMilliseconds(100))
                .ObserveOn(AvaloniaScheduler.Instance)
                .Where(_ => Running)
                .Subscribe(MeasurementPlot.AddMeasurements);
        }
    }
}

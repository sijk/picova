using System;
using System.Collections.ObjectModel;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Reactive;
using System.Reactive.Linq;
using Avalonia.Threading;
using PicovaUI.Models;
using ReactiveUI;
using ReactiveUI.Fody.Helpers;

namespace PicovaUI.ViewModels
{
    public class MainWindowViewModel : ViewModelBase
    {
        private readonly IO.MeasurementReader reader = new();

        public ReadOnlyCollection<string> SerialPorts => new(SerialPort.GetPortNames());
        [Reactive] public string? SelectedPort { get; set; }
        public ReadOnlyCollection<Filter> Filters => new(Enum.GetValues<Filter>());
        [ObservableAsProperty] public string RunLabel { get; } = string.Empty;
        public ReactiveCommand<Unit, Unit> Run { get; }
        [ObservableAsProperty] public bool Running { get; }
        public MeasurementPlotViewModel MeasurementPlot { get; } = new();
        public ReactiveCommand<Unit, Unit> SaveData { get; }

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
                {
                    MeasurementPlot.Clear();
                    reader.Connect(SelectedPort!);
                }
                else
                {
                    reader.Disconnect();
                }
            });

            SaveData = ReactiveCommand.Create(DoSaveData);

            this.WhenAnyValue(vm => vm.reader.Connected)
                .ToPropertyEx(this, vm => vm.Running);

            this.WhenAnyValue(vm => vm.Running)
                .Select(running => running ? "Stop" : "Run")
                .ToPropertyEx(this, vm => vm.RunLabel);

            reader.Measurements
                .ObserveOn(RxApp.TaskpoolScheduler)
                .Buffer(TimeSpan.FromMilliseconds(100))
                .Where(_ => Running)
                .Do(MeasurementPlot.AddMeasurements)
                .ObserveOn(AvaloniaScheduler.Instance)
                .Do(_ => MeasurementPlot.Redraw())
                .Subscribe();
        }

        private void DoSaveData()
        {
            var dst = Path.Combine(Environment.CurrentDirectory, $"PicoVA-{DateTime.Now:yyyyMMdd-HHmmss}.csv");
            using var file = new StreamWriter(dst);
            file.WriteLine("us,V,mA,mW");
            foreach (var m in MeasurementPlot.Measurements)
                file.WriteLine($"{m.Timestamp},{m.Voltage},{m.Current},{m.Power}");
        }
    }
}

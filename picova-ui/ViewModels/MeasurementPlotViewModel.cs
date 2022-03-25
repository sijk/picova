using OxyPlot;
using OxyPlot.Axes;
using OxyPlot.Series;
using PicovaUI.Models;
using System;
using System.Collections.Generic;

namespace PicovaUI.ViewModels
{
    public class MeasurementPlotViewModel : ViewModelBase
    {
        private readonly LineSeries vLine;
        private readonly LineSeries aLine;
        private readonly LineSeries wLine;

        public PlotModel Plot { get; }
        public TimeSpan TimeWindow { get; init; } = TimeSpan.FromSeconds(5);

        public MeasurementPlotViewModel()
        {
            var vAxis = new LinearAxis
            {
                Title = "Voltage [V]",
                Key = "V",
                StartPosition = 0.68,
                EndPosition = 1.0,
            };

            var aAxis = new LinearAxis
            {
                Title = "Current [mA]",
                Key = "A",
                StartPosition = 0.34,
                EndPosition = 0.66,
            };

            var wAxis = new LinearAxis
            {
                Title = "Power [mW]",
                Key = "W",
                StartPosition = 0,
                EndPosition = 0.32,
            };

            var tAxis = new LinearAxis
            {
                Title = "Time [µs]",
                Key = "T",
                Position = AxisPosition.Bottom,
            };

            vLine = new LineSeries
            {
                YAxisKey = "V",
            };

            aLine = new LineSeries
            {
                YAxisKey = "A",
            };

            wLine = new LineSeries
            {
                YAxisKey = "W",
            };

            Plot = new PlotModel();

            Plot.Axes.Add(vAxis);
            Plot.Axes.Add(aAxis);
            Plot.Axes.Add(wAxis);
            Plot.Axes.Add(tAxis);

            Plot.Series.Add(vLine);
            Plot.Series.Add(aLine);
            Plot.Series.Add(wLine);
        }

        public void AddMeasurements(IEnumerable<Measurement> measurements)
        {
            uint lastTime = 0;

            foreach (var meas in measurements)
            {
                vLine.Points.Add(new DataPoint(meas.Timestamp, meas.Voltage));
                aLine.Points.Add(new DataPoint(meas.Timestamp, meas.Current));
                wLine.Points.Add(new DataPoint(meas.Timestamp, meas.Power));
                lastTime = meas.Timestamp;
            }

            var minTime = lastTime - TimeWindow.TotalMilliseconds * 1000;
            var n = vLine.Points.FindIndex(p => p.X >= minTime);

            vLine.Points.RemoveRange(0, n);
            aLine.Points.RemoveRange(0, n);
            wLine.Points.RemoveRange(0, n);

            Plot.InvalidatePlot(true);
        }
    }
}

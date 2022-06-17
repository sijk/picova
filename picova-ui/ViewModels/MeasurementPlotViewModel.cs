using MathNet.Filtering;
using MathNet.Filtering.Median;
using OxyPlot;
using OxyPlot.Annotations;
using OxyPlot.Axes;
using OxyPlot.Series;
using PicovaUI.Models;
using ReactiveUI;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;

namespace PicovaUI.ViewModels
{
    public class MeasurementPlotViewModel : ViewModelBase
    {
        private readonly List<Measurement> meas = new();
        private readonly LineSeries vLine;
        private readonly LineSeries aLine;
        private readonly LineSeries wLine;
        private readonly TextAnnotation vLabel;
        private readonly TextAnnotation aLabel;
        private readonly TextAnnotation wLabel;
        private readonly OnlineFilter[] filters = new OnlineFilter[3];
        private Filter filterType;

        public PlotModel Plot { get; }
        public TimeSpan TimeWindow { get; set; } = TimeSpan.FromSeconds(5);
        public ReadOnlyCollection<Measurement> Measurements => new(meas);
        public Filter Filter
        {
            get => filterType;
            set
            {
                filterType = value;
                Refilter();
                this.RaisePropertyChanged(nameof(Filter));
            }
        }

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

            vLabel = new TextAnnotation
            {
                TextPosition = new DataPoint(0, 0),
                FontSize = 16,
                TextHorizontalAlignment = HorizontalAlignment.Right,
                StrokeThickness = 0,
                YAxisKey = "V",
            };

            aLabel = new TextAnnotation
            {
                TextPosition = new DataPoint(0, 0),
                FontSize = 16,
                TextHorizontalAlignment = HorizontalAlignment.Right,
                StrokeThickness = 0,
                YAxisKey = "A",
            };

            wLabel = new TextAnnotation
            {
                TextPosition = new DataPoint(0, 0),
                FontSize = 16,
                TextHorizontalAlignment = HorizontalAlignment.Right,
                StrokeThickness = 0,
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

            Plot.Annotations.Add(vLabel);
            Plot.Annotations.Add(aLabel);
            Plot.Annotations.Add(wLabel);

            Refilter();
        }

        public void AddMeasurements(IEnumerable<Measurement> measurements)
        {
            uint lastTime = 0;

            meas.AddRange(measurements);

            foreach (var m in measurements)
            {
                vLine.Points.Add(new DataPoint(m.Timestamp, filters[0].ProcessSample(m.Voltage)));
                aLine.Points.Add(new DataPoint(m.Timestamp, filters[1].ProcessSample(m.Current)));
                wLine.Points.Add(new DataPoint(m.Timestamp, filters[2].ProcessSample(m.Power)));
                lastTime = m.Timestamp;
            }

            if (meas.Count > 0)
            {
                Action<TextAnnotation> updatePosition = label => 
                {
                    var ax = Plot.GetAxis(label.YAxisKey);
                    var midAxis = ax.ActualMinimum + (ax.ActualMaximum - ax.ActualMinimum) / 2;
                    label.TextPosition = new DataPoint(lastTime, midAxis);
                };

                var latest = meas[^1];
                vLabel.Text = $"{latest.Voltage:F3} V";
                aLabel.Text = $"{latest.Current:F3} mA";
                wLabel.Text = $"{latest.Power:F3} mW";

                updatePosition(vLabel);
                updatePosition(aLabel);
                updatePosition(wLabel);
            }

            var minTime = lastTime - TimeWindow.TotalMilliseconds * 1000;
            var n = vLine.Points.FindIndex(p => p.X >= minTime);

            if (n > 0)
            {
                meas.RemoveRange(0, n);
                vLine.Points.RemoveRange(0, n);
                aLine.Points.RemoveRange(0, n);
                wLine.Points.RemoveRange(0, n);

                Plot.Axes.Single(ax => ax.Key == "T").Minimum = double.NaN;
            }
            else
            {
                Plot.Axes.Single(ax => ax.Key == "T").Minimum = minTime;
            }

            Plot.InvalidatePlot(true);
        }

        private void Refilter()
        {
            Func<OnlineFilter> makeFilter = filterType switch 
            {
                Filter.Median => () => new OnlineMedianFilter(7),
                _ => () => new OnlineIdentityFilter(),
            };

            filters[0] = makeFilter();
            filters[1] = makeFilter();
            filters[2] = makeFilter();

            vLine.Points.Clear();
            aLine.Points.Clear();
            wLine.Points.Clear();

            foreach (var m in meas)
            {
                vLine.Points.Add(new DataPoint(m.Timestamp, filters[0].ProcessSample(m.Voltage)));
                aLine.Points.Add(new DataPoint(m.Timestamp, filters[1].ProcessSample(m.Current)));
                wLine.Points.Add(new DataPoint(m.Timestamp, filters[2].ProcessSample(m.Power)));
            }

            Plot.InvalidatePlot(true);
        }

        private class OnlineIdentityFilter : OnlineFilter
        {
            public override double ProcessSample(double sample)
            {
                return sample;
            }

            public override void Reset()
            {
            }
        }
    }
}

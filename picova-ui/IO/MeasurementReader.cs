using System;
using System.IO.Ports;
using System.Reactive.Subjects;
using System.Text;
using PicovaUI.Models;
using ReactiveUI;

namespace PicovaUI.IO
{
    public class MeasurementReader : ReactiveObject, IDisposable
    {
        private static readonly string newline = "\r\n";

        private readonly SerialPort serial = new();
        private readonly StringBuilder buff = new();
        private readonly Subject<Measurement> measurements = new();

        public bool Connected => serial.IsOpen;
        public IObservable<Measurement> Measurements => measurements;

        public void Connect(string port)
        {
            serial.PortName = port;
            serial.DtrEnable = true;
            serial.RtsEnable = true;
            serial.Open();
            serial.DataReceived += OnDataReceived;
            this.RaisePropertyChanged(nameof(Connected));
        }

        public void Disconnect()
        {
            serial.DataReceived -= OnDataReceived;
            serial.Close();
            this.RaisePropertyChanged(nameof(Connected));
        }

        public void Dispose()
        {
            ((IDisposable)serial).Dispose();
        }

        private void OnDataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            if (e.EventType != SerialData.Chars)
                return;

            try
            {
                buff.Append(serial.ReadExisting());
            }
            catch
            {
                return;
            }

            var str = buff.ToString();
            var end = str.IndexOf(newline);
            while (end >= 0)
            {
                var line = str[..end];

                var meas = Parse(line);
                if (meas != null)
                    measurements.OnNext(meas);

                buff.Remove(0, end + newline.Length);
                str = buff.ToString();
                end = str.IndexOf(newline);
            }
        }

        private Measurement? Parse(string data)
        {
            var fields = data.Split(',');
            if (fields.Length != 4)
                return null;

            return new Measurement
            {
                Timestamp = uint.Parse(fields[0]),
                Voltage = float.Parse(fields[1]),
                Current = float.Parse(fields[2]),
                Power = float.Parse(fields[3]),
            };
        }
    }
}

using System;
using System.IO.Ports;
using System.Reactive.Subjects;
using System.Text;
using PicovaUI.Models;
using ReactiveUI;

namespace PicovaUI.IO
{
    public class MeasurementReader : ReactiveObject
    {
        private static readonly string newline = "\r\n";

        private readonly SerialPort serial;
        private readonly StringBuilder buff = new();
        private readonly Subject<Measurement> measurements = new();

        public IObservable<Measurement> Measurements => measurements;

        public MeasurementReader(string port)
        {
            serial = new SerialPort(port);
            serial.Open();
            serial.DtrEnable = true;
            serial.RtsEnable = true;
            serial.DataReceived += OnDataReceived;
        }

        private void OnDataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            if (e.EventType != SerialData.Chars)
                return;

            buff.Append(serial.ReadExisting());

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

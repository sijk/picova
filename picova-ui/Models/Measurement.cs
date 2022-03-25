namespace PicovaUI.Models
{
    public record Measurement
    {
        public uint Timestamp { get; init; }
        public float Voltage { get; init; }
        public float Current { get; init; }
        public float Power { get; init; }
    }
}

import sys
from queue import Empty, Queue

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
from matplotlib.figure import Figure
from serial import Serial
from serial.threaded import LineReader, ReaderThread


class SerialReader(LineReader):
    def __init__(self, queue: Queue):
        super().__init__()
        self.queue = queue

    def handle_line(self, line):
        self.queue.put(line)


class Plotter:
    def __init__(self, fig: Figure, queue: Queue, window_sec = 10):
        self.queue = queue
        self.window_sec = window_sec
        self.axes = fig.subplots(3, 1, sharex=True)
        self.lines = [ax.plot([], [])[0] for ax in self.axes]

        fig.set_tight_layout(True)
        self.axes[0].set_ylabel('Voltage (V)')
        self.axes[1].set_ylabel('Current (mA)')
        self.axes[2].set_ylabel('Power (mW)')

    def update(self, _):
        new_t = []
        new_data = ([], [], [])

        # Retrieve all new data from the queue
        while not self.queue.empty():
            try:
                line: str = self.queue.get_nowait()
            except Empty:
                break

            try:
                t, v, a, w = tuple(map(float, line.split(',')))
            except ValueError:
                continue

            new_t.append(t / 1e6)
            new_data[0].append(v)
            new_data[1].append(a)
            new_data[2].append(w)

        # Update the time axis
        t = np.append(self.lines[0].get_xdata(), new_t)
        max_t = t[-1]
        min_t = max_t - self.window_sec
        n = t.searchsorted(min_t)
        t = t[n:]

        # Update the lines and autoscale Y axes
        for ax, line, data in zip(self.axes, self.lines, new_data):
            y = np.append(line.get_ydata(), data)[n:]

            min_y = y.min()
            max_y = y.max()
            pad_y = 0.1 * (max_y - min_y)
            min_y -= pad_y
            max_y += pad_y

            ax.set_xlim(min_t, max_t)
            ax.set_ylim(min_y, max_y)
            line.set_data(t, y)

        return self.lines


class PowerScope:
    def __init__(self, port: str) -> None:
        queue = Queue()
        fig = plt.figure()
        fig.canvas.manager.set_window_title('Power Scope')

        self.serial = Serial(port)
        self.reader = ReaderThread(self.serial, lambda: SerialReader(queue))

        self.scope = Plotter(fig, queue)
        self.anim = FuncAnimation(fig, self.scope.update, interval=1000/25, save_count=0)

    def run(self):
        with self.reader:
            plt.show()


if __name__ == '__main__':
    PowerScope(sys.argv[1]).run()

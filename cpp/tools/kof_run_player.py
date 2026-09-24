#!/usr/bin/env python3
"""Simple Tk window to play the KOF run PNG sequence."""

from __future__ import print_function
from __future__ import annotations

import glob
import os

try:
    import tkinter as tk
    from tkinter import ttk
except ImportError:
    import Tkinter as tk
    import ttk

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FRAME_DIR = os.path.join(ROOT, "cpp", "tmp_kof_run")


def find_frames(folder: str) -> list[str]:
    for pattern in ("play_*.png", "world_*.png", "run_*.png"):
        files = sorted(glob.glob(os.path.join(folder, pattern)))
        if files:
            return files
    return []


class RunPlayer(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("KOF Run Player")
        self.geometry("980x720")
        self.frames = find_frames(FRAME_DIR)
        self.index = 0
        self.playing = False
        self.delay_ms = 42  # ~24 fps
        self.photos: list[tk.PhotoImage] = []

        self.label = tk.Label(self, bg="#222")
        self.label.pack(fill=tk.BOTH, expand=True)

        bar = ttk.Frame(self)
        bar.pack(fill=tk.X, padx=8, pady=8)
        self.btn_play = ttk.Button(bar, text="播放", command=self.toggle)
        self.btn_play.pack(side=tk.LEFT, padx=4)
        ttk.Button(bar, text="上一帧", command=self.prev_frame).pack(side=tk.LEFT, padx=4)
        ttk.Button(bar, text="下一帧", command=self.next_frame).pack(side=tk.LEFT, padx=4)
        ttk.Button(bar, text="重载图片", command=self.reload).pack(side=tk.LEFT, padx=4)

        self.status = ttk.Label(bar, text="")
        self.status.pack(side=tk.RIGHT)

        if not self.frames:
            self.status.config(text=f"没有找到序列帧: {FRAME_DIR}")
        else:
            self.reload()
            self.show(0)

    def reload(self) -> None:
        self.frames = find_frames(FRAME_DIR)
        self.photos = []
        for path in self.frames:
            img = tk.PhotoImage(file=path)
            # downsample if huge
            w = int(img.width())
            if w > 960:
                factor = max(2, round(w / 960))
                img = img.subsample(factor, factor)
            self.photos.append(img)
        self.status.config(text=f"{len(self.frames)} 帧  {FRAME_DIR}")
        if self.photos:
            self.show(self.index)

    def show(self, i: int) -> None:
        if not self.photos:
            return
        self.index = i % len(self.photos)
        self.label.config(image=self.photos[self.index])
        self.status.config(
            text=f"{self.index + 1}/{len(self.photos)}  {os.path.basename(self.frames[self.index])}"
        )

    def toggle(self) -> None:
        self.playing = not self.playing
        self.btn_play.config(text="暂停" if self.playing else "播放")
        if self.playing:
            self.tick()

    def tick(self) -> None:
        if not self.playing:
            return
        self.show(self.index + 1)
        self.after(self.delay_ms, self.tick)

    def prev_frame(self) -> None:
        self.playing = False
        self.btn_play.config(text="播放")
        self.show(self.index - 1)

    def next_frame(self) -> None:
        self.playing = False
        self.btn_play.config(text="播放")
        self.show(self.index + 1)


if __name__ == "__main__":
    RunPlayer().mainloop()

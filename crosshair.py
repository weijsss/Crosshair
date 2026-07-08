"""
外置准星 v2.0
Apple-style dark theme, custom radio & checkbox widgets.
"""
import tkinter as tk
from tkinter import colorchooser
import json
import os
import sys
import webbrowser
import ctypes

if getattr(sys, 'frozen', False):
    DIR = os.path.dirname(sys.executable)
else:
    DIR = os.path.dirname(os.path.abspath(__file__))

CFG = os.path.join(DIR, "crosshair_config.json")
ICON = os.path.join(DIR, "crosshair.ico")

DEFAULT = {
    "color": "#00FF00", "alpha": 0.85, "size": 20,
    "thickness": 2, "gap": 4, "style": "cross", "visible": True,
    "offset_x": 0, "offset_y": 0,
}

COLORS = [
    "#00FF00", "#FF4444", "#00D4FF", "#FF44FF",
    "#FFDD44", "#FFFFFF", "#FF8800",
]
STYLES = ["cross", "circle", "dot", "crosshair", "corner"]
STYLE_NAMES = ["十字", "圆圈", "圆点", "实心十字", "四角"]

# ---- Apple dark palette ----
BG      = "#1c1c1e"
CARD    = "#2c2c2e"
TEXT    = "#f5f5f7"
SUBTLE  = "#98989d"
ACCENT  = "#0a84ff"
GREEN   = "#30d158"
SEP     = "#38383a"
PREVIEW = "#000000"
RING    = "#636366"   # radio/checkbox border


def load():
    try:
        with open(CFG, "r", encoding="utf-8") as f:
            c = json.load(f)
        for k, v in DEFAULT.items():
            c.setdefault(k, v)
        return c
    except:
        return dict(DEFAULT)


def save(c):
    with open(CFG, "w", encoding="utf-8") as f:
        json.dump(c, f, indent=2, ensure_ascii=False)


# ---------------------------------------------------------------
#  Apple-style custom widgets
# ---------------------------------------------------------------
RADIO_SIZE = 12
CHECK_SIZE = 14


class RadioGroup:
    """Apple-style 实心单选按钮组（网格布局）"""
    def __init__(self, parent, options, var, command, bg, cols=3):
        self.var = var
        self.command = command
        self.widgets = []
        for i, (value, label) in enumerate(options):
            f = tk.Frame(parent, bg=bg)
            c = tk.Canvas(f, width=RADIO_SIZE + 4, height=RADIO_SIZE + 4,
                          bg=bg, highlightthickness=0)
            c.pack(side="left")
            tk.Label(f, text=label, fg=TEXT, bg=bg,
                     font=("Microsoft YaHei UI", 10)).pack(side="left", padx=(2, 0))
            c.bind("<Button-1>", lambda e, v=value: self._select(v))
            f.children["!label"].bind("<Button-1>", lambda e, v=value: self._select(v))
            f.grid(row=i // cols, column=i % cols, sticky="w", padx=(0, 8), pady=3)
            self.widgets.append((c, value, label))
        self._draw()

    def _select(self, v):
        self.var.set(v)
        self._draw()
        self.command()

    def _draw(self):
        cur = self.var.get()
        for c, v, _ in self.widgets:
            c.delete("all")
            x, y = 2 + RADIO_SIZE // 2, 2 + RADIO_SIZE // 2
            r = RADIO_SIZE // 2
            if v == cur:
                c.create_oval(x - r, y - r, x + r, y + r,
                              fill=ACCENT, outline=ACCENT)
                c.create_oval(x - 3, y - 3, x + 3, y + 3, fill="#fff", outline="")
            else:
                c.create_oval(x - r, y - r, x + r, y + r,
                              outline=RING, width=1.5)


class CheckSwitch:
    """Apple-style 实心复选框"""
    def __init__(self, parent, text, var, bg):
        self.var = var
        f = tk.Frame(parent, bg=bg)
        self.cv = tk.Canvas(f, width=CHECK_SIZE + 4, height=CHECK_SIZE + 4,
                            bg=bg, highlightthickness=0)
        self.cv.pack(side="left")
        tk.Label(f, text=text, fg=TEXT, bg=bg,
                 font=("Microsoft YaHei UI", 10)).pack(side="left", padx=(2, 0))
        self.cv.bind("<Button-1>", self._toggle)
        f.children["!label"].bind("<Button-1>", self._toggle)
        self.widget = f
        self._draw()

    def _toggle(self, *a):
        self.var.set(not self.var.get())
        self._draw()

    def _draw(self):
        self.cv.delete("all")
        x, y = 2, 2
        s = CHECK_SIZE
        r = 4
        if self.var.get():
            self.cv.create_rounded_rect(x, y, x + s, y + s, r,
                                        fill=ACCENT, outline=ACCENT)
            # white checkmark
            self.cv.create_line(x + 3, y + s // 2, x + s // 2 + 1, y + s - 4,
                                fill="#fff", width=2, capstyle="round")
            self.cv.create_line(x + s // 2 + 1, y + s - 4, x + s - 2, y + 2,
                                fill="#fff", width=2, capstyle="round")
        else:
            self.cv.create_rounded_rect(x, y, x + s, y + s, r,
                                        outline=RING, width=1.5)

    def pack(self, **kw):
        self.widget.pack(**kw)


def _create_rounded_rect(canvas, x1, y1, x2, y2, r, **kw):
    """Canvas 圆角矩形 helper"""
    pts = [x1 + r, y1, x2 - r, y1,
           x2, y1, x2, y1 + r,
           x2, y2 - r, x2, y2,
           x2 - r, y2, x1 + r, y2,
           x1, y2, x1, y2 - r,
           x1, y1 + r, x1, y1]
    return canvas.create_polygon(pts, smooth=True, **kw)

tk.Canvas.create_rounded_rect = _create_rounded_rect


# ---------------------------------------------------------------
#  App
# ---------------------------------------------------------------
class App:
    def __init__(self):
        self.cfg = load()
        self.root = tk.Tk()
        self.root.title("Crosshair")
        self.root.resizable(False, False)
        self.root.configure(bg=BG)

        if os.path.exists(ICON):
            try:
                self.root.iconbitmap(ICON)
            except:
                pass

        pw, ph = 340, 790
        sw = self.root.winfo_screenwidth()
        sh = self.root.winfo_screenheight()
        self.root.geometry(f"{pw}x{ph}+{sw - pw - 30}+{sh - ph - 80}")

        self._build_ui()
        self._apply_to_overlay()
        self.root.protocol("WM_DELETE_WINDOW", self.quit)
        self.root.after(100, self._create_overlay)

    # ------------------------------------------------------------------
    def _build_ui(self):
        main = tk.Frame(self.root, bg=BG, padx=16, pady=12)
        main.pack(fill="both", expand=True)

        # title
        hf = tk.Frame(main, bg=BG)
        hf.pack(fill="x", pady=(0, 10))
        tk.Label(hf, text="Crosshair", font=("Microsoft YaHei UI", 17, "bold"),
                 fg=TEXT, bg=BG).pack(side="left")
        tk.Label(hf, text="v2.0", font=("Microsoft YaHei UI", 10),
                 fg=SUBTLE, bg=BG).pack(side="left", padx=(6, 0))
        bl = tk.Label(hf, text="B站", font=("Microsoft YaHei UI", 9, "underline"),
                      fg=ACCENT, bg=BG, cursor="hand2")
        bl.pack(side="right")
        bl.bind("<Button-1>", lambda e: webbrowser.open(
            "https://space.bilibili.com/187682531"))

        # preview
        self._label(main, "预览")
        pf = tk.Frame(main, bg=CARD, padx=1, pady=1, highlightthickness=0)
        pf.pack(fill="x", pady=(0, 12))
        self.pc = tk.Canvas(pf, width=300, height=64, bg=PREVIEW,
                            bd=0, highlightthickness=0)
        self.pc.pack()
        self._pi = []

        # style
        self._label(main, "形状")
        sf = tk.Frame(main, bg=CARD, padx=10, pady=10)
        sf.pack(fill="x", pady=(0, 12))
        self.sv = tk.StringVar(value=self.cfg["style"])
        RadioGroup(sf, list(zip(STYLES, STYLE_NAMES)),
                   self.sv, self._refresh, CARD)

        # color
        self._label(main, "颜色")
        cf = tk.Frame(main, bg=CARD, padx=10, pady=10)
        cf.pack(fill="x", pady=(0, 12))
        for i, c in enumerate(COLORS):
            s = 26
            f = tk.Frame(cf, bg=CARD)
            f.grid(row=0, column=i, padx=3)
            inner = tk.Canvas(f, width=s, height=s, bg=c, bd=0,
                              highlightthickness=0, cursor="hand2")
            inner.pack()
            if c == self.cfg["color"]:
                inner.create_oval(2, 2, s - 2, s - 2, outline="#fff", width=2)
            inner.bind("<Button-1>", lambda e, x=c: self._set_color(x))
        # custom color button
        sf2 = tk.Frame(cf, bg=CARD, padx=0, pady=0)
        sf2.grid(row=0, column=len(COLORS), padx=3)
        self._cc = tk.Canvas(sf2, width=26, height=26,
                             bg=self.cfg["color"], bd=0,
                             highlightthickness=0, cursor="hand2")
        self._cc.pack()
        self._cc.create_line(8, 13, 18, 13, fill="#fff", width=2)
        self._cc.create_line(13, 8, 13, 18, fill="#fff", width=2)
        self._cc.bind("<Button-1>", self._pick_color)

        # sliders
        self._label(main, "参数")
        sf3 = tk.Frame(main, bg=CARD, padx=10, pady=8)
        sf3.pack(fill="x", pady=(0, 12))
        self._labs = {}
        self._mk_slider(sf3, "尺寸", "size", 4, 80, 0)
        self._mk_slider(sf3, "粗细", "thickness", 1, 12, 1)
        self._mk_slider(sf3, "间隙", "gap", 0, 30, 2)
        self._mk_slider(sf3, "透明度", "alpha", 20, 100, 3, True)

        # position
        self._label(main, "位置")
        pf4 = tk.Frame(main, bg=CARD, padx=10, pady=8)
        pf4.pack(fill="x", pady=(0, 12))
        self._mk_slider(pf4, "水平", "offset_x", -300, 300, 0)
        self._mk_slider(pf4, "垂直", "offset_y", -300, 300, 1)

        # force topmost
        self._label(main, "覆盖")
        tf = tk.Frame(main, bg=CARD, padx=10, pady=8)
        tf.pack(fill="x", pady=(0, 12))
        self.ft = tk.BooleanVar(value=self.cfg.get("force_topmost", False))
        self._toggle_w = CheckSwitch(tf, "强制置顶", self.ft, CARD)
        self._toggle_w.pack(anchor="w")

        # buttons
        btnf = tk.Frame(main, bg=BG)
        btnf.pack(fill="x", pady=(4, 0))
        self._btn(btnf, "显示/隐藏", self.toggle, CARD, TEXT)
        self._btn(btnf, "应用", self._apply, ACCENT, "#fff")
        self._btn(btnf, "退出设置", self._hide_settings, CARD, TEXT)

        self.root.update()
        self._refresh()

    def _label(self, parent, text):
        tk.Label(parent, text=text, font=("Microsoft YaHei UI", 9, "bold"),
                 fg=SUBTLE, bg=BG).pack(anchor="w", pady=(0, 4))

    def _btn(self, parent, text, cmd, bg_c, fg_c):
        b = tk.Button(parent, text=text, command=cmd,
                      bg=bg_c, fg=fg_c, activebackground=ACCENT,
                      activeforeground="#fff", relief="flat", bd=0,
                      padx=12, pady=4, font=("Microsoft YaHei UI", 9),
                      cursor="hand2")
        b.pack(side="left", padx=(0, 8))

    # ------------------------------------------------------------------
    def _mk_slider(self, p, label, key, mn, mx, row, alpha=False):
        f = tk.Frame(p, bg=CARD)
        f.grid(row=row, column=0, sticky="ew", pady=3)
        tk.Label(f, text=label, width=5, anchor="e",
                 fg=TEXT, bg=CARD, font=("Microsoft YaHei UI", 9)).pack(
                     side="left", padx=(0, 6))
        v = self.cfg[key]
        if alpha:
            v = int(v * 100)
        var = tk.IntVar(value=v)
        setattr(self, f"_v_{key}", var)
        tk.Scale(f, from_=mn, to=mx, orient="horizontal", variable=var,
                 length=155, showvalue=False,
                 bg=CARD, fg=CARD, troughcolor=SEP,
                 activebackground=ACCENT, highlightthickness=0, bd=0,
                 command=lambda _, k=key, a=alpha: self._on_slide(k, a)
                 ).pack(side="left")
        ent = tk.Entry(f, width=5, fg=TEXT, bg=PREVIEW,
                       insertbackground=TEXT, bd=0,
                       font=("Microsoft YaHei UI", 9), justify="center")
        ent.pack(side="left", padx=(4, 0))
        ent.bind("<Return>", lambda e, k=key, a=alpha, m=mn, x=mx:
                 self._on_entry(k, a, m, x))
        ent.bind("<FocusOut>", lambda e, k=key, a=alpha, m=mn, x=mx:
                 self._on_entry(k, a, m, x))
        self._labs[key] = (ent, alpha, mn, mx)
        self._upd_lab(key, alpha)

    def _upd_lab(self, key, alpha):
        if key not in self._labs:
            return
        ent, af, _, _ = self._labs[key]
        v = getattr(self, f"_v_{key}").get()
        t = f"{v}%{'' if af else ''}" if af else str(v)
        # don't update while user is editing
        if ent.get() != t:
            ent.delete(0, "end")
            ent.insert(0, t)

    def _on_entry(self, key, alpha, mn, mx):
        ent, _, _, _ = self._labs[key]
        try:
            raw = ent.get().replace("%", "")
            v = int(raw)
            v = max(mn, min(mx, v))
            var = getattr(self, f"_v_{key}")
            var.set(v)
            self.cfg[key] = v / 100.0 if alpha else v
            self._upd_lab(key, alpha)
            self._refresh()
            self._apply_to_overlay()
        except ValueError:
            self._upd_lab(key, alpha)

    def _on_slide(self, key, alpha):
        if key not in self._labs:
            return
        v = getattr(self, f"_v_{key}").get()
        self.cfg[key] = v / 100.0 if alpha else v
        self._upd_lab(key, alpha)
        self._refresh()
        self._apply_to_overlay()

    def _set_color(self, c):
        self.cfg["color"] = c
        self._refresh()

    def _pick_color(self, *a):
        c = colorchooser.askcolor(color=self.cfg["color"],
                                  title="选择颜色")
        if c[1]:
            self._set_color(c[1])
            self._cc.configure(bg=c[1])

    # ------------------------------------------------------------------
    def _refresh(self):
        self.cfg["style"] = self.sv.get()
        for i in self._pi:
            self.pc.delete(i)
        self._pi.clear()
        cx, cy = 150, 32
        color = self.cfg["color"]
        s = int(self.cfg["size"])
        t = int(self.cfg["thickness"])
        g = int(self.cfg["gap"])
        style = self.cfg["style"]
        self._pi = []
        if style == "cross":
            self._pi.append(self.pc.create_line(
                cx, cy - g, cx, cy - g - s, fill=color, width=t))
            self._pi.append(self.pc.create_line(
                cx, cy + g, cx, cy + g + s, fill=color, width=t))
            self._pi.append(self.pc.create_line(
                cx - g, cy, cx - g - s, cy, fill=color, width=t))
            self._pi.append(self.pc.create_line(
                cx + g, cy, cx + g + s, cy, fill=color, width=t))
            r = max(1, t // 2)
            self._pi.append(self.pc.create_oval(
                cx - r, cy - r, cx + r, cy + r, fill=color, outline=color))
        elif style == "circle":
            r = s
            self._pi.append(self.pc.create_oval(
                cx - r, cy - r, cx + r, cy + r, outline=color, width=t))
            self._pi.append(self.pc.create_oval(
                cx - 1, cy - 1, cx + 1, cy + 1, fill=color, outline=color))
        elif style == "dot":
            r = s
            self._pi.append(self.pc.create_oval(
                cx - r, cy - r, cx + r, cy + r, fill=color, outline=color))
        elif style == "crosshair":
            ll = s
            self._pi.append(self.pc.create_line(
                cx, cy - ll, cx, cy + ll, fill=color, width=t))
            self._pi.append(self.pc.create_line(
                cx - ll, cy, cx + ll, cy, fill=color, width=t))
        elif style == "corner":
            ll, g2 = s, g
            self._pi.append(self.pc.create_line(
                cx - g2, cy - g2 - ll, cx - g2, cy - g2, fill=color, width=t))
            self._pi.append(self.pc.create_line(
                cx - g2 - ll, cy - g2, cx - g2, cy - g2, fill=color, width=t))
            self._pi.append(self.pc.create_line(
                cx + g2, cy - g2 - ll, cx + g2, cy - g2, fill=color, width=t))
            self._pi.append(self.pc.create_line(
                cx + g2 + ll, cy - g2, cx + g2, cy - g2, fill=color, width=t))
            self._pi.append(self.pc.create_line(
                cx - g2, cy + g2 + ll, cx - g2, cy + g2, fill=color, width=t))
            self._pi.append(self.pc.create_line(
                cx - g2 - ll, cy + g2, cx - g2, cy + g2, fill=color, width=t))
            self._pi.append(self.pc.create_line(
                cx + g2, cy + g2 + ll, cx + g2, cy + g2, fill=color, width=t))
            self._pi.append(self.pc.create_line(
                cx + g2 + ll, cy + g2, cx + g2, cy + g2, fill=color, width=t))

    def _apply(self):
        self.cfg["style"] = self.sv.get()
        self.cfg["force_topmost"] = self.ft.get()
        self._apply_to_overlay()
        save(self.cfg)

    def _apply_to_overlay(self):
        if not hasattr(self, "_ov") or self._ov is None:
            return
        try:
            self._ov.winfo_exists()
        except:
            return
        self._ov.wm_attributes("-alpha", self.cfg["alpha"])
        self._draw_overlay()
        if self.cfg.get("force_topmost", False):
            self._ov.attributes("-topmost", True)

    def _create_overlay(self):
        self.sw = self.root.winfo_screenwidth()
        self.sh = self.root.winfo_screenheight()
        self._ov = tk.Toplevel(self.root)
        self._ov.title("crosshair_overlay")
        self._ov.overrideredirect(True)
        self._ov.geometry(f"{self.sw}x{self.sh}+0+0")
        self._ov.configure(bg="black")
        self._ov.wm_attributes("-transparentcolor", "black")
        self._ov.wm_attributes("-alpha", self.cfg["alpha"])
        self._ov.attributes("-topmost", True)
        self._ov.protocol("WM_DELETE_WINDOW", lambda: None)

        # 鼠标穿透: WS_EX_TRANSPARENT | WS_EX_LAYERED
        self._ov.update()
        hwnd = ctypes.windll.user32.GetParent(self._ov.winfo_id())
        ex = ctypes.windll.user32.GetWindowLongW(hwnd, -20)
        ctypes.windll.user32.SetWindowLongW(hwnd, -20,
            ex | 0x00000020 | 0x00080000)

        self._cv = tk.Canvas(self._ov, width=self.sw, height=self.sh,
                             bg="black", bd=0, highlightthickness=0)
        self._cv.pack()
        self._ci = []
        self._draw_overlay()
        if self.cfg.get("force_topmost", False):
            self._topmost_loop()

    def _draw_overlay(self):
        if not hasattr(self, "_cv"):
            return
        for i in self._ci:
            self._cv.delete(i)
        self._ci.clear()
        ox = int(self.cfg.get("offset_x", 0))
        oy = int(self.cfg.get("offset_y", 0))
        cx, cy = self.sw // 2 + ox, self.sh // 2 + oy
        c = self.cfg["color"]
        s = int(self.cfg["size"])
        w = int(self.cfg["thickness"])
        g = int(self.cfg["gap"])
        style = self.cfg["style"]
        if style == "cross":
            self._ci.append(self._cv.create_line(
                cx, cy - g, cx, cy - g - s, fill=c, width=w))
            self._ci.append(self._cv.create_line(
                cx, cy + g, cx, cy + g + s, fill=c, width=w))
            self._ci.append(self._cv.create_line(
                cx - g, cy, cx - g - s, cy, fill=c, width=w))
            self._ci.append(self._cv.create_line(
                cx + g, cy, cx + g + s, cy, fill=c, width=w))
            r = max(1, w // 2)
            self._ci.append(self._cv.create_oval(
                cx - r, cy - r, cx + r, cy + r, fill=c, outline=c))
        elif style == "circle":
            self._ci.append(self._cv.create_oval(
                cx - s, cy - s, cx + s, cy + s, outline=c, width=w))
            self._ci.append(self._cv.create_oval(
                cx - 1, cy - 1, cx + 1, cy + 1, fill=c, outline=c))
        elif style == "dot":
            self._ci.append(self._cv.create_oval(
                cx - s, cy - s, cx + s, cy + s, fill=c, outline=c))
        elif style == "crosshair":
            self._ci.append(self._cv.create_line(
                cx, cy - s, cx, cy + s, fill=c, width=w))
            self._ci.append(self._cv.create_line(
                cx - s, cy, cx + s, cy, fill=c, width=w))
        elif style == "corner":
            l, g2 = s, g
            self._ci.append(self._cv.create_line(
                cx - g2, cy - g2 - l, cx - g2, cy - g2, fill=c, width=w))
            self._ci.append(self._cv.create_line(
                cx - g2 - l, cy - g2, cx - g2, cy - g2, fill=c, width=w))
            self._ci.append(self._cv.create_line(
                cx + g2, cy - g2 - l, cx + g2, cy - g2, fill=c, width=w))
            self._ci.append(self._cv.create_line(
                cx + g2 + l, cy - g2, cx + g2, cy - g2, fill=c, width=w))
            self._ci.append(self._cv.create_line(
                cx - g2, cy + g2 + l, cx - g2, cy + g2, fill=c, width=w))
            self._ci.append(self._cv.create_line(
                cx - g2 - l, cy + g2, cx - g2, cy + g2, fill=c, width=w))
            self._ci.append(self._cv.create_line(
                cx + g2, cy + g2 + l, cx + g2, cy + g2, fill=c, width=w))
            self._ci.append(self._cv.create_line(
                cx + g2 + l, cy + g2, cx + g2, cy + g2, fill=c, width=w))
        if not self.cfg["visible"]:
            for i in self._ci:
                self._cv.itemconfigure(i, state="hidden")

    def _topmost_loop(self):
        if not hasattr(self, "_ov") or self._ov is None:
            return
        try:
            if self.cfg.get("force_topmost", False):
                self._ov.lift()
                self._ov.attributes("-topmost", True)
        except:
            pass
        self.root.after(200, self._topmost_loop)

    def toggle(self):
        self.cfg["visible"] = not self.cfg["visible"]
        try:
            if hasattr(self, "_ci"):
                for i in self._ci:
                    self._cv.itemconfigure(i,
                        state="normal" if self.cfg["visible"] else "hidden")
            if hasattr(self, "_ov") and self._ov is not None:
                self._ov.wm_attributes(
                    "-alpha", self.cfg["alpha"] if self.cfg["visible"] else 0)
        except:
            pass
        save(self.cfg)

    def _hide_settings(self):
        self.root.iconify()

    def quit(self):
        try:
            save(self.cfg)
        except:
            pass
        try:
            if hasattr(self, "_ov") and self._ov is not None:
                self._ov.destroy()
        except:
            pass
        self.root.destroy()
        os._exit(0)

    def run(self):
        self.root.mainloop()


def main():
    k = ctypes.windll.kernel32
    k.CreateMutexW(None, True, "Global\\Crosshair_Mutex_v2")
    if k.GetLastError() == 183:
        ctypes.windll.user32.MessageBoxW(0, "Crosshair 已在运行中!", "Crosshair", 0x40)
        sys.exit(0)
    App().run()


if __name__ == "__main__":
    main()

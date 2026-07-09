"""
外置准星 v3
Apple-style dark theme, custom radio & checkbox widgets.
支持自定义图片准星、预设保存、多层准星。
"""
import tkinter as tk
from tkinter import colorchooser, filedialog, simpledialog, messagebox
import json
import os
import sys
import shutil
import webbrowser
import ctypes
import math

try:
    from PIL import Image, ImageTk
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

# ---- 路径配置 ----
if getattr(sys, 'frozen', False):
    DIR = os.path.dirname(sys.executable)
else:
    DIR = os.path.dirname(os.path.abspath(__file__))

ICON = os.path.join(DIR, "crosshair.ico")
CONFIG_DIR = os.path.join(os.environ.get("APPDATA", os.path.expanduser("~")), "Crosshair")
os.makedirs(CONFIG_DIR, exist_ok=True)
CFG = os.path.join(CONFIG_DIR, "crosshair_config.json")
IMG_DIR = os.path.join(CONFIG_DIR, "images")
os.makedirs(IMG_DIR, exist_ok=True)

LAYER_DEFAULT = {
    "color": "#00FF00", "alpha": 0.85, "size": 20,
    "thickness": 2, "gap": 4, "style": "cross", "visible": True,
    "offset_x": 0, "offset_y": 0, "image_path": "", "angle": 0,
}

DEFAULT = {
    "multi_layer": False,
    "active_layer": 0,
    "layers": [
        dict(LAYER_DEFAULT),
        dict(LAYER_DEFAULT, color="#FF4444", size=15, style="dot"),
        dict(LAYER_DEFAULT, color="#00D4FF", size=10, style="circle", visible=False),
    ],
    "force_topmost": False,
    "presets": {},
}

COLORS = [
    "#00FF00", "#FF4444", "#00D4FF", "#FF44FF",
    "#FFDD44", "#FFFFFF", "#FF8800",
]
STYLES = ["cross", "circle", "dot", "crosshair", "corner", "triangle", "image"]
STYLE_NAMES = ["十字", "圆圈", "圆点", "实心十字", "四角", "三角", "图片"]

# ---- Apple dark palette ----
BG      = "#1c1c1e"
CARD    = "#2c2c2e"
TEXT    = "#f5f5f7"
SUBTLE  = "#98989d"
ACCENT  = "#0a84ff"
GREEN   = "#30d158"
SEP     = "#38383a"
PREVIEW = "#000000"
RING    = "#636366"


def load():
    try:
        with open(CFG, "r", encoding="utf-8") as f:
            c = json.load(f)
        # 向后兼容：迁移旧版扁平配置
        if "layers" not in c:
            old_layer = {}
            for k in LAYER_DEFAULT:
                old_layer[k] = c.pop(k, LAYER_DEFAULT[k])
            c["layers"] = [old_layer]
            for i in range(1, 3):
                c["layers"].append(dict(LAYER_DEFAULT))
        # 补齐缺失字段
        c.setdefault("multi_layer", False)
        c.setdefault("active_layer", 0)
        c.setdefault("force_topmost", False)
        c.setdefault("presets", {})
        while len(c["layers"]) < 3:
            c["layers"].append(dict(LAYER_DEFAULT))
        for layer in c["layers"]:
            for k, v in LAYER_DEFAULT.items():
                layer.setdefault(k, v)
        return c
    except:
        return {
            "multi_layer": False, "active_layer": 0,
            "layers": [dict(LAYER_DEFAULT) for _ in range(3)],
            "force_topmost": False, "presets": {},
        }


def save(c):
    with open(CFG, "w", encoding="utf-8") as f:
        json.dump(c, f, indent=2, ensure_ascii=False)


# ---------------------------------------------------------------
#  Apple-style custom widgets
# ---------------------------------------------------------------
RADIO_SIZE = 12
CHECK_SIZE = 14


class RadioGroup:
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
                c.create_oval(x - r, y - r, x + r, y + r, fill=ACCENT, outline=ACCENT)
                c.create_oval(x - 3, y - 3, x + 3, y + 3, fill="#fff", outline="")
            else:
                c.create_oval(x - r, y - r, x + r, y + r, outline=RING, width=1.5)


class CheckSwitch:
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
            self.cv.create_rounded_rect(x, y, x + s, y + s, r, fill=ACCENT, outline=ACCENT)
            self.cv.create_line(x + 3, y + s // 2, x + s // 2 + 1, y + s - 4,
                                fill="#fff", width=2, capstyle="round")
            self.cv.create_line(x + s // 2 + 1, y + s - 4, x + s - 2, y + 2,
                                fill="#fff", width=2, capstyle="round")
        else:
            self.cv.create_rounded_rect(x, y, x + s, y + s, r, outline=RING, width=1.5)

    def pack(self, **kw):
        self.widget.pack(**kw)


def _create_rounded_rect(canvas, x1, y1, x2, y2, r, **kw):
    pts = [x1 + r, y1, x2 - r, y1,
           x2, y1, x2, y1 + r,
           x2, y2 - r, x2, y2,
           x2 - r, y2, x1 + r, y2,
           x1, y2, x1, y2 - r,
           x1, y1 + r, x1, y1]
    return canvas.create_polygon(pts, smooth=True, **kw)

tk.Canvas.create_rounded_rect = _create_rounded_rect


# ---------------------------------------------------------------
#  绘制工具函数
# ---------------------------------------------------------------
def _rot(x, y, cx, cy, angle):
    """绕 (cx, cy) 旋转 (x, y) 指定角度（度）。"""
    if angle == 0:
        return x, y
    rad = math.radians(angle)
    cos_a, sin_a = math.cos(rad), math.sin(rad)
    dx, dy = x - cx, y - cy
    return cx + dx * cos_a - dy * sin_a, cy + dx * sin_a + dy * cos_a


def draw_style(canvas, style, s, t, g, color, cx, cy, result_list, img_obj=None, angle=0):
    """在一个 canvas 上绘制指定风格的准星，支持旋转。"""
    if style == "cross":
        x1, y1 = _rot(cx, cy - g - s, cx, cy, angle)
        x2, y2 = _rot(cx, cy - g, cx, cy, angle)
        result_list.append(canvas.create_line(x1, y1, x2, y2, fill=color, width=t))
        x1, y1 = _rot(cx, cy + g, cx, cy, angle)
        x2, y2 = _rot(cx, cy + g + s, cx, cy, angle)
        result_list.append(canvas.create_line(x1, y1, x2, y2, fill=color, width=t))
        x1, y1 = _rot(cx - g - s, cy, cx, cy, angle)
        x2, y2 = _rot(cx - g, cy, cx, cy, angle)
        result_list.append(canvas.create_line(x1, y1, x2, y2, fill=color, width=t))
        x1, y1 = _rot(cx + g, cy, cx, cy, angle)
        x2, y2 = _rot(cx + g + s, cy, cx, cy, angle)
        result_list.append(canvas.create_line(x1, y1, x2, y2, fill=color, width=t))
        r = max(1, t // 2)
        result_list.append(canvas.create_oval(cx - r, cy - r, cx + r, cy + r, fill=color, outline=color))
    elif style == "circle":
        r = s
        result_list.append(canvas.create_oval(cx - r, cy - r, cx + r, cy + r, outline=color, width=t))
        result_list.append(canvas.create_oval(cx - 1, cy - 1, cx + 1, cy + 1, fill=color, outline=color))
    elif style == "dot":
        r = s
        result_list.append(canvas.create_oval(cx - r, cy - r, cx + r, cy + r, fill=color, outline=color))
    elif style == "crosshair":
        x1, y1 = _rot(cx, cy - s, cx, cy, angle)
        x2, y2 = _rot(cx, cy + s, cx, cy, angle)
        result_list.append(canvas.create_line(x1, y1, x2, y2, fill=color, width=t))
        x1, y1 = _rot(cx - s, cy, cx, cy, angle)
        x2, y2 = _rot(cx + s, cy, cx, cy, angle)
        result_list.append(canvas.create_line(x1, y1, x2, y2, fill=color, width=t))
    elif style == "corner":
        ll, g2 = s, g
        for dx, dy in [(-1, -1), (1, -1), (-1, 1), (1, 1)]:
            bx, by = cx + dx * g2, cy + dy * g2
            # 垂直线：从角点向外延伸
            y1 = by - ll if dy == -1 else by
            y2 = by if dy == -1 else by + ll
            rx1, ry1 = _rot(bx, y1, cx, cy, angle)
            rx2, ry2 = _rot(bx, y2, cx, cy, angle)
            result_list.append(canvas.create_line(rx1, ry1, rx2, ry2, fill=color, width=t))
            # 水平线：从角点向外延伸
            x1 = bx - ll if dx == -1 else bx
            x2 = bx if dx == -1 else bx + ll
            rx1, ry1 = _rot(x1, by, cx, cy, angle)
            rx2, ry2 = _rot(x2, by, cx, cy, angle)
            result_list.append(canvas.create_line(rx1, ry1, rx2, ry2, fill=color, width=t))
    elif style == "triangle":
        hh = int(s * 1.6)
        hw = int(s * 1.3)
        pts = [
            _rot(cx, cy - hh, cx, cy, angle),
            _rot(cx - hw, cy + hh // 2, cx, cy, angle),
            _rot(cx + hw, cy + hh // 2, cx, cy, angle),
        ]
        flat = [v for p in pts for v in p]
        result_list.append(canvas.create_polygon(*flat, outline=color, fill="", width=t))
    elif style == "image":
        if img_obj is not None:
            result_list.append(canvas.create_image(cx, cy, image=img_obj, anchor="center"))


# ---------------------------------------------------------------
#  App
# ---------------------------------------------------------------
class App:
    def __init__(self):
        self._root_cfg = load()
        self._sync_cfg()
        self._photo_imgs = [None, None, None]
        self._ov_photo_imgs = [None, None, None]
        self._ovs = [None, None, None]    # 每图层独立覆盖窗口
        self._cvs = [None, None, None]    # 每图层独立 canvas
        self._cis = [[], [], []]          # 每图层独立绘制 ID
        self.root = tk.Tk()
        self.root.title("Crosshair")
        self.root.resizable(False, True)
        self.root.configure(bg=BG)

        if os.path.exists(ICON):
            try:
                self.root.iconbitmap(ICON)
            except:
                pass

        pw, ph = 340, 850
        sw = self.root.winfo_screenwidth()
        sh = self.root.winfo_screenheight()
        py = max(0, sh - ph - 40)
        self.root.geometry(f"{pw}x{ph}+{sw - pw - 30}+{py}")

        # 滚动画布
        self._canvas = tk.Canvas(self.root, bg=BG, highlightthickness=0, bd=0)
        self._sbar = tk.Scrollbar(self.root, orient="vertical", command=self._canvas.yview, bg=BG)
        self._sbar.pack(side="right", fill="y")
        self._canvas.pack(side="left", fill="both", expand=True)
        self._canvas.configure(yscrollcommand=self._sbar.set)
        self._canvas.bind("<Configure>", lambda e: self._canvas.itemconfigure("main", width=e.width))
        self.root.bind_all("<MouseWheel>", self._on_mousewheel)

        self._build_ui()
        self._apply_to_overlay()
        self.root.protocol("WM_DELETE_WINDOW", self.quit)
        self.root.after(100, self._create_overlays)

    # ------------------------------------------------------------------
    #  cfg 指针管理
    # ------------------------------------------------------------------
    def _sync_cfg(self):
        """将 self.cfg 指向当前活动图层。"""
        idx = 0
        if self._root_cfg.get("multi_layer", False):
            idx = self._root_cfg.get("active_layer", 0)
        self.cfg = self._root_cfg["layers"][idx]
        self._active_idx = idx

    def _active_layer_idx(self):
        return self._active_idx

    def _root_save(self):
        save(self._root_cfg)

    # ------------------------------------------------------------------
    def _on_mousewheel(self, event):
        self._canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

    # ------------------------------------------------------------------
    def _build_ui(self):
        main = tk.Frame(self._canvas, bg=BG, padx=16, pady=12)
        self._canvas.create_window((0, 0), window=main, anchor="nw", tags="main")
        main.bind("<Configure>", lambda e: self._canvas.configure(scrollregion=self._canvas.bbox("all")))

        # title
        hf = tk.Frame(main, bg=BG)
        hf.pack(fill="x", pady=(0, 10))
        tk.Label(hf, text="Crosshair", font=("Microsoft YaHei UI", 17, "bold"),
                 fg=TEXT, bg=BG).pack(side="left")
        tk.Label(hf, text="v3", font=("Microsoft YaHei UI", 10),
                 fg=SUBTLE, bg=BG).pack(side="left", padx=(6, 0))
        bl = tk.Label(hf, text="B站", font=("Microsoft YaHei UI", 9, "underline"),
                      fg=ACCENT, bg=BG, cursor="hand2")
        bl.pack(side="right")
        bl.bind("<Button-1>", lambda e: webbrowser.open("https://space.bilibili.com/187682531"))

        # ---- 多层准星开关 ----
        self._label(main, "多层准星")
        self._ml_frame = tk.Frame(main, bg=CARD, padx=10, pady=8)
        self._ml_frame.pack(fill="x", pady=(0, 4))
        self._ml_var = tk.BooleanVar(value=self._root_cfg.get("multi_layer", False))
        CheckSwitch(self._ml_frame, "启用多层准星", self._ml_var, CARD).pack(anchor="w")
        self._ml_var.trace_add("write", lambda *a: self._on_multi_layer_toggle())

        # 图层选择按钮
        self._layer_btns_frame = tk.Frame(main, bg=BG)
        self._layer_labels = ["图层 1", "图层 2", "图层 3"]
        self._layer_btns = []    # 图层标签
        self._layer_eyes = []    # 眼睛图标 Canvas
        lf = tk.Frame(self._layer_btns_frame, bg=CARD, padx=6, pady=6)
        lf.pack(fill="x", pady=(0, 12))
        for i in range(3):
            row = tk.Frame(lf, bg=CARD)
            row.pack(side="left", padx=4)
            # 眼睛图标
            eye = tk.Canvas(row, width=18, height=14, bg=CARD, highlightthickness=0, cursor="hand2")
            eye.pack(side="left")
            eye.bind("<Button-1>", lambda e, idx=i: self._toggle_layer_vis(idx))
            self._layer_eyes.append(eye)
            # 图层名
            lbl = tk.Label(row, text=self._layer_labels[i],
                           fg=TEXT, bg=CARD, font=("Microsoft YaHei UI", 9), cursor="hand2")
            lbl.pack(side="left", padx=(2, 0))
            lbl.bind("<Button-1>", lambda e, idx=i: self._switch_layer(idx))
            self._layer_btns.append(lbl)
        self._update_layer_btns()

        # preview
        self._label(main, "预览")
        self._preview_frame = tk.Frame(main, bg=CARD, padx=1, pady=1, highlightthickness=0)
        self._preview_frame.pack(fill="x", pady=(0, 12))
        self.pc = tk.Canvas(self._preview_frame, width=300, height=64, bg=PREVIEW,
                            bd=0, highlightthickness=0)
        self.pc.pack()
        self._pi = []

        # style
        self._label(main, "形状")
        self._style_frame = tk.Frame(main, bg=CARD, padx=10, pady=10)
        self._style_frame.pack(fill="x", pady=(0, 12))
        self.sv = tk.StringVar(value=self.cfg["style"])
        self._style_group = RadioGroup(self._style_frame, list(zip(STYLES, STYLE_NAMES)),
                   self.sv, self._on_style_change, CARD)

        # 图片选择区域
        self._img_frame = tk.Frame(main, bg=BG)
        self._label_inner(self._img_frame, "自定义图片")
        self._img_card = tk.Frame(self._img_frame, bg=CARD, padx=10, pady=8)
        self._img_card.pack(fill="x")
        self._img_label = tk.Label(self._img_card, text="未选择图片",
                                   fg=SUBTLE, bg=CARD, font=("Microsoft YaHei UI", 9), anchor="w")
        self._img_label.pack(side="left", fill="x", expand=True)
        tk.Button(self._img_card, text="选择PNG", command=self._pick_image,
                  bg=ACCENT, fg="#fff", activebackground=ACCENT,
                  relief="flat", bd=0, padx=10, pady=2,
                  font=("Microsoft YaHei UI", 9), cursor="hand2").pack(side="right", padx=(6, 0))
        tk.Button(self._img_card, text="清除", command=self._clear_image,
                  bg=CARD, fg=TEXT, activebackground=SEP,
                  relief="flat", bd=0, padx=8, pady=2,
                  font=("Microsoft YaHei UI", 9), cursor="hand2").pack(side="right")
        self._update_image_ui()

        # color
        self._label(main, "颜色")
        cf = tk.Frame(main, bg=CARD, padx=10, pady=10)
        cf.pack(fill="x", pady=(0, 12))
        for i, c in enumerate(COLORS):
            s = 26
            f = tk.Frame(cf, bg=CARD)
            f.grid(row=0, column=i, padx=3)
            inner = tk.Canvas(f, width=s, height=s, bg=c, bd=0, highlightthickness=0, cursor="hand2")
            inner.pack()
            if c == self.cfg["color"]:
                inner.create_oval(2, 2, s - 2, s - 2, outline="#fff", width=2)
            inner.bind("<Button-1>", lambda e, x=c: self._set_color(x))
        sf2 = tk.Frame(cf, bg=CARD)
        sf2.grid(row=0, column=len(COLORS), padx=3)
        self._cc = tk.Canvas(sf2, width=26, height=26, bg=self.cfg["color"],
                             bd=0, highlightthickness=0, cursor="hand2")
        self._cc.pack()
        self._cc.create_line(8, 13, 18, 13, fill="#fff", width=2)
        self._cc.create_line(13, 8, 13, 18, fill="#fff", width=2)
        self._cc.bind("<Button-1>", self._pick_color)

        # sliders
        self._label(main, "参数")
        sf3 = tk.Frame(main, bg=CARD, padx=10, pady=8)
        sf3.pack(fill="x", pady=(0, 12))
        self._labs = {}
        self._mk_slider(sf3, "尺寸", "size", 1, 80, 0)
        self._mk_slider(sf3, "粗细", "thickness", 1, 12, 1)
        self._mk_slider(sf3, "间隙", "gap", 0, 30, 2)
        self._mk_slider(sf3, "旋转", "angle", 0, 360, 3)
        self._mk_slider(sf3, "透明度", "alpha", 20, 100, 4, True)

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
        self.ft = tk.BooleanVar(value=self._root_cfg.get("force_topmost", False))
        CheckSwitch(tf, "强制置顶", self.ft, CARD).pack(anchor="w")

        # 预设
        self._label(main, "预设")
        preset_frame = tk.Frame(main, bg=CARD, padx=10, pady=8)
        preset_frame.pack(fill="x", pady=(0, 12))
        self._preset_list = tk.Listbox(preset_frame, height=4,
                                       fg=TEXT, bg=PREVIEW,
                                       selectbackground=ACCENT, selectforeground="#fff",
                                       bd=0, highlightthickness=0,
                                       font=("Microsoft YaHei UI", 9), exportselection=False)
        self._preset_list.pack(fill="x", pady=(0, 6))
        self._update_preset_list()
        pbtn = tk.Frame(preset_frame, bg=CARD)
        pbtn.pack(fill="x")
        tk.Button(pbtn, text="保存预设", command=self._save_preset,
                  bg=GREEN, fg="#fff", activebackground=GREEN,
                  relief="flat", bd=0, padx=8, pady=3,
                  font=("Microsoft YaHei UI", 9), cursor="hand2").pack(side="left", padx=(0, 6))
        tk.Button(pbtn, text="加载", command=self._load_preset,
                  bg=ACCENT, fg="#fff", activebackground=ACCENT,
                  relief="flat", bd=0, padx=12, pady=3,
                  font=("Microsoft YaHei UI", 9), cursor="hand2").pack(side="left", padx=(0, 6))
        tk.Button(pbtn, text="删除", command=self._delete_preset,
                  bg="#d93", fg="#fff", activebackground="#d93",
                  relief="flat", bd=0, padx=12, pady=3,
                  font=("Microsoft YaHei UI", 9), cursor="hand2").pack(side="left")
        tk.Button(pbtn, text="重置", command=self._reset_current_layer,
                  bg=SEP, fg=TEXT, activebackground=RING,
                  relief="flat", bd=0, padx=12, pady=3,
                  font=("Microsoft YaHei UI", 9), cursor="hand2").pack(side="left", padx=(6, 0))

        # buttons
        btnf = tk.Frame(main, bg=BG)
        btnf.pack(fill="x", pady=(4, 0))
        self._btn(btnf, "显示/隐藏", self.toggle, CARD, TEXT)
        self._btn(btnf, "应用", self._apply, ACCENT, "#fff")
        self._btn(btnf, "退出设置", self._hide_settings, CARD, TEXT)

        info = tk.Frame(main, bg=BG)
        info.pack(fill="x", pady=(6, 0))
        tk.Label(info, text=f"数据: {CONFIG_DIR}",
                 font=("Microsoft YaHei UI", 7), fg=SUBTLE, bg=BG).pack(side="left")
        tk.Label(info, text="配置/预设保存于此",
                 font=("Microsoft YaHei UI", 7), fg=SEP, bg=BG).pack(side="left", padx=(4, 0))

        self._on_multi_layer_toggle()
        self._refresh()

    # ------------------------------------------------------------------
    #  多层准星
    # ------------------------------------------------------------------
    def _on_multi_layer_toggle(self):
        on = self._ml_var.get()
        self._root_cfg["multi_layer"] = on
        if on:
            self._layer_btns_frame.pack(fill="x", before=self._preview_frame)
        else:
            self._layer_btns_frame.pack_forget()
        self._sync_cfg()
        self._update_layer_btns()
        self._refresh_ui_from_cfg()
        self._refresh()
        self._apply_to_overlay()
        self._root_save()

    def _switch_layer(self, idx):
        self._root_cfg["active_layer"] = idx
        self._sync_cfg()
        self._update_layer_btns()
        self._refresh_ui_from_cfg()
        self._refresh()
        self._apply_to_overlay()

    def _toggle_layer_vis(self, idx):
        """左键切换指定图层的可见性。"""
        layer = self._root_cfg["layers"][idx]
        layer["visible"] = not layer.get("visible", True)
        self._update_layer_btns()
        self._refresh()
        self._apply_to_overlay()
        self._root_save()

    def _update_layer_btns(self):
        active = self._active_layer_idx()
        ml = self._root_cfg.get("multi_layer", False)
        for i in range(3):
            visible = self._root_cfg["layers"][i].get("visible", True)
            # 更新图层标签
            lbl = self._layer_btns[i]
            if ml and i == active:
                lbl.configure(fg=ACCENT)
            elif ml:
                lbl.configure(fg=TEXT if visible else SUBTLE)
            else:
                lbl.configure(fg=SUBTLE)
            # 更新眼睛图标
            eye = self._layer_eyes[i]
            eye.delete("all")
            if ml:
                ex, ey = 9, 7
                r = 5
                if visible:
                    # 睁眼：实心圆 + 光点
                    eye.create_oval(ex - r, ey - r, ex + r, ey + r,
                                    fill=GREEN, outline=GREEN)
                    eye.create_oval(ex - 2, ey - 2, ex + 2, ey + 2,
                                    fill="#fff", outline="")
                else:
                    # 闭眼：空心圆 + 斜线
                    eye.create_oval(ex - r, ey - r, ex + r, ey + r,
                                    outline=RING, width=1.5)
                    eye.create_line(ex - 3, ey + 3, ex + 3, ey - 3,
                                    fill=RING, width=1.5)
            else:
                eye.create_oval(7, 5, 11, 9, outline=RING, width=1)

    # ------------------------------------------------------------------
    def _label(self, parent, text):
        tk.Label(parent, text=text, font=("Microsoft YaHei UI", 9, "bold"),
                 fg=SUBTLE, bg=BG).pack(anchor="w", pady=(0, 4))

    def _label_inner(self, parent, text):
        tk.Label(parent, text=text, font=("Microsoft YaHei UI", 9, "bold"),
                 fg=SUBTLE, bg=BG).pack(anchor="w", pady=(10, 4))

    def _btn(self, parent, text, cmd, bg_c, fg_c):
        tk.Button(parent, text=text, command=cmd,
                  bg=bg_c, fg=fg_c, activebackground=ACCENT,
                  activeforeground="#fff", relief="flat", bd=0,
                  padx=12, pady=4, font=("Microsoft YaHei UI", 9),
                  cursor="hand2").pack(side="left", padx=(0, 8))

    # ------------------------------------------------------------------
    def _refresh_ui_from_cfg(self):
        """将 self.cfg 的内容同步到所有 UI 控件。"""
        self.sv.set(self.cfg["style"])
        self._style_group._draw()
        self._toggle_image_ui()
        self._update_image_ui()
        for key in ["size", "thickness", "gap", "angle", "offset_x", "offset_y"]:
            if hasattr(self, f"_v_{key}"):
                getattr(self, f"_v_{key}").set(self.cfg[key])
                self._upd_lab(key, False)
        if hasattr(self, "_v_alpha"):
            self._v_alpha.set(int(self.cfg["alpha"] * 100))
            self._upd_lab("alpha", True)

    # ------------------------------------------------------------------
    #  图片选择
    # ------------------------------------------------------------------
    def _pick_image(self):
        if not HAS_PIL:
            messagebox.showwarning("缺少依赖",
                                   "需要安装 Pillow 库才能使用图片准星功能。\n请在命令行执行: pip install Pillow")
            return
        path = filedialog.askopenfilename(
            title="选择PNG图片",
            filetypes=[("PNG 图片", "*.png"), ("所有图片", "*.png;*.jpg;*.jpeg;*.bmp;*.gif")])
        if not path:
            return
        try:
            fname = os.path.basename(path)
            dest = os.path.join(IMG_DIR, fname)
            base, ext = os.path.splitext(fname)
            n = 1
            while os.path.exists(dest):
                dest = os.path.join(IMG_DIR, f"{base}_{n}{ext}")
                n += 1
            shutil.copy2(path, dest)
            self.cfg["image_path"] = dest
        except Exception as e:
            messagebox.showerror("错误", f"复制图片失败: {e}")
            return
        self._photo_imgs[self._active_idx] = None
        self._update_image_ui()
        self._refresh()
        self._apply_to_overlay()
        self._root_save()

    def _clear_image(self):
        self.cfg["image_path"] = ""
        self._photo_imgs[self._active_idx] = None
        self._ov_photo_imgs[self._active_idx] = None
        self._update_image_ui()
        self._refresh()
        self._apply_to_overlay()
        self._root_save()

    def _update_image_ui(self):
        path = self.cfg.get("image_path", "")
        if path and os.path.exists(path):
            self._img_label.configure(text=os.path.basename(path), fg=TEXT)
        else:
            self._img_label.configure(text="未选择图片", fg=SUBTLE)

    def _toggle_image_ui(self):
        if self.sv.get() == "image":
            self._img_frame.pack(fill="x", after=self._style_frame, pady=(0, 12))
        else:
            self._img_frame.pack_forget()

    # ------------------------------------------------------------------
    def _mk_slider(self, p, label, key, mn, mx, row, alpha=False):
        f = tk.Frame(p, bg=CARD)
        f.grid(row=row, column=0, sticky="ew", pady=3)
        tk.Label(f, text=label, width=5, anchor="e",
                 fg=TEXT, bg=CARD, font=("Microsoft YaHei UI", 9)).pack(side="left", padx=(0, 6))
        v = self.cfg[key]
        if alpha:
            v = int(v * 100)
        var = tk.IntVar(value=v)
        setattr(self, f"_v_{key}", var)
        tk.Scale(f, from_=mn, to=mx, orient="horizontal", variable=var,
                 length=155, showvalue=False, bg=CARD, fg=CARD,
                 troughcolor=SEP, activebackground=ACCENT,
                 highlightthickness=0, bd=0,
                 command=lambda _, k=key, a=alpha: self._on_slide(k, a)).pack(side="left")
        ent = tk.Entry(f, width=5, fg=TEXT, bg=PREVIEW,
                       insertbackground=TEXT, bd=0,
                       font=("Microsoft YaHei UI", 9), justify="center")
        ent.pack(side="left", padx=(4, 0))
        ent.bind("<Return>", lambda e, k=key, a=alpha, m=mn, x=mx: self._on_entry(k, a, m, x))
        ent.bind("<FocusOut>", lambda e, k=key, a=alpha, m=mn, x=mx: self._on_entry(k, a, m, x))
        self._labs[key] = (ent, alpha, mn, mx)
        self._upd_lab(key, alpha)

    def _upd_lab(self, key, alpha):
        if key not in self._labs:
            return
        ent, af, _, _ = self._labs[key]
        v = getattr(self, f"_v_{key}").get()
        t = f"{v}%" if af else str(v)
        if ent.get() != t:
            ent.delete(0, "end")
            ent.insert(0, t)

    def _on_entry(self, key, alpha, mn, mx):
        ent, _, _, _ = self._labs[key]
        try:
            raw = ent.get().replace("%", "")
            v = int(raw)
            if key in ("offset_x", "offset_y"):
                v = max(-9999, min(9999, v))
            else:
                v = max(mn, min(mx, v))
            getattr(self, f"_v_{key}").set(v)
            self.cfg[key] = v / 100.0 if alpha else v
            self._upd_lab(key, alpha)
            self._refresh()
            self._apply_to_overlay()
        except ValueError:
            self._upd_lab(key, alpha)

    def _on_slide(self, key, alpha):
        if key not in self._labs:
            return
        self.cfg[key] = getattr(self, f"_v_{key}").get() / 100.0 if alpha else getattr(self, f"_v_{key}").get()
        self._upd_lab(key, alpha)
        self._refresh()
        self._apply_to_overlay()

    def _set_color(self, c):
        self.cfg["color"] = c
        self._refresh()
        self._apply_to_overlay()

    def _pick_color(self, *a):
        c = colorchooser.askcolor(color=self.cfg["color"], title="选择颜色")
        if c[1]:
            self._set_color(c[1])
            self._cc.configure(bg=c[1])

    def _on_style_change(self):
        self.cfg["style"] = self.sv.get()
        self._toggle_image_ui()
        self._photo_imgs[self._active_idx] = None
        self._ov_photo_imgs[self._active_idx] = None
        self._refresh()
        self._apply_to_overlay()

    # ------------------------------------------------------------------
    #  图片加载
    # ------------------------------------------------------------------
    def _load_photo_image(self, layer, scale=None, angle=0):
        path = layer.get("image_path", "")
        if not path or not os.path.exists(path) or not HAS_PIL:
            return None
        try:
            img = Image.open(path).convert("RGBA")
            if angle != 0:
                img = img.rotate(angle, expand=True, resample=Image.BICUBIC)
            if scale is not None:
                new_w = max(1, int(img.width * scale))
                new_h = max(1, int(img.height * scale))
                img = img.resize((new_w, new_h), Image.LANCZOS)
            return ImageTk.PhotoImage(img)
        except Exception:
            return None

    # ------------------------------------------------------------------
    def _refresh(self):
        self.cfg["style"] = self.sv.get()
        for i in self._pi:
            self.pc.delete(i)
        self._pi.clear()

        # 预览：如果是多层模式，显示所有图层（堆叠）；否则只显示当前图层
        if self._root_cfg.get("multi_layer", False):
            layers = self._root_cfg["layers"]
        else:
            layers = [self.cfg]

        cx, cy = 150, 32
        for idx, layer in enumerate(layers):
            if not layer.get("visible", True) and self._root_cfg.get("multi_layer", False):
                continue
            s = int(layer["size"])
            t = int(layer["thickness"])
            g = int(layer["gap"])
            style = layer["style"]
            color = layer["color"]

            if style == "image":
                if HAS_PIL:
                    path = layer.get("image_path", "")
                    if path and os.path.exists(path):
                        scale = min(50 / max(1, s * 2), 1.0)
                        self._photo_imgs[idx] = self._load_photo_image(
                            layer, scale=scale, angle=int(layer.get("angle", 0)))
                        if self._photo_imgs[idx]:
                            self._pi.append(self.pc.create_image(cx, cy, image=self._photo_imgs[idx], anchor="center"))
                        continue
                r = s
                self._pi.append(self.pc.create_rectangle(cx - r, cy - r, cx + r, cy + r,
                                                          outline=SUBTLE, dash=(3, 3)))
                self._pi.append(self.pc.create_text(cx, cy, text="PNG", fill=SUBTLE,
                                                     font=("Microsoft YaHei UI", 8)))
            else:
                draw_style(self.pc, style, s, t, g, color, cx, cy, self._pi,
                           angle=int(layer.get("angle", 0)))

    # ------------------------------------------------------------------
    #  预设
    # ------------------------------------------------------------------
    def _update_preset_list(self):
        self._preset_list.delete(0, "end")
        for name in sorted(self._root_cfg.get("presets", {}).keys()):
            self._preset_list.insert("end", name)

    def _save_preset(self):
        name = simpledialog.askstring("保存预设", "请输入预设名称:", parent=self.root)
        if not name or not name.strip():
            return
        name = name.strip()
        if "presets" not in self._root_cfg:
            self._root_cfg["presets"] = {}
        # 保存全部图层 + 全局设置
        self._root_cfg["presets"][name] = {
            "multi_layer": self._root_cfg.get("multi_layer", False),
            "layers": [dict(l) for l in self._root_cfg["layers"]],
            "force_topmost": self._root_cfg.get("force_topmost", False),
        }
        self._root_save()
        self._update_preset_list()
        names = sorted(self._root_cfg["presets"].keys())
        idx = names.index(name)
        self._preset_list.selection_clear(0, "end")
        self._preset_list.selection_set(idx)
        self._preset_list.see(idx)

    def _load_preset(self):
        sel = self._preset_list.curselection()
        if not sel:
            messagebox.showinfo("提示", "请先选择一个预设")
            return
        name = self._preset_list.get(sel[0])
        preset = self._root_cfg.get("presets", {}).get(name)
        if not preset:
            return
        self._root_cfg["multi_layer"] = preset.get("multi_layer", False)
        self._root_cfg["layers"] = [dict(l) for l in preset.get("layers", [dict(LAYER_DEFAULT)])]
        while len(self._root_cfg["layers"]) < 3:
            self._root_cfg["layers"].append(dict(LAYER_DEFAULT))
        self._root_cfg["force_topmost"] = preset.get("force_topmost", False)
        self._root_cfg["active_layer"] = 0
        self._sync_cfg()
        self.ft.set(self._root_cfg["force_topmost"])
        self._ml_var.set(self._root_cfg["multi_layer"])
        # _ml_var 的 trace 已触发 _on_multi_layer_toggle，完成全部 UI 同步
        self._root_save()

    def _delete_preset(self):
        sel = self._preset_list.curselection()
        if not sel:
            messagebox.showinfo("提示", "请先选择一个预设")
            return
        name = self._preset_list.get(sel[0])
        if not messagebox.askyesno("删除预设", f"确定要删除预设「{name}」吗?"):
            return
        if "presets" in self._root_cfg and name in self._root_cfg["presets"]:
            del self._root_cfg["presets"][name]
            self._root_save()
            self._update_preset_list()

    def _reset_current_layer(self):
        """只重置当前活动图层的设置。"""
        if not messagebox.askyesno("重置图层", "确定要重置当前图层的设置吗？"):
            return
        for k, v in LAYER_DEFAULT.items():
            self.cfg[k] = v
        self._refresh_ui_from_cfg()
        self._refresh()
        self._apply_to_overlay()
        self._root_save()

    # ------------------------------------------------------------------
    def _apply(self):
        self.cfg["style"] = self.sv.get()
        self._root_cfg["force_topmost"] = self.ft.get()
        self._apply_to_overlay()
        self._root_save()

    def _apply_to_overlay(self):
        """更新所有覆盖层窗口的透明度。"""
        ml = self._root_cfg.get("multi_layer", False)
        for idx in range(3):
            ov = self._ovs[idx]
            if ov is None:
                continue
            try:
                ov.winfo_exists()
            except:
                continue
            layer = self._root_cfg["layers"][idx]
            if ml:
                alpha = layer["alpha"] if layer.get("visible", True) else 0.0
            else:
                alpha = layer["alpha"] if (idx == 0 and layer.get("visible", True)) else 0.0
            ov.wm_attributes("-alpha", alpha)
        self._draw_overlay()
        if self._root_cfg.get("force_topmost", False):
            for ov in self._ovs:
                if ov is not None:
                    try:
                        ov.attributes("-topmost", True)
                    except:
                        pass

    def _create_overlays(self):
        """为每个图层创建独立的透明覆盖窗口。"""
        self.sw = self.root.winfo_screenwidth()
        self.sh = self.root.winfo_screenheight()
        for idx in range(3):
            ov = tk.Toplevel(self.root)
            ov.title(f"crosshair_overlay_{idx}")
            ov.overrideredirect(True)
            ov.geometry(f"{self.sw}x{self.sh}+0+0")
            ov.configure(bg="black")
            ov.wm_attributes("-transparentcolor", "black")
            ov.wm_attributes("-alpha", 0.0)
            ov.attributes("-topmost", True)
            ov.protocol("WM_DELETE_WINDOW", lambda: None)
            ov.update()
            hwnd = ctypes.windll.user32.GetParent(ov.winfo_id())
            ex = ctypes.windll.user32.GetWindowLongW(hwnd, -20)
            ctypes.windll.user32.SetWindowLongW(hwnd, -20, ex | 0x00000020 | 0x00080000)
            cv = tk.Canvas(ov, width=self.sw, height=self.sh, bg="black", bd=0, highlightthickness=0)
            cv.pack()
            self._ovs[idx] = ov
            self._cvs[idx] = cv
        self._apply_to_overlay()
        self._draw_overlay()
        if self._root_cfg.get("force_topmost", False):
            self._topmost_loop()

    def _draw_overlay(self):
        ml = self._root_cfg.get("multi_layer", False)
        for idx in range(3):
            layer = self._root_cfg["layers"][idx]
            if ml and not layer.get("visible", True):
                continue
            if not ml and idx != 0:
                continue
            cv = self._cvs[idx]
            if cv is None:
                continue
            for i in self._cis[idx]:
                cv.delete(i)
            self._cis[idx].clear()
            layer = self._root_cfg["layers"][idx]
            ox = int(layer.get("offset_x", 0))
            oy = int(layer.get("offset_y", 0))
            cx, cy = self.sw // 2 + ox, self.sh // 2 + oy
            s = int(layer["size"])
            w = int(layer["thickness"])
            g = int(layer["gap"])
            style = layer["style"]
            c = layer["color"]
            if style == "image":
                if HAS_PIL:
                    path = layer.get("image_path", "")
                    if path and os.path.exists(path):
                        scale = s / 20.0
                        self._ov_photo_imgs[idx] = self._load_photo_image(
                            layer, scale=scale, angle=int(layer.get("angle", 0)))
                        if self._ov_photo_imgs[idx]:
                            self._cis[idx].append(cv.create_image(
                                cx, cy, image=self._ov_photo_imgs[idx], anchor="center"))
            else:
                draw_style(cv, style, s, w, g, c, cx, cy, self._cis[idx],
                           angle=int(layer.get("angle", 0)))

    def _topmost_loop(self):
        if self._ovs[0] is None:
            return
        try:
            if self._root_cfg.get("force_topmost", False):
                for ov in self._ovs:
                    if ov is not None:
                        ov.lift()
                        ov.attributes("-topmost", True)
        except:
            pass
        self.root.after(200, self._topmost_loop)

    def toggle(self):
        """切换当前活动图层的可见性。"""
        self.cfg["visible"] = not self.cfg["visible"]
        self._draw_overlay()
        self._apply_to_overlay()
        self._update_layer_btns()
        self._root_save()

    def _hide_settings(self):
        self.root.iconify()

    def quit(self):
        try:
            self._root_save()
        except:
            pass
        for ov in self._ovs:
            try:
                if ov is not None:
                    ov.destroy()
            except:
                pass
        try:
            self.root.destroy()
        except:
            pass
        os._exit(0)

    def run(self):
        self.root.mainloop()


def main():
    import subprocess
    subprocess.run(
        ['taskkill', '/f', '/im', 'Crosshair.exe', '/fi', f'PID ne {os.getpid()}'],
        capture_output=True, creationflags=0x08000000)
    App().run()


if __name__ == "__main__":
    main()

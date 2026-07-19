#include "settings.h"
#include "render.h"
#include "resource.h"
#include <algorithm>
#include <cstring>
#include <commdlg.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <dwmapi.h>

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")

#define WM_TRAYICON (WM_APP + 1)
#define TIMER_UPDATE 1

enum { ID_TRAY_SHOW = 1200, ID_TRAY_EXIT = 1201 };

// ---- modern dark palette ----
static const COLORREF BG      = RGB(24, 25, 29);
static const COLORREF CARD    = RGB(37, 39, 45);
static const COLORREF CARD2   = RGB(50, 53, 61);
static const COLORREF PRESS   = RGB(28, 30, 35);
static const COLORREF TEXT_C  = RGB(240, 241, 244);
static const COLORREF SUBT    = RGB(146, 150, 160);
static const COLORREF ACCENT  = RGB(10, 132, 255);
static const COLORREF ACC_HOV = RGB(48, 158, 255);
static const COLORREF GREEN_C = RGB(48, 209, 88);
static const COLORREF PREV    = RGB(15, 16, 19);
static const COLORREF RING    = RGB(64, 67, 76);
static const COLORREF GRID    = RGB(46, 48, 56);

static HFONT g_font, g_font_sm, g_font_title, g_font_sec;
static HBRUSH g_bg_brush, g_card_brush, g_prev_brush;

// ---- custom control messages ----
#define XBM_SETSTYLE  (WM_USER + 401)   // 0 normal, 1 accent, 2 subtle
#define XBM_SETSELECT (WM_USER + 402)

static const wchar_t* XSLIDER_CLASS = L"XSliderCtrl";
static const wchar_t* XTOGGLE_CLASS = L"XToggleCtrl";
static const wchar_t* XBTN_CLASS    = L"XBtnCtrl";

static Gdiplus::Color gcol(COLORREF c, BYTE a = 255) {
    return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

static void gdi_round_rect(Gdiplus::Graphics& g, Gdiplus::Brush* br, Gdiplus::Pen* pen,
                           float x, float y, float w, float h, float r) {
    if (w <= 0 || h <= 0) return;
    Gdiplus::GraphicsPath path;
    float d = (std::min)(r * 2, (std::min)(w, h));
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
    if (br) g.FillPath(br, &path);
    if (pen) g.DrawPath(pen, &path);
}

// Paint into an offscreen bitmap, then blit to target DC.
// painter receives the mem HDC; GDI+ must be scoped so GDI text works after.
template <typename F>
static void paint_dc(HDC target, int tx, int ty, int w, int h, F&& painter) {
    HDC mem = CreateCompatibleDC(target);
    HBITMAP bmp = CreateCompatibleBitmap(target, w, h);
    HGDIOBJ ob = SelectObject(mem, bmp);
    painter(mem, w, h);
    BitBlt(target, tx, ty, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, ob);
    DeleteObject(bmp);
    DeleteDC(mem);
}

// Double-buffered WM_PAINT helper for custom control windows
template <typename F>
static void paint_window(HWND hwnd, F&& painter) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w > 0 && h > 0) paint_dc(hdc, 0, 0, w, h, painter);
    EndPaint(hwnd, &ps);
}

static void track_leave(HWND hwnd) {
    TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
    TrackMouseEvent(&tme);
}

// ================= XSlider =================
struct SliderState { int minv = 0, maxv = 100, val = 0; bool drag = false, hover = false; };

static void slider_notify(HWND hwnd, int code) {
    HWND p = GetParent(hwnd);
    auto* st = (SliderState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    SendMessage(p, WM_HSCROLL, MAKEWPARAM(code, st->val), (LPARAM)hwnd);
}

static void slider_set_from_x(HWND hwnd, SliderState* st, int x) {
    RECT rc; GetClientRect(hwnd, &rc);
    float pad = 11.0f;
    float x0 = pad, x1 = (float)(rc.right) - pad;
    float t = (x1 > x0) ? (float)(x - x0) / (x1 - x0) : 0;
    if (t < 0) t = 0; if (t > 1) t = 1;
    int v = st->minv + (int)(t * (st->maxv - st->minv) + 0.5f);
    if (v < st->minv) v = st->minv;
    if (v > st->maxv) v = st->maxv;
    if (v != st->val) {
        st->val = v;
        InvalidateRect(hwnd, nullptr, FALSE);
        slider_notify(hwnd, SB_THUMBTRACK);
    }
}

static LRESULT CALLBACK xslider_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = (SliderState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)(new SliderState()));
        return DefWindowProcW(hwnd, msg, wp, lp); // default proc stores window text
    case WM_NCDESTROY:
        delete st;
        return 0;
    case TBM_SETRANGE:
        st->minv = (short)LOWORD(lp); st->maxv = (short)HIWORD(lp);
        if (st->val < st->minv) st->val = st->minv;
        if (st->val > st->maxv) st->val = st->maxv;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case TBM_SETPOS:
        st->val = (int)lp;
        if (st->val < st->minv) st->val = st->minv;
        if (st->val > st->maxv) st->val = st->maxv;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case TBM_GETPOS:
        return st->val;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        st->drag = true;
        SetCapture(hwnd);
        slider_set_from_x(hwnd, st, (int)(short)LOWORD(lp));
        return 0;
    case WM_MOUSEMOVE:
        if (!st->hover) { st->hover = true; track_leave(hwnd); InvalidateRect(hwnd, nullptr, FALSE); }
        if (st->drag) slider_set_from_x(hwnd, st, (int)(short)LOWORD(lp));
        return 0;
    case WM_MOUSELEAVE:
        st->hover = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP:
        if (st->drag) {
            st->drag = false;
            ReleaseCapture();
            slider_notify(hwnd, SB_THUMBPOSITION);
        }
        return 0;
    case WM_MOUSEWHEEL: {
        int d = (int)(short)HIWORD(wp);
        int v = st->val + (d > 0 ? 1 : -1);
        if (v < st->minv) v = st->minv;
        if (v > st->maxv) v = st->maxv;
        if (v != st->val) {
            st->val = v;
            InvalidateRect(hwnd, nullptr, FALSE);
            slider_notify(hwnd, SB_THUMBPOSITION);
        }
        return 0;
    }
    case WM_KEYDOWN: {
        int v = st->val;
        if (wp == VK_LEFT || wp == VK_DOWN) v--;
        else if (wp == VK_RIGHT || wp == VK_UP) v++;
        else break;
        if (v < st->minv) v = st->minv;
        if (v > st->maxv) v = st->maxv;
        if (v != st->val) {
            st->val = v;
            InvalidateRect(hwnd, nullptr, FALSE);
            slider_notify(hwnd, SB_THUMBPOSITION);
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        paint_window(hwnd, [&](HDC mem, int w, int h) {
            Gdiplus::Graphics g(mem);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::SolidBrush bgbr(gcol(BG));
            g.FillRectangle(&bgbr, 0, 0, w, h);
            float cy = h / 2.0f, pad = 11.0f;
            float x0 = pad, x1 = (float)w - pad;
            float t = (st->maxv > st->minv) ? (float)(st->val - st->minv) / (st->maxv - st->minv) : 0;
            float tx = x0 + t * (x1 - x0);
            // track
            Gdiplus::SolidBrush trbr(gcol(CARD2));
            gdi_round_rect(g, &trbr, nullptr, x0, cy - 2.5f, x1 - x0, 5, 2.5f);
            // filled portion
            if (tx > x0 + 1) {
                Gdiplus::SolidBrush fbr(gcol(ACCENT));
                gdi_round_rect(g, &fbr, nullptr, x0, cy - 2.5f, tx - x0, 5, 2.5f);
            }
            // thumb
            float r = (st->hover || st->drag) ? 8.5f : 7.5f;
            Gdiplus::SolidBrush wbr(Gdiplus::Color(255, 250, 250, 252));
            Gdiplus::Pen rp(gcol(RING), 1.2f);
            g.FillEllipse(&wbr, tx - r, cy - r, r * 2, r * 2);
            g.DrawEllipse(&rp, tx - r, cy - r, r * 2, r * 2);
        });
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ================= XToggle =================
struct ToggleState { bool checked = false, hover = false; };

static LRESULT CALLBACK xtoggle_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = (ToggleState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)(new ToggleState()));
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_NCDESTROY:
        delete st;
        return 0;
    case BM_SETCHECK:
        st->checked = (wp == BST_CHECKED);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case BM_GETCHECK:
        return st->checked ? BST_CHECKED : BST_UNCHECKED;
    case WM_MOUSEMOVE:
        if (!st->hover) { st->hover = true; track_leave(hwnd); InvalidateRect(hwnd, nullptr, FALSE); }
        return 0;
    case WM_MOUSELEAVE:
        st->hover = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP:
        st->checked = !st->checked;
        InvalidateRect(hwnd, nullptr, FALSE);
        SendMessage(GetParent(hwnd), WM_COMMAND,
            MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED), (LPARAM)hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        paint_window(hwnd, [&](HDC mem, int w, int h) {
            Gdiplus::Graphics g(mem);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::SolidBrush bgbr(gcol(BG));
            g.FillRectangle(&bgbr, 0, 0, w, h);
            float th = 22.0f, tw = 40.0f;
            float x = 2.0f, y = (h - th) / 2.0f;
            if (st->checked) {
                Gdiplus::SolidBrush br(gcol(st->hover ? ACC_HOV : ACCENT));
                gdi_round_rect(g, &br, nullptr, x, y, tw, th, th / 2);
            } else {
                Gdiplus::SolidBrush br(gcol(st->hover ? CARD2 : CARD));
                Gdiplus::Pen pn(gcol(RING), 1.2f);
                gdi_round_rect(g, &br, &pn, x, y, tw, th, th / 2);
            }
            float kd = 16.0f;
            float kx = st->checked ? (x + tw - kd - 3) : (x + 3);
            Gdiplus::SolidBrush kbr(Gdiplus::Color(255, 250, 250, 252));
            g.FillEllipse(&kbr, kx, y + (th - kd) / 2, kd, kd);
        });
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ================= XButton =================
struct BtnState { bool hover = false, pressed = false, selected = false; int style = 0; };

static LRESULT CALLBACK xbtn_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = (BtnState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        auto* st = new BtnState();
        st->style = (int)(INT_PTR)((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)st);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    case WM_NCDESTROY:
        delete st;
        return 0;
    case XBM_SETSTYLE:
        st->style = (int)wp;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case XBM_SETSELECT:
        st->selected = (wp != 0);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_MOUSEMOVE:
        if (!st->hover) { st->hover = true; track_leave(hwnd); InvalidateRect(hwnd, nullptr, FALSE); }
        return 0;
    case WM_MOUSELEAVE:
        st->hover = false; st->pressed = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        st->pressed = true;
        SetCapture(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP: {
        bool was = st->pressed;
        st->pressed = false;
        ReleaseCapture();
        InvalidateRect(hwnd, nullptr, FALSE);
        if (was) {
            POINT pt = { (int)(short)LOWORD(lp), (int)(short)HIWORD(lp) };
            RECT rc; GetClientRect(hwnd, &rc);
            if (PtInRect(&rc, pt)) {
                SendMessage(GetParent(hwnd), WM_COMMAND,
                    MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED), (LPARAM)hwnd);
            }
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        paint_window(hwnd, [&](HDC mem, int w, int h) {
            COLORREF fillc = CARD, textc = TEXT_C, borderc = RING;
            bool draw_border = true;
            if (st->style == 1) {          // accent
                fillc = st->pressed ? ACCENT : (st->hover ? ACC_HOV : ACCENT);
                borderc = fillc;
            } else if (st->style == 2) {   // subtle (link)
                draw_border = false;
                fillc = BG;
                textc = st->hover ? ACCENT : SUBT;
            } else {                       // normal
                if (st->pressed) fillc = PRESS;
                else if (st->hover) fillc = CARD2;
            }
            if (st->selected) { borderc = ACCENT; textc = ACCENT; }

            {
                Gdiplus::Graphics g(mem);
                g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::SolidBrush bgbr(gcol(BG));
                g.FillRectangle(&bgbr, 0, 0, w, h);
                Gdiplus::SolidBrush fbr(gcol(fillc));
                Gdiplus::Pen bpen(gcol(borderc), st->selected ? 1.6f : 1.0f);
                gdi_round_rect(g, &fbr, draw_border || st->selected ? &bpen : nullptr,
                               0.5f, 0.5f, w - 1.0f, h - 1.0f, 8);
            } // GDI+ released before GDI text

            wchar_t text[64] = {};
            GetWindowTextW(hwnd, text, 64);
            HFONT oldf = (HFONT)SelectObject(mem, g_font);
            SetBkMode(mem, TRANSPARENT);
            SetTextColor(mem, textc);
            RECT tr = { 0, 0, w, h };
            DrawTextW(mem, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(mem, oldf);
        });
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void register_controls(HINSTANCE hinst) {
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    wc.lpfnWndProc = xslider_proc;
    wc.lpszClassName = XSLIDER_CLASS;
    RegisterClassExW(&wc);

    wc.lpfnWndProc = xtoggle_proc;
    wc.lpszClassName = XTOGGLE_CLASS;
    RegisterClassExW(&wc);

    wc.lpfnWndProc = xbtn_proc;
    wc.lpszClassName = XBTN_CLASS;
    RegisterClassExW(&wc);
    done = true;
}

static void init_resources() {
    g_font       = CreateFontW(-16, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    g_font_sm    = CreateFontW(-13, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    g_font_title = CreateFontW(-26, 0,0,0, FW_BOLD,   0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    g_font_sec   = CreateFontW(-13, 0,0,0, FW_BOLD,   0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    g_bg_brush   = CreateSolidBrush(BG);
    g_card_brush = CreateSolidBrush(CARD);
    g_prev_brush = CreateSolidBrush(PREV);
}

static std::map<HWND, SettingsWindow*> g_map;
SettingsWindow* SettingsWindow::get(HWND hwnd) {
    auto it = g_map.find(hwnd);
    return it != g_map.end() ? it->second : nullptr;
}

// ---- helpers for creating controls ----
static HWND mk_label(HWND parent, HINSTANCE hinst, const wchar_t* text, int x, int y, int w, int h, HFONT f) {
    HWND s = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, parent, nullptr, hinst, nullptr);
    SendMessage(s, WM_SETFONT, (WPARAM)f, TRUE);
    return s;
}
static HWND mk_btn(HWND parent, HINSTANCE hinst, const wchar_t* text, int x, int y, int w, int h, int id, int style = 0) {
    return CreateWindowW(XBTN_CLASS, text, WS_CHILD | WS_VISIBLE, x, y, w, h,
        parent, (HMENU)(INT_PTR)id, hinst, (LPVOID)(INT_PTR)style);
}

// ---- main window proc ----
LRESULT CALLBACK SettingsWindow::wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = get(hwnd);

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lp;
        g_map[hwnd] = (SettingsWindow*)cs->lpCreateParams;
        return 0;
    }
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps; BeginPaint(hwnd, &ps); EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT r; GetClientRect(hwnd, &r);
        FillRect((HDC)wp, &r, g_bg_brush);
        return 1;
    }
    case WM_GETMINMAXINFO: {
        RECT wr = { 0, 0, self->m_window_w, self->m_window_h };
        AdjustWindowRect(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX, FALSE);
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize.x = wr.right - wr.left;
        mmi->ptMinTrackSize.y = wr.bottom - wr.top;
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        SetBkColor((HDC)wp, BG);
        SetTextColor((HDC)wp, TEXT_C);
        return (LRESULT)g_bg_brush;
    }
    case WM_CTLCOLORBTN: {
        SetBkColor((HDC)wp, BG);
        SetTextColor((HDC)wp, TEXT_C);
        return (LRESULT)g_bg_brush;
    }
    case WM_CTLCOLOREDIT: {
        SetBkColor((HDC)wp, CARD);
        SetTextColor((HDC)wp, TEXT_C);
        return (LRESULT)g_card_brush;
    }
    case WM_CTLCOLORLISTBOX: {
        SetBkColor((HDC)wp, PREV);
        SetTextColor((HDC)wp, TEXT_C);
        return (LRESULT)g_prev_brush;
    }
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mi = (MEASUREITEMSTRUCT*)lp;
        if (mi->CtlID == ID_PRESET_LIST) mi->itemHeight = 28;
        return TRUE;
    }
    case WM_DROPFILES:
        self->on_dropfile((HDROP)wp);
        return 0;
    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) { self->to_tray(); return 0; }
        InvalidateRect(self->m_preview_area, nullptr, TRUE);
        return 0;
    case WM_TRAYICON:
        if (lp == WM_LBUTTONUP) {
            self->from_tray();
        } else if (lp == WM_RBUTTONUP) {
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW, L"打开设置");
            AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"退出");
            POINT pt; GetCursorPos(&pt);
            SetForegroundWindow(hwnd); // required so menu dismisses correctly
            TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(menu);
        }
        return 0;
    case WM_TIMER:
        if (wp == TIMER_UPDATE) {
            KillTimer(hwnd, TIMER_UPDATE);
            self->update_preview();
            self->update_overlay();
        }
        return 0;
    case WM_COMMAND:
        self->on_command(LOWORD(wp), HIWORD(wp));
        return 0;
    case WM_HSCROLL:
        self->on_hscroll(LOWORD(wp), GetDlgCtrlID((HWND)lp));
        return 0;
    case WM_DRAWITEM:
        self->on_drawitem(wp, lp);
        return 0;
    case WM_DESTROY:
        self->tray_cleanup();
        g_map.erase(hwnd);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---- create ----
void SettingsWindow::create(HINSTANCE hinst, AppCfg& cfg, OverlayManager& overlay) {
    m_hinst = hinst; m_cfg = &cfg; m_overlay = &overlay;
    init_resources();
    register_controls(hinst);
    InitCommonControls();

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_bg_brush;
    wc.hIcon = LoadIconW(hinst, MAKEINTRESOURCEW(IDI_ICON1));
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = L"CrosshairSettings";
    RegisterClassExW(&wc);

    DWORD wstyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX;
    RECT wr = { 0, 0, m_window_w, m_window_h };
    AdjustWindowRect(&wr, wstyle, FALSE);
    int winw = wr.right - wr.left, winh = wr.bottom - wr.top;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);

    // Launch at right side of screen
    m_hwnd = CreateWindowExW(0, L"CrosshairSettings", L"Crosshair v3",
        wstyle, sw - winw - 30, (std::max)(0, (sh - winh) / 2), winw, winh,
        nullptr, nullptr, hinst, this);

    // Dark title bar (20 = Win11/Win10 20H1+, 19 = older Win10)
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(m_hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark))))
        DwmSetWindowAttribute(m_hwnd, 19, &dark, sizeof(dark));

    DragAcceptFiles(m_hwnd, TRUE);

    const int x0 = 20, cw = 360; // content x / width

    // ===== Header =====
    mk_label(m_hwnd, hinst, L"Crosshair v3", x0, 14, 240, 32, g_font_title);
    mk_btn(m_hwnd, hinst, L"B站",    268, 20, 44, 26, ID_BILIBILI, 2);
    mk_btn(m_hwnd, hinst, L"GitHub", 318, 20, 62, 26, ID_GITHUB, 2);

    // ===== Multi-layer toggle =====
    m_ml_toggle = CreateWindowW(XTOGGLE_CLASS, L"", WS_CHILD | WS_VISIBLE,
        x0, 56, 46, 26, m_hwnd, (HMENU)(INT_PTR)ID_ML_CHECK, hinst, nullptr);
    mk_label(m_hwnd, hinst, L"启用多层准星", 76, 58, 200, 22, g_font);
    if (cfg.multi_layer) SendMessage(m_ml_toggle, BM_SETCHECK, BST_CHECKED, 0);

    // ===== Layer tabs =====
    for (int i = 0; i < 3; i++) {
        int gx = x0 + i * 124;
        m_layer_eyes[i] = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            gx, 90, 28, 30, m_hwnd, (HMENU)(INT_PTR)(ID_LAYER_EYE0 + i), hinst, nullptr);
        wchar_t lbuf[16];
        swprintf(lbuf, 16, L"图层 %d", i + 1);
        m_layer_btns[i] = CreateWindowW(XBTN_CLASS, lbuf,
            WS_CHILD | WS_VISIBLE,
            gx + 30, 90, 82, 30, m_hwnd, (HMENU)(INT_PTR)(ID_LAYER_BTN0 + i), hinst, nullptr);
    }

    // ===== Preview =====
    m_preview_area = CreateWindowW(L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        x0, 132, cw, 88, m_hwnd, (HMENU)(INT_PTR)ID_PREVIEW, hinst, nullptr);

    // ===== Style section =====
    mk_label(m_hwnd, hinst, L"样式", x0, 228, 200, 18, g_font_sec)
        ;
    const wchar_t* style_names[] = { L"十字", L"圆圈", L"圆点", L"实心十字", L"四角", L"三角", L"图片", L"空心" };
    for (int i = 0; i < 8; i++) {
        m_style_btns[i] = CreateWindowW(XBTN_CLASS, style_names[i],
            WS_CHILD | WS_VISIBLE,
            x0 + (i % 4) * 92, 250 + (i / 4) * 36, 84, 28,
            m_hwnd, (HMENU)(INT_PTR)(ID_STYLE0 + i), hinst, nullptr);
    }

    // ===== Image picker =====
    m_img_label = mk_label(m_hwnd, hinst, L"未选择图片 · 可拖拽 PNG 到窗口", x0, 324, 250, 22, g_font_sm);
    mk_btn(m_hwnd, hinst, L"选择图片", 280, 320, 100, 28, ID_PICK_IMG);

    // ===== Colors =====
    mk_label(m_hwnd, hinst, L"颜色", x0, 356, 200, 18, g_font_sec);
    for (int i = 0; i < 8; i++) {
        m_color_btns[i] = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            21 + i * 48, 378, 32, 32, m_hwnd, (HMENU)(INT_PTR)(ID_COLOR0 + i), hinst, nullptr);
    }

    // ===== Params =====
    mk_label(m_hwnd, hinst, L"参数", x0, 420, 200, 18, g_font_sec);
    const wchar_t* sl_names[] = { L"尺寸", L"粗细", L"间隙", L"旋转", L"透明度", L"位移 X", L"位移 Y" };
    const int sl_mins[] = { 1, 1, 0, 0, 20, -300, -300 };
    const int sl_maxs[] = { 500, 30, 100, 360, 100, 300, 300 };
    for (int i = 0; i < 7; i++) {
        int y = 444 + i * 32;
        mk_label(m_hwnd, hinst, sl_names[i], x0, y + 3, 52, 20, g_font_sm);
        m_sliders[i] = CreateWindowW(XSLIDER_CLASS, L"",
            WS_CHILD | WS_VISIBLE,
            76, y, 224, 26, m_hwnd, (HMENU)(INT_PTR)(ID_SLIDER0 + i), hinst, nullptr);
        SendMessage(m_sliders[i], TBM_SETRANGE, TRUE, MAKELONG(sl_mins[i], sl_maxs[i]));

        DWORD est = WS_CHILD | WS_VISIBLE | ES_CENTER;
        if (i != SL_OFFX && i != SL_OFFY) est |= ES_NUMBER;
        m_slider_edits[i] = CreateWindowW(L"EDIT", L"", est,
            310, y, 70, 26, m_hwnd, (HMENU)(INT_PTR)(ID_SLIDER_EDIT0 + i), hinst, nullptr);
        SendMessage(m_slider_edits[i], WM_SETFONT, (WPARAM)g_font_sm, TRUE);
        SetWindowTheme(m_slider_edits[i], L"DarkMode_Explorer", nullptr);
    }

    // ===== Presets =====
    mk_label(m_hwnd, hinst, L"预设", x0, 672, 200, 18, g_font_sec);
    m_preset_list = CreateWindowW(L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | WS_VSCROLL,
        x0, 694, 224, 122, m_hwnd, (HMENU)(INT_PTR)ID_PRESET_LIST, hinst, nullptr);
    SendMessage(m_preset_list, WM_SETFONT, (WPARAM)g_font, TRUE);
    SetWindowTheme(m_preset_list, L"DarkMode_Explorer", nullptr);

    mk_btn(m_hwnd, hinst, L"保存预设", 254, 694, 126, 26, ID_SAVE_PRESET);
    mk_btn(m_hwnd, hinst, L"加载",     254, 726, 126, 26, ID_LOAD_PRESET);
    mk_btn(m_hwnd, hinst, L"删除",     254, 758, 126, 26, ID_DEL_PRESET);
    mk_btn(m_hwnd, hinst, L"重置图层", 254, 790, 126, 26, ID_RESET);

    // ===== Actions =====
    mk_btn(m_hwnd, hinst, L"显示 / 隐藏", x0,  828, 113, 32, ID_TOGGLE);
    mk_btn(m_hwnd, hinst, L"应用",        143, 828, 113, 32, ID_APPLY, 1);
    mk_btn(m_hwnd, hinst, L"退出设置",    266, 828, 113, 32, ID_HIDE);

    // ===== Footer hotkey hint =====
    HWND foot = mk_label(m_hwnd, hinst,
        L"快捷键  Ctrl + Shift + F1 / F2 / F3   开关图层 1 / 2 / 3",
        x0, 866, cw, 16, g_font_sm);
    // gray footer text handled via WM_CTLCOLORSTATIC default

    (void)foot;
    update_layer_btns();
    refresh_ui();
    update_preview();
    update_overlay();
}

void SettingsWindow::show() {
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(m_hwnd, 20, &dark, sizeof(dark))))
        DwmSetWindowAttribute(m_hwnd, 19, &dark, sizeof(dark));
    ShowWindow(m_hwnd, SW_SHOW);
}
void SettingsWindow::hide() { ShowWindow(m_hwnd, SW_HIDE); }

// ---- system tray ----
void SettingsWindow::to_tray() {
    if (m_in_tray) return;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIconW(m_hinst, MAKEINTRESOURCEW(IDI_ICON1));
    wcscpy_s(nid.szTip, L"Crosshair v3");
    if (Shell_NotifyIconW(NIM_ADD, &nid)) {
        m_in_tray = true;
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void SettingsWindow::from_tray() {
    if (!m_in_tray) return;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    m_in_tray = false;
    ShowWindow(m_hwnd, SW_RESTORE);
    SetForegroundWindow(m_hwnd);
}

void SettingsWindow::tray_cleanup() {
    if (!m_in_tray) return;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    m_in_tray = false;
}

// ---- drop file ----
void SettingsWindow::on_dropfile(HDROP drop) {
    wchar_t path[MAX_PATH];
    DragQueryFileW(drop, 0, path, MAX_PATH);
    DragFinish(drop);

    std::wstring ws(path);
    size_t dot = ws.rfind(L'.');
    if (dot == std::wstring::npos) return;
    std::wstring ext = ws.substr(dot);
    for (auto& c : ext) c = towlower(c);
    if (ext != L".png" && ext != L".jpg" && ext != L".jpeg" && ext != L".bmp" && ext != L".gif") return;

    std::string upath = wstr_to_utf8(path);
    size_t pos = upath.rfind('\\');
    std::string name = (pos != std::string::npos) ? upath.substr(pos + 1) : upath;
    std::string dest = AppCfg::img_dir() + "\\" + name;
    DeleteFileW(utf8_to_wstr(dest).c_str());
    CopyFileW(path, utf8_to_wstr(dest).c_str(), FALSE);

    m_cfg->active().image_path = dest;
    m_cfg->active().style = "image";
    m_cfg->active().visible = true;
    {
        auto* tmp = Gdiplus::Bitmap::FromFile(path);
        if (tmp && tmp->GetLastStatus() == Gdiplus::Ok) {
            int md = (std::max)(tmp->GetWidth(), tmp->GetHeight());
            int ns = md > 0 ? 10000 / md : 100;
            if (ns < 1) ns = 1; if (ns > 500) ns = 500;
            m_cfg->active().size = ns;
        }
        delete tmp;
    }
    if (m_cfg->active_layer > 0) {
        m_cfg->multi_layer = true;
        SendMessage(m_ml_toggle, BM_SETCHECK, BST_CHECKED, 0);
        update_layer_btns();
    }
    refresh_ui();
    update_preview();
    update_overlay();
    m_cfg->save();
}

// Preview image cache (per layer): avoid reloading PNG from disk on every repaint
static Gdiplus::Bitmap* g_prev_img[3] = {};
static std::string g_prev_img_key[3];
static Gdiplus::Bitmap* preview_image(int idx, const LayerCfg& layer) {
    char keybuf[40];
    snprintf(keybuf, 40, "|%d|%d", layer.size, layer.angle);
    std::string key = layer.image_path + keybuf;
    if (g_prev_img[idx] && g_prev_img_key[idx] == key) return g_prev_img[idx];
    delete g_prev_img[idx]; g_prev_img[idx] = nullptr; g_prev_img_key[idx].clear();
    g_prev_img[idx] = load_crosshair_image(layer.image_path, layer.size, layer.angle);
    if (g_prev_img[idx]) g_prev_img_key[idx] = key;
    return g_prev_img[idx];
}

// ---- owner draw ----
void SettingsWindow::on_drawitem(WPARAM wp, LPARAM lp) {
    DRAWITEMSTRUCT& di = *(DRAWITEMSTRUCT*)lp;
    int id = (int)wp;

    // ---- preset listbox items ----
    if (id == ID_PRESET_LIST) {
        HDC hdc = di.hDC;
        RECT r = di.rcItem;
        if (di.itemID == (UINT)-1) {
            FillRect(hdc, &r, g_prev_brush);
            return;
        }
        bool sel = (di.itemState & ODS_SELECTED) != 0;
        int w = r.right - r.left, h = r.bottom - r.top;
        paint_dc(hdc, r.left, r.top, w, h, [&](HDC mem, int, int) {
            {
                Gdiplus::Graphics g(mem);
                g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::SolidBrush bgbr(gcol(PREV));
                g.FillRectangle(&bgbr, 0, 0, w, h);
                if (sel) {
                    Gdiplus::SolidBrush sbr(gcol(ACCENT));
                    gdi_round_rect(g, &sbr, nullptr, 2, 2, w - 4.0f, h - 4.0f, 6);
                }
            }
            wchar_t text[256] = {};
            SendMessageW(di.hwndItem, LB_GETTEXT, di.itemID, (LPARAM)text);
            HFONT oldf = (HFONT)SelectObject(mem, g_font);
            SetBkMode(mem, TRANSPARENT);
            SetTextColor(mem, TEXT_C);
            RECT tr = { 12, 0, w - 8, h };
            DrawTextW(mem, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(mem, oldf);
        });
        return;
    }

    // ---- layer eye toggles ----
    if (id >= ID_LAYER_EYE0 && id <= ID_LAYER_EYE2) {
        int idx = id - ID_LAYER_EYE0;
        RECT r = di.rcItem;
        int w = r.right - r.left, h = r.bottom - r.top;
        paint_dc(di.hDC, r.left, r.top, w, h, [&](HDC mem, int, int) {
            Gdiplus::Graphics g(mem);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::SolidBrush bgbr(gcol(BG));
            g.FillRectangle(&bgbr, 0, 0, w, h);
            bool vis = m_cfg->layers[idx].visible;
            COLORREF c = vis ? GREEN_C : SUBT;
            float cx = w / 2.0f, cy = h / 2.0f;
            // eye outline
            Gdiplus::Pen pen(gcol(c), 1.6f);
            g.DrawEllipse(&pen, cx - 9.0f, cy - 5.5f, 18.0f, 11.0f);
            if (vis) {
                Gdiplus::SolidBrush pup(gcol(c));
                g.FillEllipse(&pup, cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
            } else {
                // slash
                Gdiplus::Pen sp(gcol(SUBT), 1.8f);
                g.DrawLine(&sp, cx - 8.0f, cy + 7.0f, cx + 8.0f, cy - 7.0f);
            }
        });
        return;
    }

    // ---- color swatches ----
    if (id >= ID_COLOR0 && id <= ID_COLOR7) {
        int ci = id - ID_COLOR0;
        RECT r = di.rcItem;
        int w = r.right - r.left, h = r.bottom - r.top;
        paint_dc(di.hDC, r.left, r.top, w, h, [&](HDC mem, int, int) {
            const char* hex_cols[] = { "#00FF00","#FF4444","#00D4FF","#FF44FF","#FFDD44","#FFFFFF","#FF8800" };
            bool custom = (ci == 7);
            float cx = w / 2.0f, cy = h / 2.0f, d = 20.0f;

            {
                Gdiplus::Graphics g(mem);
                g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::SolidBrush bgbr(gcol(BG));
                g.FillRectangle(&bgbr, 0, 0, w, h);

                if (!custom) {
                    std::string hex = hex_cols[ci];
                    int rr = (int)strtoul(hex.substr(1, 2).c_str(), nullptr, 16);
                    int gg = (int)strtoul(hex.substr(3, 2).c_str(), nullptr, 16);
                    int bb = (int)strtoul(hex.substr(5, 2).c_str(), nullptr, 16);
                    Gdiplus::SolidBrush cbr(Gdiplus::Color(255, (BYTE)rr, (BYTE)gg, (BYTE)bb));
                    g.FillEllipse(&cbr, cx - d / 2, cy - d / 2, d, d);
                    // selected ring
                    if (m_cfg->active().color == hex_cols[ci]) {
                        Gdiplus::Pen wp(Gdiplus::Color(255, 250, 250, 252), 2.0f);
                        g.DrawEllipse(&wp, cx - d / 2 - 3, cy - d / 2 - 3, d + 6, d + 6);
                    }
                } else {
                    Gdiplus::Pen gp(gcol(SUBT), 1.4f);
                    gp.SetDashStyle(Gdiplus::DashStyleDash);
                    g.DrawEllipse(&gp, cx - d / 2, cy - d / 2, d, d);
                }
            }
            if (custom) {
                SetBkMode(mem, TRANSPARENT);
                SetTextColor(mem, SUBT);
                HFONT oldf = (HFONT)SelectObject(mem, g_font);
                RECT tr = { 0, 0, w, h };
                DrawTextW(mem, L"+", 1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(mem, oldf);
            }
        });
        return;
    }

    // ---- preview card ----
    if (id == ID_PREVIEW) {
        HDC hdc = di.hDC;
        RECT r = di.rcItem;
        int pw = r.right - r.left, ph = r.bottom - r.top;
        if (pw < 4 || ph < 4) return;

        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, pw, ph);
        HGDIOBJ ob = SelectObject(mem, bmp);
        {
            Gdiplus::Graphics g(mem);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            // card
            Gdiplus::SolidBrush bgbr(gcol(BG));
            g.FillRectangle(&bgbr, 0, 0, pw, ph);
            Gdiplus::SolidBrush pbr(gcol(PREV));
            Gdiplus::Pen bpn(gcol(RING), 1.0f);
            gdi_round_rect(g, &pbr, &bpn, 0.5f, 0.5f, pw - 1.0f, ph - 1.0f, 10);
            // dot grid
            Gdiplus::SolidBrush gbr(gcol(GRID));
            for (int gx = 10; gx < pw - 4; gx += 14)
                for (int gy = 10; gy < ph - 4; gy += 14)
                    g.FillRectangle(&gbr, (float)gx, (float)gy, 1.4f, 1.4f);

            // crosshair layers with alpha
            Gdiplus::Bitmap offscreen(pw, ph, PixelFormat32bppARGB);
            Gdiplus::Graphics offg(&offscreen);
            offg.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
            RenderContext ctx; ctx.gfx = &offg;

            float cx = pw / 2.0f, cy = ph / 2.0f;
            auto draw_layer_preview = [&](int li, const LayerCfg& layer) {
                if (!layer.visible) return;
                Gdiplus::ColorMatrix cm = {};
                cm.m[0][0] = 1; cm.m[1][1] = 1; cm.m[2][2] = 1;
                cm.m[3][3] = (Gdiplus::REAL)layer.alpha; cm.m[4][4] = 1;
                Gdiplus::ImageAttributes ia;
                ia.SetColorMatrix(&cm);

                if (layer.style == "image" && !layer.image_path.empty()) {
                    auto* ib = preview_image(li, layer);
                    if (ib) {
                        offg.DrawImage(ib,
                            Gdiplus::RectF(cx - ib->GetWidth() / 2.0f, cy - ib->GetHeight() / 2.0f,
                                           (float)ib->GetWidth(), (float)ib->GetHeight()),
                            0, 0, (float)ib->GetWidth(), (float)ib->GetHeight(),
                            Gdiplus::UnitPixel, &ia);
                    }
                } else {
                    Gdiplus::Bitmap tmp(pw, ph, PixelFormat32bppARGB);
                    Gdiplus::Graphics tmpg(&tmp);
                    tmpg.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
                    RenderContext tmpctx; tmpctx.gfx = &tmpg;
                    draw_crosshair(tmpctx, layer, cx, cy);
                    offg.DrawImage(&tmp, Gdiplus::Rect(0, 0, pw, ph),
                        0, 0, pw, ph, Gdiplus::UnitPixel, &ia);
                }
            };

            if (m_cfg->multi_layer) {
                for (int i = 0; i < 3; i++) draw_layer_preview(i, m_cfg->layers[i]);
            } else {
                draw_layer_preview(m_cfg->active_layer, m_cfg->active());
            }
            g.DrawImage(&offscreen, 0, 0, pw, ph);
        }
        BitBlt(hdc, r.left, r.top, pw, ph, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob);
        DeleteObject(bmp);
        DeleteDC(mem);
        return;
    }
}

// ---- update helpers ----
void SettingsWindow::update_preview() {
    if (m_preview_area) InvalidateRect(m_preview_area, nullptr, TRUE);
}
void SettingsWindow::update_overlay() {
    m_overlay->update(*m_cfg);
}
// Coalesce rapid updates (slider drags) into one redraw per ~16ms
void SettingsWindow::schedule_update() {
    SetTimer(m_hwnd, TIMER_UPDATE, 16, nullptr);
}

static int slider_val(HWND sl) { return (int)SendMessage(sl, TBM_GETPOS, 0, 0); }
static void slider_set(HWND sl, int v) { SendMessage(sl, TBM_SETPOS, TRUE, v); }
static void edit_set(HWND ed, int v, bool pct) {
    wchar_t buf[16];
    swprintf(buf, 16, pct ? L"%d%%" : L"%d", v);
    SetWindowTextW(ed, buf);
}

// ---- refresh UI ----
void SettingsWindow::refresh_ui() {
    auto& layer = m_cfg->active();

    const char* st_names[] = { "cross","circle","dot","crosshair","corner","triangle","image","hollow" };
    int si = 0;
    for (int i = 0; i < 8; i++)
        if (layer.style == st_names[i]) { si = i; break; }
    for (int i = 0; i < 8; i++)
        SendMessage(m_style_btns[i], XBM_SETSELECT, i == si ? 1 : 0, 0);

    int vals[] = { layer.size, layer.thickness, layer.gap, layer.angle,
                   (int)(layer.alpha * 100), layer.offset_x, layer.offset_y };
    for (int i = 0; i < 7; i++) {
        slider_set(m_sliders[i], vals[i]);
        edit_set(m_slider_edits[i], vals[i], i == SL_ALPHA);
    }

    std::string img = layer.image_path;
    if (!img.empty()) {
        size_t pos = img.rfind('\\');
        SetWindowTextW(m_img_label, utf8_to_wstr(pos != std::string::npos ? img.substr(pos + 1) : img).c_str());
    } else {
        SetWindowTextW(m_img_label, L"未选择图片 · 可拖拽 PNG 到窗口");
    }

    SendMessage(m_preset_list, LB_RESETCONTENT, 0, 0);
    std::vector<std::string> names;
    for (auto& kv : m_cfg->presets) names.push_back(kv.first);
    std::sort(names.begin(), names.end());
    for (auto& n : names)
        SendMessageW(m_preset_list, LB_ADDSTRING, 0, (LPARAM)utf8_to_wstr(n).c_str());

    InvalidateRect(m_preview_area, nullptr, TRUE);
    for (int i = 0; i < 3; i++) {
        if (m_layer_eyes[i]) InvalidateRect(m_layer_eyes[i], nullptr, TRUE);
        if (m_layer_btns[i]) SendMessage(m_layer_btns[i], XBM_SETSELECT,
            (m_cfg->multi_layer && i == m_active_layer) ? 1 : 0, 0);
    }
    for (int i = 0; i < 8; i++)
        if (m_color_btns[i]) InvalidateRect(m_color_btns[i], nullptr, TRUE);
}

void SettingsWindow::update_layer_btns() {
    bool ml = m_cfg->multi_layer;
    for (int i = 0; i < 3; i++) {
        ShowWindow(m_layer_eyes[i], ml ? SW_SHOW : SW_HIDE);
        ShowWindow(m_layer_btns[i], ml ? SW_SHOW : SW_HIDE);
        SendMessage(m_layer_btns[i], XBM_SETSELECT, (ml && i == m_active_layer) ? 1 : 0, 0);
    }
    update_preview();
}

void SettingsWindow::sync_active_layer() {
    m_active_layer = m_cfg->active_layer;
}

// ---- command handling ----
void SettingsWindow::on_command(WORD id, WORD notify) {
    auto& cfg = *m_cfg;

    if (id == ID_TRAY_SHOW) { from_tray(); return; }
    if (id == ID_TRAY_EXIT) { DestroyWindow(m_hwnd); return; }
    if (id == ID_ML_CHECK) {
        cfg.multi_layer = (SendMessage(m_ml_toggle, BM_GETCHECK, 0, 0) == BST_CHECKED);
        sync_active_layer();
        update_layer_btns();
        refresh_ui();
        update_overlay();
        return;
    }
    if (id >= ID_LAYER_EYE0 && id <= ID_LAYER_EYE2) {
        toggle_layer_eye(id - ID_LAYER_EYE0);
        return;
    }
    if (id >= ID_LAYER_BTN0 && id <= ID_LAYER_BTN2) {
        int idx = id - ID_LAYER_BTN0;
        if (!cfg.multi_layer) idx = 0;
        cfg.active_layer = idx;
        sync_active_layer();
        refresh_ui();
        return;
    }
    if (id >= ID_STYLE0 && id <= ID_STYLE0 + 7) {
        const char* names[] = { "cross","circle","dot","crosshair","corner","triangle","image","hollow" };
        cfg.active().style = names[id - ID_STYLE0];
        for (int i = 0; i < 8; i++)
            SendMessage(m_style_btns[i], XBM_SETSELECT, i == (int)(id - ID_STYLE0) ? 1 : 0, 0);
        update_preview();
        update_overlay();
        return;
    }
    if (id >= ID_COLOR0 && id <= ID_COLOR7) {
        if (id == ID_COLOR7) {
            CHOOSECOLORW cc = {}; static COLORREF cust[16];
            cc.lStructSize = sizeof(cc); cc.hwndOwner = m_hwnd;
            cc.lpCustColors = cust; cc.Flags = CC_RGBINIT | CC_FULLOPEN;
            if (ChooseColorW(&cc)) {
                char buf[16]; snprintf(buf, 16, "#%02X%02X%02X",
                    GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult));
                cfg.active().color = buf;
            }
        } else {
            const char* cols[] = { "#00FF00","#FF4444","#00D4FF","#FF44FF","#FFDD44","#FFFFFF","#FF8800" };
            cfg.active().color = cols[id - ID_COLOR0];
        }
        refresh_ui();
        update_preview();
        update_overlay();
        return;
    }
    if (id >= ID_SLIDER_EDIT0 && id <= ID_SLIDER_EDIT0 + 6) {
        if (notify == EN_CHANGE) on_slider_edit(id - ID_SLIDER_EDIT0);
        return;
    }
    if (id == ID_PICK_IMG)      { pick_image(); return; }
    if (id == ID_SAVE_PRESET)   { save_preset(); return; }
    if (id == ID_LOAD_PRESET)   { load_preset(); return; }
    if (id == ID_DEL_PRESET)    { delete_preset(); return; }
    if (id == ID_RESET)         { reset_layer(); return; }
    if (id == ID_TOGGLE) {
        // Master switch: hide/show the on-screen crosshair only; preview unaffected
        cfg.overlay_visible = !cfg.overlay_visible;
        update_overlay();
        cfg.save();
        return;
    }
    if (id == ID_APPLY)         { cfg.save(); update_overlay(); return; }
    if (id == ID_HIDE)          { ShowWindow(m_hwnd, SW_MINIMIZE); return; }
    if (id == ID_BILIBILI) {
        ShellExecuteW(nullptr, L"open", L"https://space.bilibili.com/187682531", nullptr, nullptr, SW_SHOW);
        return;
    }
    if (id == ID_GITHUB) {
        ShellExecuteW(nullptr, L"open", L"https://github.com/weijsss/Crosshair", nullptr, nullptr, SW_SHOW);
        return;
    }
    if (id == ID_PRESET_LIST && notify == LBN_DBLCLK) { load_preset(); return; }
}

void SettingsWindow::on_hscroll(WORD code, int id) {
    int idx = id - ID_SLIDER0;
    if (idx < 0 || idx >= 7) return;

    int raw = slider_val(m_sliders[idx]);

    if (idx == SL_ALPHA)        m_cfg->active().alpha = raw / 100.0;
    else if (idx == SL_SIZE)    m_cfg->active().size = raw;
    else if (idx == SL_THICK)   m_cfg->active().thickness = raw;
    else if (idx == SL_GAP)     m_cfg->active().gap = raw;
    else if (idx == SL_ANGLE)   m_cfg->active().angle = raw;
    else if (idx == SL_OFFX)    m_cfg->active().offset_x = raw;
    else if (idx == SL_OFFY)    m_cfg->active().offset_y = raw;

    edit_set(m_slider_edits[idx], raw, idx == SL_ALPHA);
    update_preview();
    schedule_update();
}

void SettingsWindow::on_slider_edit(int idx) {
    wchar_t buf[32];
    GetWindowTextW(m_slider_edits[idx], buf, 32);
    int val = _wtoi(buf);

    if (idx == SL_ALPHA) {
        if (val < 20) val = 20; if (val > 100) val = 100;
        m_cfg->active().alpha = val / 100.0;
        slider_set(m_sliders[idx], val);
    } else if (idx == SL_OFFX || idx == SL_OFFY) {
        if (val < -9999) val = -9999; if (val > 9999) val = 9999;
        if (idx == SL_OFFX) m_cfg->active().offset_x = val;
        else m_cfg->active().offset_y = val;
        slider_set(m_sliders[idx], (std::min)(300, (std::max)(-300, val)));
    } else {
        int mins[] = { 1,1,0,0 }, maxs[] = { 500,30,100,360 };
        if (val < mins[idx]) val = mins[idx];
        if (val > maxs[idx]) val = maxs[idx];
        if (idx == SL_SIZE)  m_cfg->active().size = val;
        else if (idx == SL_THICK) m_cfg->active().thickness = val;
        else if (idx == SL_GAP)   m_cfg->active().gap = val;
        else if (idx == SL_ANGLE) m_cfg->active().angle = val;
        slider_set(m_sliders[idx], val);
    }

    update_preview();
    schedule_update();
}

// ---- actions ----
void SettingsWindow::pick_image() {
    OPENFILENAMEW ofn = {};
    wchar_t buf[MAX_PATH] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"图片\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0所有文件\0*.*\0\0";
    ofn.lpstrFile = buf; ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        std::string path = wstr_to_utf8(buf);
        size_t pos = path.rfind('\\');
        std::string name = (pos != std::string::npos) ? path.substr(pos + 1) : path;
        std::string dest = AppCfg::img_dir() + "\\" + name;
        DeleteFileW(utf8_to_wstr(dest).c_str());
        CopyFileW(buf, utf8_to_wstr(dest).c_str(), FALSE);
        m_cfg->active().image_path = dest;
        m_cfg->active().style = "image";
        m_cfg->active().visible = true;
        {
            auto* tmp = Gdiplus::Bitmap::FromFile(buf);
            if (tmp && tmp->GetLastStatus() == Gdiplus::Ok) {
                int md = (std::max)(tmp->GetWidth(), tmp->GetHeight());
                int ns = md > 0 ? 10000 / md : 100;
                if (ns < 1) ns = 1; if (ns > 500) ns = 500;
                m_cfg->active().size = ns;
            }
            delete tmp;
        }
        if (m_cfg->active_layer > 0) {
            m_cfg->multi_layer = true;
            SendMessage(m_ml_toggle, BM_SETCHECK, BST_CHECKED, 0);
            update_layer_btns();
        }
        refresh_ui();
        update_preview();
        update_overlay();
        m_cfg->save();
    }
}

void SettingsWindow::save_preset() {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = m_hinst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = g_bg_brush;
        wc.lpszClassName = L"CrosshairSaveDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    int dlg_w = 280, dlg_h = 110;
    HWND dlg = CreateWindowExW(WS_EX_TOPMOST, L"CrosshairSaveDlg", L"保存预设",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, dlg_w, dlg_h, m_hwnd, nullptr, m_hinst, nullptr);

    // Dark title bar before showing, then force frame redraw
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(dlg, 20, &dark, sizeof(dark))))
        DwmSetWindowAttribute(dlg, 19, &dark, sizeof(dark));

    RECT pr; GetWindowRect(m_hwnd, &pr);
    RECT dr = { 0, 0, dlg_w, dlg_h };
    AdjustWindowRect(&dr, WS_CAPTION, FALSE);
    SetWindowPos(dlg, nullptr,
        pr.left + (pr.right - pr.left - (dr.right - dr.left)) / 2,
        pr.top + (pr.bottom - pr.top - (dr.bottom - dr.top)) / 2,
        dr.right - dr.left, dr.bottom - dr.top,
        SWP_NOZORDER | SWP_FRAMECHANGED);

    struct DlgData { bool done = false; wchar_t name[64] = {}; };
    DlgData data{};

    // Subclass BEFORE showing so first paint already uses dark colors
    SetWindowSubclass(dlg, [](HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref) -> LRESULT {
        DlgData* d = (DlgData*)ref;
        if (msg == WM_CTLCOLORSTATIC) {
            SetBkColor((HDC)wp, BG);
            SetTextColor((HDC)wp, TEXT_C);
            return (LRESULT)g_bg_brush;
        }
        if (msg == WM_CTLCOLOREDIT) {
            SetBkColor((HDC)wp, CARD);
            SetTextColor((HDC)wp, TEXT_C);
            return (LRESULT)g_card_brush;
        }
        if (msg == WM_ERASEBKGND) {
            RECT r; GetClientRect(h, &r);
            FillRect((HDC)wp, &r, g_bg_brush);
            return 1;
        }
        if (msg == WM_COMMAND) {
            if (LOWORD(wp) == 1) {
                GetWindowTextW(GetDlgItem(h, 100), d->name, 64);
                d->done = true;
                return 0;
            }
            if (LOWORD(wp) == 2) {
                d->name[0] = 0;
                d->done = true;
                return 0;
            }
        }
        if (msg == WM_CLOSE || (msg == WM_SYSCOMMAND && (wp & 0xFFF0) == SC_CLOSE)) {
            d->name[0] = 0;
            d->done = true;
            return 0;
        }
        return DefSubclassProc(h, msg, wp, lp);
    }, 0, (DWORD_PTR)&data);

    // Create controls after subclassing, then show
    HWND lb = CreateWindowW(L"STATIC", L"预设名称",
        WS_CHILD | WS_VISIBLE, 14, 16, 70, 22, dlg, nullptr, m_hinst, nullptr);
    SendMessage(lb, WM_SETFONT, (WPARAM)g_font, TRUE);

    HWND ed = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        88, 14, 176, 26, dlg, (HMENU)100, m_hinst, nullptr);
    SendMessage(ed, WM_SETFONT, (WPARAM)g_font, TRUE);
    SetWindowTheme(ed, L"DarkMode_Explorer", nullptr);

    HWND ok = CreateWindowW(XBTN_CLASS, L"确定",
        WS_CHILD | WS_VISIBLE, 70, 54, 80, 28, dlg, (HMENU)1, m_hinst, (LPVOID)(INT_PTR)1);
    CreateWindowW(XBTN_CLASS, L"取消",
        WS_CHILD | WS_VISIBLE, 160, 54, 80, 28, dlg, (HMENU)2, m_hinst, nullptr);

    ShowWindow(dlg, SW_SHOW);
    SetFocus(ed);

    while (!data.done) {
        MSG m;
        if (GetMessage(&m, nullptr, 0, 0)) {
            if (!IsDialogMessage(dlg, &m)) {
                TranslateMessage(&m);
                DispatchMessage(&m);
            }
        }
    }

    if (data.name[0]) {
        JsonObj pr;
        pr["multi_layer"] = m_cfg->multi_layer;
        pr["force_topmost"] = m_cfg->force_topmost;
        pr["active_layer"] = double(m_cfg->active_layer);
        JsonArr la;
        for (int i = 0; i < 3; i++) la.push_back(m_cfg->layers[i].to_json());
        pr["layers"] = la;
        m_cfg->presets[wstr_to_utf8(data.name)] = pr;
        m_cfg->save();
        refresh_ui();
    }

    DestroyWindow(dlg);
    SetForegroundWindow(m_hwnd);
}

void SettingsWindow::load_preset() {
    int sel = (int)SendMessage(m_preset_list, LB_GETCURSEL, 0, 0);
    if (sel < 0) return;

    wchar_t buf[256];
    SendMessageW(m_preset_list, LB_GETTEXT, sel, (LPARAM)buf);
    std::string name = wstr_to_utf8(buf);
    if (!m_cfg->presets.count(name)) return;

    auto& pr = m_cfg->presets[name];
    if (pr.contains("multi_layer"))   m_cfg->multi_layer   = pr["multi_layer"].as_bool();
    if (pr.contains("active_layer"))  m_cfg->active_layer  = pr["active_layer"].as_int();
    if (pr.contains("force_topmost")) m_cfg->force_topmost = pr["force_topmost"].as_bool();
    if (pr.contains("layers") && pr["layers"].is_arr()) {
        auto& arr = pr["layers"].as_arr();
        for (int i = 0; i < 3 && i < (int)arr.size(); i++)
            m_cfg->layers[i] = LayerCfg::from_json(arr[i]);
    }

    SendMessage(m_ml_toggle, BM_SETCHECK, m_cfg->multi_layer ? BST_CHECKED : BST_UNCHECKED, 0);
    sync_active_layer();
    update_layer_btns();
    refresh_ui();
    update_preview();
    update_overlay();
    m_cfg->save();
}

void SettingsWindow::delete_preset() {
    int sel = (int)SendMessage(m_preset_list, LB_GETCURSEL, 0, 0);
    if (sel < 0) return;
    wchar_t buf[256];
    SendMessageW(m_preset_list, LB_GETTEXT, sel, (LPARAM)buf);
    m_cfg->presets.erase(wstr_to_utf8(buf));
    m_cfg->save();
    refresh_ui();
}

void SettingsWindow::reset_layer() {
    m_cfg->active() = LayerCfg();
    sync_active_layer();
    refresh_ui();
    update_preview();
    update_overlay();
    m_cfg->save();
}

void SettingsWindow::reset_layer_by_idx(int idx) {
    if (idx < 0 || idx > 2) return;
    m_cfg->layers[idx] = LayerCfg();
    if (idx == 1) { m_cfg->layers[1].color = "#FF4444"; m_cfg->layers[1].size = 15; m_cfg->layers[1].style = "dot"; }
    if (idx == 2) { m_cfg->layers[2].color = "#00D4FF"; m_cfg->layers[2].size = 10; m_cfg->layers[2].style = "circle"; m_cfg->layers[2].visible = false; }
    m_cfg->active_layer = idx;
    sync_active_layer();
    refresh_ui();
    update_preview();
    update_overlay();
    m_cfg->save();
}

void SettingsWindow::toggle_layer_eye(int idx) {
    if (idx < 0 || idx > 2) return;
    // Only toggle visibility; do NOT change the active layer
    m_cfg->layers[idx].visible = !m_cfg->layers[idx].visible;
    refresh_ui();
    update_preview();
    update_overlay();
    m_cfg->save();
}

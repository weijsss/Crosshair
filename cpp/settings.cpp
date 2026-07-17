#include "settings.h"
#include "render.h"
#include <algorithm>
#include <cstring>
#include <commdlg.h>
#include <shlobj.h>
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

// ---- Apple dark palette (BGR) ----
static const COLORREF BG      = 0x1E1C1C;
static const COLORREF CARD    = 0x2E2C2C;
static const COLORREF TEXT_C  = 0xF7F5F5;
static const COLORREF ACCENT  = 0xFF840A;
static const COLORREF GREEN_C = 0x58D130;
static const COLORREF PREV    = 0x000000;
static const COLORREF RING    = 0x666363;

static HFONT g_font, g_font_title;
static HBRUSH g_bg_brush, g_card_brush, g_prev_brush;

static void init_resources() {
    g_font       = CreateFontW(17, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_font_title = CreateFontW(28, 0,0,0, FW_BOLD,    0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    g_bg_brush   = CreateSolidBrush(BG);
    g_card_brush = CreateSolidBrush(CARD);
    g_prev_brush = CreateSolidBrush(PREV);
}

static std::map<HWND, SettingsWindow*> g_map;
SettingsWindow* SettingsWindow::get(HWND hwnd) {
    auto it = g_map.find(hwnd);
    return it != g_map.end() ? it->second : nullptr;
}

// ---- trackbar subclass: smooth click-to-position + custom drag ----
static LRESULT CALLBACK tb_subclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    static bool dragging = false;

    auto set_thumb = [&](int pos, WPARAM code) {
        int rmin = (int)SendMessage(hwnd, TBM_GETRANGEMIN, 0, 0);
        int rmax = (int)SendMessage(hwnd, TBM_GETRANGEMAX, 0, 0);
        if (pos < rmin) pos = rmin;
        if (pos > rmax) pos = rmax;
        SendMessage(hwnd, TBM_SETPOS, TRUE, pos);
        HWND p = GetParent(hwnd);
        SendMessage(p, WM_HSCROLL, MAKEWPARAM(code, pos), (LPARAM)hwnd);
    };

    if (msg == WM_LBUTTONDOWN) {
        RECT rc; GetClientRect(hwnd, &rc);
        int x = (int)(short)LOWORD(lp);
        int rmin = (int)SendMessage(hwnd, TBM_GETRANGEMIN, 0, 0);
        int rmax = (int)SendMessage(hwnd, TBM_GETRANGEMAX, 0, 0);
        int thumb = (int)SendMessage(hwnd, TBM_GETTHUMBLENGTH, 0, 0);
        int range = rmax - rmin; if (range == 0) range = 1;
        int avail = rc.right - rc.left - thumb; if (avail <= 0) avail = 1;

        int cur_val = (int)SendMessage(hwnd, TBM_GETPOS, 0, 0);
        int cur_x = thumb/2 + (cur_val - rmin) * avail / range;

        if (abs(x - cur_x) <= thumb * 3) {
            // Start custom drag
            dragging = true;
            SetCapture(hwnd);
            return 0;
        }
        // Click on channel: jump (use THUMBPOSITION so overlay updates)
        set_thumb(rmin + (x - thumb/2) * range / avail, SB_THUMBPOSITION);
        return 0;
    }

    if (msg == WM_MOUSEMOVE && dragging) {
        RECT rc; GetClientRect(hwnd, &rc);
        int x = (int)(short)LOWORD(lp);
        int rmin = (int)SendMessage(hwnd, TBM_GETRANGEMIN, 0, 0);
        int rmax = (int)SendMessage(hwnd, TBM_GETRANGEMAX, 0, 0);
        int thumb = (int)SendMessage(hwnd, TBM_GETTHUMBLENGTH, 0, 0);
        int range = rmax - rmin; if (range == 0) range = 1;
        int avail = rc.right - rc.left - thumb; if (avail <= 0) avail = 1;
        set_thumb(rmin + (x - thumb/2) * range / avail, SB_THUMBTRACK);
        return 0;
    }

    if (msg == WM_LBUTTONUP && dragging) {
        dragging = false;
        ReleaseCapture();
        int pos = (int)SendMessage(hwnd, TBM_GETPOS, 0, 0);
        HWND p = GetParent(hwnd);
        SendMessage(p, WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, pos), (LPARAM)hwnd);
        return 0;
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
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
        SetBkColor((HDC)wp, PREV);
        SetTextColor((HDC)wp, TEXT_C);
        return (LRESULT)g_prev_brush;
    }
    case WM_CTLCOLORLISTBOX: {
        SetBkColor((HDC)wp, PREV);
        SetTextColor((HDC)wp, TEXT_C);
        return (LRESULT)g_prev_brush;
    }
    case WM_DROPFILES:
        self->on_dropfile((HDROP)wp);
        return 0;
    case WM_SIZE: {
        // On resize, invalidate preview to redraw
        InvalidateRect(self->m_preview_area, nullptr, TRUE);
        return 0;
    }
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
    InitCommonControls();

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_bg_brush;
    wc.lpszClassName = L"CrosshairSettings";
    RegisterClassExW(&wc);

    RECT wr = {0, 0, m_window_w, m_window_h};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);

    m_hwnd = CreateWindowExW(0, L"CrosshairSettings", L"Crosshair v3",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX,
        sw - wr.right + wr.left - 30, (std::max)(0, (int)(sh - (wr.bottom - wr.top) - 40)),
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, hinst, this);

    DragAcceptFiles(m_hwnd, TRUE);

    // ---- Build UI directly on m_hwnd ----
    int y = 12;
    int x0 = 16;

    // Title + links
    HWND title = CreateWindowW(L"STATIC", L"Crosshair",
        WS_CHILD | WS_VISIBLE, x0, y, 120, 30, m_hwnd, nullptr, hinst, nullptr);
    SendMessage(title, WM_SETFONT, (WPARAM)g_font_title, TRUE);
    // B站 link
    HWND bili = CreateWindowW(L"BUTTON", L"B站",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        250, y + 2, 36, 24, m_hwnd, (HMENU)(INT_PTR)ID_BILIBILI, hinst, nullptr);
    // GitHub link
    HWND gh = CreateWindowW(L"BUTTON", L"GitHub",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        290, y + 2, 50, 24, m_hwnd, (HMENU)(INT_PTR)ID_GITHUB, hinst, nullptr);
    y += 32;

    // Multi-layer checkbox
    m_ml_check = CreateWindowW(L"BUTTON", L"启用多层准星",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x0, y, 200, 26, m_hwnd, (HMENU)(INT_PTR)ID_ML_CHECK, hinst, nullptr);
    SetWindowTheme(m_ml_check, L"", L"");
    SendMessage(m_ml_check, WM_SETFONT, (WPARAM)g_font, TRUE);
    if (cfg.multi_layer) SendMessage(m_ml_check, BM_SETCHECK, BST_CHECKED, 0);
    y += 36;

    // Layer area: eye toggle button + layer name button
    for (int i = 0; i < 3; i++) {
        m_layer_eyes[i] = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            x0 + i * 105, y, 24, 28, m_hwnd, (HMENU)(INT_PTR)(ID_LAYER_EYE0 + i), hinst, nullptr);
        wchar_t lbuf[16];
        swprintf(lbuf, 16, L"图层 %d", i + 1);
        HWND lb = CreateWindowW(L"BUTTON", lbuf,
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            x0 + i * 105 + 26, y, 72, 28, m_hwnd, (HMENU)(INT_PTR)(ID_LAYER_BTN0 + i), hinst, nullptr);
        m_layer_btns[i] = lb;
    }
    y += 36;

    // Preview
    m_preview_area = CreateWindowW(L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        x0, y, 308, 64, m_hwnd, (HMENU)(INT_PTR)ID_PREVIEW, hinst, nullptr);
    y += 74;

    // Style radios: BS_AUTORADIOBUTTON, WS_GROUP on first
    const wchar_t* style_names[] = { L"十字", L"圆圈", L"圆点", L"实心十字", L"四角", L"三角", L"图片", L"空心" };
    for (int i = 0; i < 8; i++) {
        DWORD st = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
        if (i == 0) st |= WS_GROUP;
        m_style_radios[i] = CreateWindowW(L"BUTTON", style_names[i],
            st, x0 + (i % 3) * 100, y + (i / 3) * 28, 90, 22,
            m_hwnd, (HMENU)(INT_PTR)(ID_STYLE0 + i), hinst, nullptr);
        SetWindowTheme(m_style_radios[i], L"", L"");
        SendMessage(m_style_radios[i], WM_SETFONT, (WPARAM)g_font, TRUE);
    }
    y += 110;

    // Image picker
    m_img_label = CreateWindowW(L"STATIC", L"未选择图片",
        WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS,
        x0, y + 2, 210, 22, m_hwnd, nullptr, hinst, nullptr);
    SendMessage(m_img_label, WM_SETFONT, (WPARAM)g_font, TRUE);
    HWND pick_btn = CreateWindowW(L"BUTTON", L"选择PNG",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x0 + 215, y, 85, 26, m_hwnd, (HMENU)(INT_PTR)ID_PICK_IMG, hinst, nullptr);
    y += 36;

    // Color swatches (owner-draw)
    for (int i = 0; i < 8; i++) {
        m_color_btns[i] = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            x0 + i * 38, y, 30, 30, m_hwnd, (HMENU)(INT_PTR)(ID_COLOR0 + i), hinst, nullptr);
    }
    y += 44;

    // Sliders with labels and edit boxes
    const wchar_t* sl_names[]  = { L"尺寸", L"粗细", L"间隙", L"旋转", L"透明度" };
    const int sl_mins[]  = {1, 1, 0, 0, 20};
    const int sl_maxs[]  = {500, 30, 100, 360, 100};
    for (int i = 0; i < 5; i++) {
        HWND lb = CreateWindowW(L"STATIC", sl_names[i],
            WS_CHILD | WS_VISIBLE, x0, y + 2, 50, 22, m_hwnd, nullptr, hinst, nullptr);
        SendMessage(lb, WM_SETFONT, (WPARAM)g_font, TRUE);

        m_sliders[i] = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            x0 + 56, y, 150, 28, m_hwnd, (HMENU)(INT_PTR)(ID_SLIDER0 + i), hinst, nullptr);
        SendMessage(m_sliders[i], TBM_SETRANGE, TRUE, MAKELONG(sl_mins[i], sl_maxs[i]));
        SetWindowSubclass(m_sliders[i], tb_subclass, 0, 0);

        m_slider_edits[i] = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_NUMBER,
            x0 + 212, y, 50, 24, m_hwnd, (HMENU)(INT_PTR)(ID_SLIDER_EDIT0 + i), hinst, nullptr);
        y += 32;
    }

    // Position sliders
    const wchar_t* pos_names[] = { L"位移X", L"位移Y" };
    for (int i = 0; i < 2; i++) {
        int idx = SL_OFFX + i;
        HWND lb = CreateWindowW(L"STATIC", pos_names[i],
            WS_CHILD | WS_VISIBLE, x0, y + 2, 50, 22, m_hwnd, nullptr, hinst, nullptr);
        SendMessage(lb, WM_SETFONT, (WPARAM)g_font, TRUE);

        m_sliders[idx] = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            x0 + 56, y, 150, 28, m_hwnd, (HMENU)(INT_PTR)(ID_SLIDER0 + idx), hinst, nullptr);
        SendMessage(m_sliders[idx], TBM_SETRANGE, TRUE, MAKELONG(-300, 300));
        SetWindowSubclass(m_sliders[idx], tb_subclass, 0, 0);

        m_slider_edits[idx] = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER,
            x0 + 212, y, 50, 24, m_hwnd, (HMENU)(INT_PTR)(ID_SLIDER_EDIT0 + idx), hinst, nullptr);
        y += 32;
    }

    // Presets list
    m_preset_list = CreateWindowW(L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | WS_VSCROLL,
        x0, y, 308, 150, m_hwnd, (HMENU)(INT_PTR)ID_PRESET_LIST, hinst, nullptr);
    SendMessage(m_preset_list, WM_SETFONT, (WPARAM)g_font, TRUE);
    y += 158;

    // Preset buttons
    CreateWindowW(L"BUTTON", L"保存预设",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x0,       y, 70, 26, m_hwnd, (HMENU)(INT_PTR)ID_SAVE_PRESET, hinst, nullptr);
    CreateWindowW(L"BUTTON", L"加载",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x0 + 74,  y, 55, 26, m_hwnd, (HMENU)(INT_PTR)ID_LOAD_PRESET, hinst, nullptr);
    CreateWindowW(L"BUTTON", L"删除",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x0 + 133, y, 55, 26, m_hwnd, (HMENU)(INT_PTR)ID_DEL_PRESET, hinst, nullptr);
    CreateWindowW(L"BUTTON", L"重置",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x0 + 192, y, 55, 26, m_hwnd, (HMENU)(INT_PTR)ID_RESET, hinst, nullptr);
    y += 36;

    // Action buttons
    CreateWindowW(L"BUTTON", L"显示/隐藏",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x0,       y, 90, 28, m_hwnd, (HMENU)(INT_PTR)ID_TOGGLE, hinst, nullptr);
    CreateWindowW(L"BUTTON", L"应用",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x0 + 99,  y, 70, 28, m_hwnd, (HMENU)(INT_PTR)ID_APPLY, hinst, nullptr);
    CreateWindowW(L"BUTTON", L"退出设置",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x0 + 179, y, 90, 28, m_hwnd, (HMENU)(INT_PTR)ID_HIDE, hinst, nullptr);
    y += 36;

    // ---- Hotkey info ----
    HWND hk;
    hk = CreateWindowW(L"STATIC", L"键盘快捷键", WS_CHILD|WS_VISIBLE, x0, y+4, 100, 20, m_hwnd, nullptr, hinst, nullptr);
    SendMessage(hk, WM_SETFONT, (WPARAM)g_font, TRUE);
    hk = CreateWindowW(L"STATIC", L"Ctrl+Shift+F1   开关图层 1", WS_CHILD|WS_VISIBLE, x0+4, y+26, 220, 18, m_hwnd, nullptr, hinst, nullptr);
    SendMessage(hk, WM_SETFONT, (WPARAM)g_font, TRUE);
    hk = CreateWindowW(L"STATIC", L"Ctrl+Shift+F2   开关图层 2", WS_CHILD|WS_VISIBLE, x0+4, y+46, 220, 18, m_hwnd, nullptr, hinst, nullptr);
    SendMessage(hk, WM_SETFONT, (WPARAM)g_font, TRUE);
    hk = CreateWindowW(L"STATIC", L"Ctrl+Shift+F3   开关图层 3", WS_CHILD|WS_VISIBLE, x0+4, y+66, 220, 18, m_hwnd, nullptr, hinst, nullptr);
    SendMessage(hk, WM_SETFONT, (WPARAM)g_font, TRUE);

    update_layer_btns();
    refresh_ui();
    update_preview();
    update_overlay();
}

void SettingsWindow::show() { ShowWindow(m_hwnd, SW_SHOW); }
void SettingsWindow::hide() { ShowWindow(m_hwnd, SW_HIDE); }

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
    // Auto-size
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
        SendMessage(m_ml_check, BM_SETCHECK, BST_CHECKED, 0);
        update_layer_btns();
    }
    refresh_ui();
    update_preview();
    update_overlay();
    m_cfg->save();
}

// ---- owner draw ----
void SettingsWindow::on_drawitem(WPARAM wp, LPARAM lp) {
    DRAWITEMSTRUCT& di = *(DRAWITEMSTRUCT*)lp;
    int id = (int)wp;

    // Layer eye toggle buttons
    if (id >= ID_LAYER_EYE0 && id <= ID_LAYER_EYE2) {
        int idx = id - ID_LAYER_EYE0;
        HDC hdc = di.hDC;
        RECT r = di.rcItem;

        HBRUSH fill = CreateSolidBrush(BG);
        FillRect(hdc, &r, fill);
        DeleteObject(fill);

        bool vis = m_cfg->layers[idx].visible;
        int m = 4; // margin
        int s = (r.right - r.left) - m * 2;

        if (vis) {
            // Green circle
            HBRUSH gb = CreateSolidBrush(GREEN_C);
            HPEN gp = CreatePen(PS_SOLID, 1, GREEN_C);
            HGDIOBJ ob = SelectObject(hdc, gb);
            HGDIOBJ op = SelectObject(hdc, gp);
            Ellipse(hdc, r.left + m, r.top + m, r.left + m + s, r.top + m + s);
            SelectObject(hdc, ob); SelectObject(hdc, op);
            DeleteObject(gb); DeleteObject(gp);
        } else {
            // Just an X
            HPEN xp = CreatePen(PS_SOLID, 2, RING);
            HGDIOBJ op = SelectObject(hdc, xp);
            MoveToEx(hdc, r.left + m + 2, r.top + m + 2, nullptr);
            LineTo(hdc, r.right - m - 2, r.bottom - m - 2);
            MoveToEx(hdc, r.right - m - 2, r.top + m + 2, nullptr);
            LineTo(hdc, r.left + m + 2, r.bottom - m - 2);
            SelectObject(hdc, op);
            DeleteObject(xp);
        }

        if (di.itemState & ODS_SELECTED) {
            HPEN fp = CreatePen(PS_SOLID, 1, ACCENT);
            HGDIOBJ op2 = SelectObject(hdc, fp);
            HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, r.left, r.top, r.right, r.bottom);
            SelectObject(hdc, ob); SelectObject(hdc, op2);
            DeleteObject(fp);
        }
        return;
    }

    // B站 / GitHub link buttons
    if (id == ID_BILIBILI || id == ID_GITHUB) {
        HDC hdc = di.hDC;
        RECT r = di.rcItem;

        HBRUSH fill = CreateSolidBrush(BG);
        FillRect(hdc, &r, fill);
        DeleteObject(fill);

        wchar_t text[32];
        GetWindowTextW(di.hwndItem, text, 32);
        HFONT uf = CreateFontW(15, 0,0,0, FW_NORMAL, 0,1,0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HFONT old_f = (HFONT)SelectObject(hdc, uf);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, ACCENT);
        DrawTextW(hdc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old_f);
        DeleteObject(uf);
        return;
    }

    // Regular push buttons + layer name buttons
    if ((id >= ID_PICK_IMG && id <= ID_PICK_IMG) ||
        (id >= ID_SAVE_PRESET && id <= ID_RESET) ||
        (id >= ID_TOGGLE && id <= ID_HIDE) ||
        (id >= ID_LAYER_BTN0 && id <= ID_LAYER_BTN2)) {
        HDC hdc = di.hDC;
        RECT r = di.rcItem;

        bool pressed = (di.itemState & ODS_SELECTED) != 0;
        bool is_active_layer = (id >= ID_LAYER_BTN0 && id <= ID_LAYER_BTN2)
            && (id - ID_LAYER_BTN0 == m_active_layer);
        COLORREF bg = pressed ? ACCENT : (is_active_layer ? ACCENT : CARD);
        HBRUSH fill = CreateSolidBrush(bg);
        HPEN pen = CreatePen(PS_SOLID, 1, RING);
        HGDIOBJ old_br = SelectObject(hdc, fill);
        HGDIOBJ old_pen = SelectObject(hdc, pen);
        RoundRect(hdc, r.left, r.top, r.right, r.bottom, 6, 6);
        SelectObject(hdc, old_br); SelectObject(hdc, old_pen);
        DeleteObject(fill); DeleteObject(pen);

        wchar_t text[64];
        GetWindowTextW(di.hwndItem, text, 64);
        HFONT old_f = (HFONT)SelectObject(hdc, g_font);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, TEXT_C);
        DrawTextW(hdc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old_f);

        if (di.itemState & ODS_FOCUS) {
            RECT fr = {r.left+3, r.top+3, r.right-3, r.bottom-3};
            DrawFocusRect(hdc, &fr);
        }
        return;
    }

    // Color swatches
    if (id >= ID_COLOR0 && id <= ID_COLOR7) {
        HDC hdc = di.hDC;
        RECT r = di.rcItem;

        HBRUSH fill = CreateSolidBrush(BG);
        FillRect(hdc, &r, fill);
        DeleteObject(fill);

        COLORREF col = 0;
        if (id == ID_COLOR7) {
            col = RING;
        } else {
            const char* hex_cols[] = {"#00FF00","#FF4444","#00D4FF","#FF44FF","#FFDD44","#FFFFFF","#FF8800"};
            std::string hex = hex_cols[id - ID_COLOR0];
            unsigned int rr = strtoul(hex.substr(1,2).c_str(), nullptr, 16);
            unsigned int gg = strtoul(hex.substr(3,2).c_str(), nullptr, 16);
            unsigned int bb = strtoul(hex.substr(5,2).c_str(), nullptr, 16);
            col = RGB(rr, gg, bb);
        }

        int m = 4;
        RECT inner = {r.left + m, r.top + m, r.right - m, r.bottom - m};
        HBRUSH cb = CreateSolidBrush(col);
        HPEN pen = CreatePen(PS_SOLID, 1, RING);
        HGDIOBJ old_br = SelectObject(hdc, cb);
        HGDIOBJ old_pen = SelectObject(hdc, pen);
        RoundRect(hdc, inner.left, inner.top, inner.right, inner.bottom, 6, 6);
        SelectObject(hdc, old_br); SelectObject(hdc, old_pen);
        DeleteObject(cb); DeleteObject(pen);

        if (id == ID_COLOR7) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, TEXT_C);
            DrawTextA(hdc, "+", 1, &inner, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        if (di.itemState & ODS_SELECTED) {
            HPEN fp = CreatePen(PS_SOLID, 1, ACCENT);
            old_pen = SelectObject(hdc, fp);
            old_br = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, r.left, r.top, r.right, r.bottom);
            SelectObject(hdc, old_br); SelectObject(hdc, old_pen);
            DeleteObject(fp);
        }
        return;
    }

    // Preview area
    if (id == ID_PREVIEW) {
        HDC hdc = di.hDC;
        RECT r = di.rcItem;

        FillRect(hdc, &r, g_prev_brush);

        // Use an offscreen bitmap to apply per-layer alpha
        int pw = r.right - r.left, ph = r.bottom - r.top;
        Gdiplus::Bitmap offscreen(pw, ph, PixelFormat32bppARGB);
        Gdiplus::Graphics offg(&offscreen);
        offg.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        RenderContext ctx; ctx.gfx = &offg;

        float cx = pw / 2.0f, cy = ph / 2.0f;

        auto draw_layer_preview = [&](const LayerCfg& layer) {
            if (!layer.visible) return;
            Gdiplus::ColorMatrix cm = {};
            cm.m[0][0] = 1; cm.m[1][1] = 1; cm.m[2][2] = 1;
            cm.m[3][3] = layer.alpha; cm.m[4][4] = 1;
            Gdiplus::ImageAttributes ia;
            ia.SetColorMatrix(&cm);

            if (layer.style == "image" && !layer.image_path.empty()) {
                auto* bmp = load_crosshair_image(layer.image_path, layer.size, layer.angle);
                if (bmp) {
                    offg.DrawImage(bmp,
                        Gdiplus::RectF(cx - bmp->GetWidth()/2.0f, cy - bmp->GetHeight()/2.0f,
                                       (float)bmp->GetWidth(), (float)bmp->GetHeight()),
                        0, 0, (float)bmp->GetWidth(), (float)bmp->GetHeight(),
                        Gdiplus::UnitPixel, &ia);
                    delete bmp;
                }
            } else {
                // Draw vector crosshair offscreen, then blit with alpha
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
            for (int i = 0; i < 3; i++)
                draw_layer_preview(m_cfg->layers[i]);
        } else {
            draw_layer_preview(m_cfg->active());
        }

        // Blit offscreen to preview DC
        Gdiplus::Graphics gfx(hdc);
        gfx.DrawImage(&offscreen, r.left, r.top, pw, ph);
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

// ---- slider helpers ----
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

    const char* st_names[] = {"cross","circle","dot","crosshair","corner","triangle","image","hollow"};
    int si = 0;
    for (int i = 0; i < 8; i++)
        if (layer.style == st_names[i]) { si = i; break; }
    for (int i = 0; i < 8; i++)
        SendMessage(m_style_radios[i], BM_SETCHECK, i == si ? BST_CHECKED : BST_UNCHECKED, 0);

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
        SetWindowTextW(m_img_label, L"未选择图片");
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
        if (m_layer_btns[i]) InvalidateRect(m_layer_btns[i], nullptr, TRUE);
    }
}

void SettingsWindow::update_layer_btns() {
    bool ml = m_cfg->multi_layer;
    for (int i = 0; i < 3; i++) {
        ShowWindow(m_layer_eyes[i], ml ? SW_SHOW : SW_HIDE);
        ShowWindow(m_layer_btns[i], ml ? SW_SHOW : SW_HIDE);
    }
    update_preview();
}

void SettingsWindow::sync_active_layer() {
    m_active_layer = m_cfg->active_layer;
}

// ---- command handling ----
void SettingsWindow::on_command(WORD id, WORD notify) {
    auto& cfg = *m_cfg;

    if (id == ID_ML_CHECK) {
        cfg.multi_layer = (SendMessage(m_ml_check, BM_GETCHECK, 0, 0) == BST_CHECKED);
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
        if (notify == BN_CLICKED && SendMessage(m_style_radios[id - ID_STYLE0], BM_GETCHECK, 0, 0) == BST_CHECKED) {
            const char* names[] = {"cross","circle","dot","crosshair","corner","triangle","image","hollow"};
            cfg.active().style = names[id - ID_STYLE0];
            update_preview();
            update_overlay();
        }
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
            const char* cols[] = {"#00FF00","#FF4444","#00D4FF","#FF44FF","#FFDD44","#FFFFFF","#FF8800"};
            cfg.active().color = cols[id - ID_COLOR0];
        }
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
        // Toggle active layer's visibility
        toggle_layer_eye(m_cfg->active_layer);
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
    update_overlay();
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
        int mins[] = {1,1,0,0}, maxs[] = {500,30,100,360};
        if (val < mins[idx]) val = mins[idx];
        if (val > maxs[idx]) val = maxs[idx];
        if (idx == SL_SIZE)  m_cfg->active().size = val;
        else if (idx == SL_THICK) m_cfg->active().thickness = val;
        else if (idx == SL_GAP)   m_cfg->active().gap = val;
        else if (idx == SL_ANGLE) m_cfg->active().angle = val;
        slider_set(m_sliders[idx], val);
    }

    update_preview();
    update_overlay();
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
        std::string name = (pos != std::string::npos) ? path.substr(pos+1) : path;
        std::string dest = AppCfg::img_dir() + "\\" + name;
        DeleteFileW(utf8_to_wstr(dest).c_str());
        CopyFileW(buf, utf8_to_wstr(dest).c_str(), FALSE);
        m_cfg->active().image_path = dest;
        m_cfg->active().style = "image";
        m_cfg->active().visible = true;
        // Auto-size: ~100px max dimension initially (100 = 1x scale)
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
        // Auto-enable multi-layer if on layer 2/3
        if (m_cfg->active_layer > 0) {
            m_cfg->multi_layer = true;
            SendMessage(m_ml_check, BM_SETCHECK, BST_CHECKED, 0);
            update_layer_btns();
        }
        refresh_ui();
        update_preview();
        update_overlay();
        m_cfg->save();
    }
}

void SettingsWindow::save_preset() {
    // Register dialog class with dark theme
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

    int dlg_w = 260, dlg_h = 105;
    HWND dlg = CreateWindowExW(WS_EX_TOPMOST, L"CrosshairSaveDlg", L"保存预设",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        0, 0, dlg_w, dlg_h, m_hwnd, nullptr, m_hinst, nullptr);

    // Center on parent
    RECT pr; GetWindowRect(m_hwnd, &pr);
    RECT dr = {0, 0, dlg_w, dlg_h};
    AdjustWindowRect(&dr, WS_CAPTION, FALSE);
    SetWindowPos(dlg, nullptr,
        pr.left + (pr.right - pr.left - (dr.right - dr.left)) / 2,
        pr.top + (pr.bottom - pr.top - (dr.bottom - dr.top)) / 2,
        dr.right - dr.left, dr.bottom - dr.top, SWP_NOZORDER);

    // Label
    HWND lb = CreateWindowW(L"STATIC", L"预设名称:",
        WS_CHILD | WS_VISIBLE, 12, 14, 80, 22, dlg, nullptr, m_hinst, nullptr);
    SendMessage(lb, WM_SETFONT, (WPARAM)g_font, TRUE);

    // Edit box
    HWND ed = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        90, 12, 155, 24, dlg, (HMENU)100, m_hinst, nullptr);
    SendMessage(ed, WM_SETFONT, (WPARAM)g_font, TRUE);
    SetWindowTheme(ed, L"DarkMode_Explorer", nullptr);
    SetFocus(ed);

    // Owner-draw buttons matching main UI style
    CreateWindowW(L"BUTTON", L"确定",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        65, 48, 75, 28, dlg, (HMENU)1, m_hinst, nullptr);
    CreateWindowW(L"BUTTON", L"取消",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        150, 48, 75, 28, dlg, (HMENU)2, m_hinst, nullptr);

    // Shared state: result flag + name buffer, passed via GWLP_USERDATA
    struct DlgData {
        bool done = false;
        wchar_t name[64] = {};
    };
    DlgData data{};

    // Subclass for dark theme, owner-draw, and dismiss
    SetWindowSubclass(dlg, [](HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref) -> LRESULT {
        DlgData* d = (DlgData*)ref;

        if (msg == WM_CTLCOLORSTATIC) {
            SetBkColor((HDC)wp, BG);
            SetTextColor((HDC)wp, TEXT_C);
            return (LRESULT)g_bg_brush;
        }
        if (msg == WM_CTLCOLOREDIT) {
            SetBkColor((HDC)wp, PREV);
            SetTextColor((HDC)wp, TEXT_C);
            return (LRESULT)g_prev_brush;
        }
        if (msg == WM_ERASEBKGND) {
            RECT r; GetClientRect(h, &r);
            FillRect((HDC)wp, &r, g_bg_brush);
            return 1;
        }
        if (msg == WM_DRAWITEM) {
            DRAWITEMSTRUCT& di = *(DRAWITEMSTRUCT*)lp;
            HDC hdc = di.hDC;
            RECT r = di.rcItem;
            bool pressed = (di.itemState & ODS_SELECTED) != 0;
            COLORREF bg = pressed ? ACCENT : CARD;
            HBRUSH fill = CreateSolidBrush(bg);
            HPEN pen = CreatePen(PS_SOLID, 1, RING);
            SelectObject(hdc, fill); SelectObject(hdc, pen);
            RoundRect(hdc, r.left, r.top, r.right, r.bottom, 6, 6);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            SelectObject(hdc, GetStockObject(BLACK_PEN));
            DeleteObject(fill); DeleteObject(pen);
            wchar_t text[64];
            GetWindowTextW(di.hwndItem, text, 64);
            SelectObject(hdc, g_font);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, TEXT_C);
            DrawTextW(hdc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return 0;
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
        // Handle close: X button, Alt+F4, etc.
        if (msg == WM_CLOSE || (msg == WM_SYSCOMMAND && (wp & 0xFFF0) == SC_CLOSE)) {
            d->name[0] = 0;
            d->done = true;
            return 0;
        }
        return DefSubclassProc(h, msg, wp, lp);
    }, 0, (DWORD_PTR)&data);

    // Non-modal loop: pump messages until dialog dismisses itself
    while (!data.done) {
        MSG m;
        if (GetMessage(&m, nullptr, 0, 0)) {
            if (!IsDialogMessage(dlg, &m)) {
                TranslateMessage(&m);
                DispatchMessage(&m);
            }
        }
    }

    // If confirmed with a non-empty name, save preset
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

    SendMessage(m_ml_check, BM_SETCHECK, m_cfg->multi_layer ? BST_CHECKED : BST_UNCHECKED, 0);
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
    // Set defaults matching Python version for layers 1/2
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
    m_cfg->layers[idx].visible = !m_cfg->layers[idx].visible;
    // Also switch to this layer if in multi-layer mode
    if (m_cfg->multi_layer) {
        m_cfg->active_layer = idx;
        sync_active_layer();
    }
    refresh_ui();
    update_preview();
    update_overlay();
    m_cfg->save();
}

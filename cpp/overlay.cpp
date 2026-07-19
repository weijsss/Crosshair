#include "overlay.h"
#include "render.h"
#include <algorithm>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

static const wchar_t* OVERLAY_CLASS = L"CrosshairOverlay";

void OverlayManager::create(HWND parent, int sw, int sh) {
    m_sw = sw; m_sh = sh;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = OVERLAY_CLASS;
    RegisterClassExW(&wc);

    for (int i = 0; i < 3; i++) {
        m_hwnds[i] = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            OVERLAY_CLASS, L"", WS_POPUP,
            0, 0, 1, 1, parent, nullptr, wc.hInstance, nullptr);
    }
}

void OverlayManager::destroy() {
    release_resources();
    for (int i = 0; i < 3; i++) {
        free_dib(i);
        if (m_hwnds[i]) { DestroyWindow(m_hwnds[i]); m_hwnds[i] = nullptr; }
    }
}

void OverlayManager::release_resources() {
    for (int i = 0; i < 3; i++) {
        if (m_img[i]) { delete m_img[i]; m_img[i] = nullptr; }
        m_path[i].clear(); m_sz[i] = m_ang[i] = 0;
    }
}

void OverlayManager::set_force_topmost(bool v) {
    for (int i = 0; i < 3; i++) {
        if (!m_hwnds[i]) continue;
        SetWindowPos(m_hwnds[i], v ? HWND_TOPMOST : HWND_NOTOPMOST,
            0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void OverlayManager::free_dib(int idx) {
    if (m_dc[idx]) { DeleteDC(m_dc[idx]); m_dc[idx] = nullptr; }
    if (m_dib[idx]) { DeleteObject(m_dib[idx]); m_dib[idx] = nullptr; }
    m_bits[idx] = nullptr; m_w[idx] = m_h[idx] = 0;
}

void OverlayManager::ensure_dib(int idx, int w, int h) {
    w = (std::max)(64, w); h = (std::max)(64, h);
    // Quantize to 64px steps so slider drags don't reallocate every frame
    w = (w + 63) & ~63;
    h = (h + 63) & ~63;
    if (w == m_w[idx] && h == m_h[idx]) return;

    free_dib(idx);

    HDC screen_dc = GetDC(nullptr);
    m_dc[idx] = CreateCompatibleDC(screen_dc);

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    m_dib[idx] = CreateDIBSection(m_dc[idx], &bi, DIB_RGB_COLORS, &m_bits[idx], nullptr, 0);
    SelectObject(m_dc[idx], m_dib[idx]);
    ReleaseDC(nullptr, screen_dc);

    m_w[idx] = w; m_h[idx] = h;
}

Gdiplus::Bitmap* OverlayManager::get_image(int idx, const LayerCfg& layer) {
    if (m_img[idx] && m_path[idx] == layer.image_path
        && m_sz[idx] == layer.size && m_ang[idx] == layer.angle) {
        return m_img[idx];
    }
    delete m_img[idx]; m_img[idx] = nullptr; m_path[idx].clear();

    if (!layer.image_path.empty()) {
        m_img[idx] = load_crosshair_image(layer.image_path, layer.size, layer.angle);
        if (m_img[idx]) {
            m_path[idx] = layer.image_path;
            m_sz[idx] = layer.size; m_ang[idx] = layer.angle;
        }
    }
    return m_img[idx];
}

void OverlayManager::draw_layer(int idx, const LayerCfg& layer) {
    if (!m_bits[idx]) return;

    int w = m_w[idx], h = m_h[idx];
    ZeroMemory(m_bits[idx], w * h * 4);

    Gdiplus::Graphics gfx(m_dc[idx]);
    gfx.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

    if (!layer.visible) return;

    float cx = (float)w / 2, cy = (float)h / 2;

    if (layer.style == "image" && !layer.image_path.empty()) {
        auto* bmp = get_image(idx, layer);
        if (bmp) {
            gfx.DrawImage(bmp, (INT)(cx - bmp->GetWidth() / 2),
                         (INT)(cy - bmp->GetHeight() / 2),
                         (INT)bmp->GetWidth(), (INT)bmp->GetHeight());
        }
    } else {
        RenderContext ctx; ctx.gfx = &gfx;
        ctx.bounds = Gdiplus::RectF(0, 0, (float)w, (float)h);
        draw_crosshair(ctx, layer, cx, cy);
    }
    GdiFlush();
}

void OverlayManager::update(const AppCfg& cfg) {
    HDC screen_dc = GetDC(nullptr);

    for (int i = 0; i < 3; i++) {
        if (!m_hwnds[i]) continue;

        bool show = cfg.overlay_visible &&
            (cfg.multi_layer ? cfg.layers[i].visible : (i == 0 && cfg.layers[i].visible));
        BYTE alpha_val = show ? (BYTE)(cfg.layers[i].alpha * 255.0) : 0;

        if (alpha_val == 0) {
            ShowWindow(m_hwnds[i], SW_HIDE);
            free_dib(i);  // release memory for hidden layers
            continue;
        }

        auto& layer = cfg.layers[i];

        // Size DIB to actual content extent, not a fixed 1024
        int tw, th;
        if (layer.style == "image" && !layer.image_path.empty()) {
            auto* bmp = get_image(i, layer);
            if (bmp) {
                tw = bmp->GetWidth();
                th = bmp->GetHeight();
            } else {
                tw = th = 64;
            }
            if (tw > m_sw) tw = m_sw;
            if (th > m_sh) th = m_sh;
        } else {
            // extent = (size + gap + thickness) * sqrt(2) for rotation, + margin
            float ext = (layer.size + layer.gap + layer.thickness) * 1.5f + 16.0f;
            tw = th = (int)ext;
        }

        ensure_dib(i, tw, th);
        tw = m_w[i]; th = m_h[i];  // use actual (quantized) size
        draw_layer(i, layer);

        int ox = layer.offset_x, oy = layer.offset_y;
        int wx = m_sw / 2 + ox - tw / 2;
        int wy = m_sh / 2 + oy - th / 2;

        ShowWindow(m_hwnds[i], SW_SHOWNOACTIVATE);
        SetWindowPos(m_hwnds[i], nullptr, wx, wy, tw, th,
            SWP_NOZORDER | SWP_NOACTIVATE);

        BLENDFUNCTION blend = {};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = alpha_val;
        blend.AlphaFormat = AC_SRC_ALPHA;

        POINT pt = {0, 0};
        SIZE size = {tw, th};
        UpdateLayeredWindow(m_hwnds[i], screen_dc, nullptr, &size,
                           m_dc[i], &pt, 0, &blend, ULW_ALPHA);
    }
    ReleaseDC(nullptr, screen_dc);
}

LRESULT CALLBACK OverlayManager::wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return DefWindowProcW(hwnd, msg, wp, lp);
}

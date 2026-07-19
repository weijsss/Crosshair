#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include "config.h"

class OverlayManager {
public:
    void create(HWND parent, int sw, int sh);
    void update(const AppCfg& cfg);
    void destroy();
    void set_force_topmost(bool v);

private:
    HWND m_hwnds[3] = {};
    int m_sw = 0, m_sh = 0;

    // Per-layer DIB (dynamic size)
    HBITMAP m_dib[3] = {};
    HDC     m_dc[3] = {};
    void*   m_bits[3] = {};
    int     m_w[3] = {}, m_h[3] = {};

    // Image cache
    Gdiplus::Bitmap* m_img[3] = {};
    std::string      m_path[3];
    int              m_sz[3] = {}, m_ang[3] = {};

    void ensure_dib(int idx, int w, int h);
    void free_dib(int idx);
    void draw_layer(int idx, const LayerCfg& cfg);
    Gdiplus::Bitmap* get_image(int idx, const LayerCfg& layer);
    void release_resources();

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};

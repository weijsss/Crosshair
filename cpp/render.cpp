#include "render.h"
#include <cmath>
#include <algorithm>
#include <utility>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Gdiplus::Color hex_to_color(const std::string& hex) {
    unsigned int r = 0, g = 0, b = 0;
    if (hex.size() >= 7 && hex[0] == '#') {
        r = strtoul(hex.substr(1, 2).c_str(), nullptr, 16);
        g = strtoul(hex.substr(3, 2).c_str(), nullptr, 16);
        b = strtoul(hex.substr(5, 2).c_str(), nullptr, 16);
    }
    return Gdiplus::Color(255, (BYTE)r, (BYTE)g, (BYTE)b);
}

static void rot(float& x, float& y, float cx, float cy, float angle_deg) {
    if (angle_deg == 0) return;
    float rad = angle_deg * (float)M_PI / 180.0f;
    float s = sinf(rad), c_ = cosf(rad);
    float dx = x - cx, dy = y - cy;
    x = cx + dx * c_ - dy * s;
    y = cy + dx * s + dy * c_;
}

float image_scale(int size) { 
    // size 100 = 1:1, size 200 = 2x, etc.
    return size / 100.0f; 
}

Gdiplus::Bitmap* load_crosshair_image(const std::string& path, int size, int angle) {
    if (path.empty()) return nullptr;
    auto* bmp = Gdiplus::Bitmap::FromFile(utf8_to_wstr(path).c_str());
    if (!bmp || bmp->GetLastStatus() != Gdiplus::Ok) { delete bmp; return nullptr; }

    float scale = image_scale(size);
    int nw = std::max(1, (int)(bmp->GetWidth() * scale));
    int nh = std::max(1, (int)(bmp->GetHeight() * scale));
    // Cap at 2048 to prevent memory explosion
    const float MAX_DIM = 2048.0f;
    if (nw > MAX_DIM || nh > MAX_DIM) {
        float adj = MAX_DIM / (std::max)((float)nw, (float)nh);
        nw = (int)(nw * adj); nh = (int)(nh * adj);
        scale *= adj;
    }

    if (angle != 0) {
        auto* rb = new Gdiplus::Bitmap(nw, nh, PixelFormat32bppARGB);
        Gdiplus::Graphics g(rb);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.TranslateTransform((float)nw / 2, (float)nh / 2);
        g.RotateTransform((float)angle);
        g.ScaleTransform(scale, scale);
        g.DrawImage(bmp, Gdiplus::REAL(-bmp->GetWidth() / 2), Gdiplus::REAL(-bmp->GetHeight() / 2),
                    (Gdiplus::REAL)bmp->GetWidth(), (Gdiplus::REAL)bmp->GetHeight());
        delete bmp;
        return rb;
    }

    auto* rb = new Gdiplus::Bitmap(nw, nh, PixelFormat32bppARGB);
    Gdiplus::Graphics g(rb);
    g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    g.ScaleTransform(scale, scale);
    g.DrawImage(bmp, 0, 0, (INT)bmp->GetWidth(), (INT)bmp->GetHeight());
    delete bmp;
    return rb;
}

void draw_crosshair(RenderContext& ctx, const LayerCfg& cfg, float cx, float cy) {
    if (!ctx.gfx) return;
    auto color = hex_to_color(cfg.color);
    Gdiplus::Pen pen(color, (float)cfg.thickness);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    Gdiplus::SolidBrush brush(color);

    float s = (float)cfg.size;
    float g = (float)cfg.gap;
    float a = (float)cfg.angle;
    auto& style = cfg.style;

    auto line = [&](float x1, float y1, float x2, float y2) {
        rot(x1, y1, cx, cy, a); rot(x2, y2, cx, cy, a);
        ctx.gfx->DrawLine(&pen, x1, y1, x2, y2);
    };

    if (style == "cross") {
        line(cx, cy - g, cx, cy - g - s);
        line(cx, cy + g, cx, cy + g + s);
        line(cx - g, cy, cx - g - s, cy);
        line(cx + g, cy, cx + g + s, cy);
        float r = std::max(1.0f, (float)cfg.thickness / 2);
        ctx.gfx->FillEllipse(&brush, cx - r, cy - r, r * 2, r * 2);
        ctx.gfx->DrawEllipse(&pen, cx - r, cy - r, r * 2, r * 2);
    } else if (style == "circle") {
        ctx.gfx->DrawEllipse(&pen, cx - s, cy - s, s * 2, s * 2);
        ctx.gfx->FillEllipse(&brush, cx - 1.0f, cy - 1.0f, 2.0f, 2.0f);
    } else if (style == "dot") {
        ctx.gfx->FillEllipse(&brush, cx - s, cy - s, s * 2, s * 2);
    } else if (style == "crosshair") {
        line(cx, cy - s, cx, cy + s);
        line(cx - s, cy, cx + s, cy);
    } else if (style == "corner") {
        int dirs[4][2] = {{-1,-1},{1,-1},{-1,1},{1,1}};
        for (auto& d : dirs) {
            float dx = (float)d[0], dy = (float)d[1];
            float bx = cx + dx * g, by = cy + dy * g;
            float y1 = by - s * (dy == -1.0f ? 1.0f : 0), y2 = by + s * (dy == 1.0f ? 1.0f : 0);
            line(bx, y1, bx, y2);
            float x1 = bx - s * (dx == -1.0f ? 1.0f : 0), x2 = bx + s * (dx == 1.0f ? 1.0f : 0);
            line(x1, by, x2, by);
        }
    } else if (style == "triangle") {
        float hh = s * 1.6f, hw = s * 1.3f;
        Gdiplus::PointF pts[] = {{cx, cy - hh}, {cx - hw, cy + hh / 2}, {cx + hw, cy + hh / 2}};
        for (auto& p : pts) rot(p.X, p.Y, cx, cy, a);
        ctx.gfx->DrawPolygon(&pen, pts, 3);
    } else if (style == "hollow") {
        // Hollow cross (plus sign)
        float w = s / 3.0f, h = s + g;
        float hw = w, hh = h;
        if (a != 0) {
            // Rotated: thick lines with flat caps so size matches the rect version
            Gdiplus::Pen thickPen(color, s / 1.5f);
            thickPen.SetStartCap(Gdiplus::LineCapFlat);
            thickPen.SetEndCap(Gdiplus::LineCapFlat);
            float x1 = cx, y1 = cy - h, x2 = cx, y2 = cy + h;
            rot(x1, y1, cx, cy, a); rot(x2, y2, cx, cy, a);
            ctx.gfx->DrawLine(&thickPen, x1, y1, x2, y2);
            x1 = cx - h; y1 = cy; x2 = cx + h; y2 = cy;
            rot(x1, y1, cx, cy, a); rot(x2, y2, cx, cy, a);
            ctx.gfx->DrawLine(&thickPen, x1, y1, x2, y2);
        } else {
            ctx.gfx->FillRectangle(&brush, cx - hw, cy - hh, hw * 2, hh * 2);
            ctx.gfx->FillRectangle(&brush, cx - hh, cy - hw, hh * 2, hw * 2);
        }
    }
    // "image" style handled separately in overlay/settings drawing
}

#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include "config.h"

#pragma comment(lib, "gdiplus.lib")

struct RenderContext {
    Gdiplus::Graphics* gfx = nullptr;
    Gdiplus::RectF bounds;
};

// Parse hex color like "#FF4444" to GDI+ Color
Gdiplus::Color hex_to_color(const std::string& hex);

// Draw a crosshair shape centered at (cx, cy)
void draw_crosshair(RenderContext& ctx, const LayerCfg& cfg, float cx, float cy);

// Load an image from file, optionally rotate
Gdiplus::Bitmap* load_crosshair_image(const std::string& path, int size, int angle);

// Get appropriate image scale from size parameter
float image_scale(int size);

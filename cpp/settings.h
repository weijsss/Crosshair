#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <string>
#include "config.h"
#include "overlay.h"

#pragma comment(lib, "comctl32.lib")

class SettingsWindow {
public:
    void create(HINSTANCE hinst, AppCfg& cfg, OverlayManager& overlay);
    void show();
    void hide();
    HWND hwnd() const { return m_hwnd; }
    void refresh_ui();
    void update_preview();
    void update_overlay();
    void update_layer_btns();
    void sync_active_layer();
    void reset_layer_by_idx(int idx);
    void toggle_layer_eye(int idx);

private:
    HWND m_hwnd = nullptr;
    HINSTANCE m_hinst = nullptr;
    AppCfg* m_cfg = nullptr;
    OverlayManager* m_overlay = nullptr;

    HWND m_ml_toggle = nullptr;
    HWND m_layer_btns[3] = {};
    HWND m_layer_eyes[3] = {};
    HWND m_style_btns[8] = {};
    HWND m_img_label = nullptr;
    HWND m_color_btns[8] = {};
    HWND m_sliders[7] = {};
    HWND m_slider_edits[7] = {};
    HWND m_preset_list = nullptr;
    HWND m_preview_area = nullptr;

    int m_active_layer = 0;
    int m_window_w = 400, m_window_h = 898;
    bool m_sync = false;    // guard: skip edit-change handling during programmatic sync
    bool m_in_tray = false;

    void to_tray();
    void from_tray();
    void tray_cleanup();
    void schedule_update();

    static constexpr int SL_SIZE = 0, SL_THICK = 1, SL_GAP = 2, SL_ANGLE = 3, SL_ALPHA = 4;
    static constexpr int SL_OFFX = 5, SL_OFFY = 6;

    void on_command(WORD id, WORD notify);
    void on_hscroll(WORD code, int id);
    void on_slider_edit(int idx);
    void on_drawitem(WPARAM wp, LPARAM lp);
    void on_dropfile(HDROP drop);

    void pick_image();
    void save_preset();
    void load_preset();
    void delete_preset();
    void reset_layer();

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static SettingsWindow* get(HWND hwnd);
};

enum {
    ID_ML_CHECK = 1001,
    ID_LAYER_BTN0 = 1010, ID_LAYER_BTN1 = 1011, ID_LAYER_BTN2 = 1012,
    ID_LAYER_EYE0 = 1015, ID_LAYER_EYE1 = 1016, ID_LAYER_EYE2 = 1017,
    ID_STYLE0 = 1020,
    ID_COLOR0 = 1030, ID_COLOR7 = 1037,
    ID_SLIDER0 = 1040,
    ID_SLIDER_EDIT0 = 1050,
    ID_PICK_IMG = 1060,
    ID_SAVE_PRESET = 1070, ID_LOAD_PRESET = 1071, ID_DEL_PRESET = 1072, ID_RESET = 1073,
    ID_PRESET_LIST = 1080,
    ID_TOGGLE = 1090, ID_APPLY = 1091, ID_HIDE = 1092,
    ID_PREVIEW = 1100,
    ID_BILIBILI = 1110, ID_GITHUB = 1111,
};

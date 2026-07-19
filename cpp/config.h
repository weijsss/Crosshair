#pragma once
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <fstream>
#include <cstdlib>
#include <windows.h>

// ---- minimal JSON value ----
struct JsonVal;
using JsonObj = std::map<std::string, JsonVal>;
using JsonArr = std::vector<JsonVal>;
using JsonVar = std::variant<std::monostate, bool, double, std::string, JsonArr, JsonObj>;
struct JsonVal {
    JsonVar v;
    JsonVal() : v(std::monostate{}) {}
    JsonVal(bool b) : v(b) {}
    JsonVal(double n) : v(n) {}
    JsonVal(int n) : v(double(n)) {}
    JsonVal(const char* s) : v(std::string(s)) {}
    JsonVal(std::string s) : v(std::move(s)) {}
    JsonVal(JsonArr a) : v(std::move(a)) {}
    JsonVal(JsonObj o) : v(std::move(o)) {}

    bool is_null()  const { return std::holds_alternative<std::monostate>(v); }
    bool is_bool()  const { return std::holds_alternative<bool>(v); }
    bool is_num()   const { return std::holds_alternative<double>(v); }
    bool is_str()   const { return std::holds_alternative<std::string>(v); }
    bool is_arr()   const { return std::holds_alternative<JsonArr>(v); }
    bool is_obj()   const { return std::holds_alternative<JsonObj>(v); }

    bool     as_bool()  const { return std::get<bool>(v); }
    double   as_num()   const { return std::get<double>(v); }
    int      as_int()   const { return (int)std::get<double>(v); }
    std::string as_str()const { return std::get<std::string>(v); }
    JsonArr& as_arr()        { return std::get<JsonArr>(v); }
    JsonObj& as_obj()        { return std::get<JsonObj>(v); }
    const JsonArr& as_arr()  const { return std::get<JsonArr>(v); }
    const JsonObj& as_obj()  const { return std::get<JsonObj>(v); }

    JsonVal& operator[](const char* k) { return as_obj()[k]; }
    JsonVal& operator[](size_t i)      { return as_arr()[i]; }
    const JsonVal& operator[](const char* k) const { return as_obj().at(k); }
    const JsonVal& operator[](size_t i)      const { return as_arr()[i]; }

    bool contains(const char* k) const { return is_obj() && as_obj().count(k); }
};

JsonVal json_parse(const std::string& src);
std::string json_dump(const JsonVal& v, int indent = 0);

// ---- config ----
struct LayerCfg {
    std::string color = "#00FF00";
    double alpha = 0.85;
    int size = 20, thickness = 2, gap = 4;
    std::string style = "cross";
    bool visible = true;
    int offset_x = 0, offset_y = 0;
    std::string image_path;
    int angle = 0;
    LayerCfg() = default;
    static LayerCfg from_json(const JsonVal& j);
    JsonVal to_json() const;
};

struct AppCfg {
    bool multi_layer = false;
    int active_layer = 0;
    LayerCfg layers[3];
    bool force_topmost = false;
    bool overlay_visible = true;  // master show/hide for on-screen crosshair only
    JsonObj presets;

    void load();
    void save();
    LayerCfg& active() { return layers[active_layer]; }

    static std::string dir();
    static std::string img_dir();
private:
    static std::string cfg_path();
};

std::wstring utf8_to_wstr(const std::string& s);
std::string wstr_to_utf8(const std::wstring& ws);

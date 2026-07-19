#include "config.h"
#include <shlobj.h>
#include <codecvt>
#include <locale>
#include <sstream>

// ---- JSON parser ----
static void skip_ws(const char*& p) { while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++; }
static std::string parse_str(const char*& p) {
    if (*p++ != '"') return "";
    std::string r;
    while (*p && *p != '"') {
        if (*p == '\\') { p++; if (*p) r += *p++; }
        else r += *p++;
    }
    if (*p) p++;
    return r;
}
static JsonVal parse_val(const char*& p);
static JsonArr parse_arr(const char*& p) {
    JsonArr a;
    if (*p != '[') return a; p++;
    skip_ws(p);
    if (*p != ']') {
        a.push_back(parse_val(p));
        skip_ws(p);
        while (*p == ',') {
            p++; skip_ws(p);
            a.push_back(parse_val(p));
            skip_ws(p);
        }
    }
    if (*p == ']') p++;
    return a;
}
static JsonObj parse_obj(const char*& p) {
    JsonObj o;
    if (*p != '{') return o; p++;
    skip_ws(p);
    if (*p != '}') {
        std::string k = parse_str(p);
        skip_ws(p);
        if (*p == ':') p++;
        skip_ws(p);
        o[k] = parse_val(p);
        skip_ws(p);
        while (*p == ',') {
            p++; skip_ws(p);
            k = parse_str(p);
            skip_ws(p);
            if (*p == ':') p++;
            skip_ws(p);
            o[k] = parse_val(p);
            skip_ws(p);
        }
    }
    if (*p == '}') p++;
    return o;
}
static JsonVal parse_val(const char*& p) {
    skip_ws(p);
    if (*p == '"') return JsonVal(parse_str(p));
    if (*p == '{') return JsonVal(parse_obj(p));
    if (*p == '[') return JsonVal(parse_arr(p));
    std::string lit;
    while (*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\n' && *p != '\r') lit += *p++;
    if (lit == "true")  return JsonVal(true);
    if (lit == "false") return JsonVal(false);
    if (lit == "null")  return JsonVal();
    char* end;
    double n = strtod(lit.c_str(), &end);
    if (end != lit.c_str() && *end == 0) return JsonVal(n);
    return JsonVal();
}
JsonVal json_parse(const std::string& src) {
    const char* p = src.c_str();
    return parse_val(p);
}

// ---- JSON writer ----
static std::string esc(const std::string& s) {
    std::string r = "\"";
    for (char c : s) { if (c == '"' || c == '\\') r += '\\'; r += c; }
    return r + "\"";
}
std::string json_dump(const JsonVal& v, int indent) {
    std::string pad(indent, ' '), pad2(indent + 2, ' '), r;
    if (v.is_null()) return "null";
    if (v.is_bool()) return v.as_bool() ? "true" : "false";
    if (v.is_num()) { char b[64]; snprintf(b, sizeof(b), "%.15g", v.as_num()); return b; }
    if (v.is_str()) return esc(v.as_str());
    if (v.is_arr()) {
        r = "[\n";
        auto& a = v.as_arr();
        for (size_t i = 0; i < a.size(); i++) {
            r += pad2 + json_dump(a[i], indent + 2);
            if (i + 1 < a.size()) r += ",";
            r += "\n";
        }
        return r + pad + "]";
    }
    if (v.is_obj()) {
        r = "{\n";
        auto& o = v.as_obj();
        size_t i = 0;
        for (auto& kv : o) {
            r += pad2 + esc(kv.first) + ": " + json_dump(kv.second, indent + 2);
            if (++i < o.size()) r += ",";
            r += "\n";
        }
        return r + pad + "}";
    }
    return "null";
}

// ---- LayerCfg ----
LayerCfg LayerCfg::from_json(const JsonVal& j) {
    LayerCfg c;
    if (j.contains("color"))       c.color       = j["color"].as_str();
    if (j.contains("alpha"))       c.alpha       = j["alpha"].as_num();
    if (j.contains("size"))        c.size        = j["size"].as_int();
    if (j.contains("thickness"))   c.thickness   = j["thickness"].as_int();
    if (j.contains("gap"))         c.gap         = j["gap"].as_int();
    if (j.contains("style"))       c.style       = j["style"].as_str();
    if (j.contains("visible"))     c.visible     = j["visible"].as_bool();
    if (j.contains("offset_x"))    c.offset_x    = j["offset_x"].as_int();
    if (j.contains("offset_y"))    c.offset_y    = j["offset_y"].as_int();
    if (j.contains("image_path"))  c.image_path  = j["image_path"].as_str();
    if (j.contains("angle"))       c.angle       = j["angle"].as_int();
    return c;
}
JsonVal LayerCfg::to_json() const {
    JsonObj o;
    o["color"] = color; o["alpha"] = alpha; o["size"] = double(size);
    o["thickness"] = double(thickness); o["gap"] = double(gap); o["style"] = style;
    o["visible"] = visible; o["offset_x"] = double(offset_x); o["offset_y"] = double(offset_y);
    o["image_path"] = image_path; o["angle"] = double(angle);
    return o;
}

// ---- AppCfg ----
std::string AppCfg::dir() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::string d = wstr_to_utf8(path) + "\\Crosshair";
        CreateDirectoryA(d.c_str(), NULL);
        return d;
    }
    return ".";
}
std::string AppCfg::img_dir() {
    std::string d = dir() + "\\images";
    CreateDirectoryA(d.c_str(), NULL);
    return d;
}
std::string AppCfg::cfg_path() { return dir() + "\\crosshair_config.json"; }

void AppCfg::load() {
    std::ifstream f(cfg_path());
    if (!f) {
        // defaults
        layers[0] = LayerCfg();
        layers[1] = LayerCfg(); layers[1].color = "#FF4444"; layers[1].size = 15; layers[1].style = "dot";
        layers[2] = LayerCfg(); layers[2].color = "#00D4FF"; layers[2].size = 10; layers[2].style = "circle"; layers[2].visible = false;
        return;
    }
    std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    try {
        JsonVal j = json_parse(src);
        if (j.contains("multi_layer")) multi_layer = j["multi_layer"].as_bool();
        if (j.contains("active_layer")) active_layer = j["active_layer"].as_int();
        if (j.contains("force_topmost")) force_topmost = j["force_topmost"].as_bool();
        if (j.contains("overlay_visible")) overlay_visible = j["overlay_visible"].as_bool();
        if (j.contains("presets")) presets = j["presets"].as_obj(); // shallow copy
        if (j.contains("layers") && j["layers"].is_arr()) {
            auto& arr = j["layers"].as_arr();
            for (size_t i = 0; i < 3 && i < arr.size(); i++)
                layers[i] = LayerCfg::from_json(arr[i]);
        }
    } catch (...) {
        layers[0] = LayerCfg();
    }
}
void AppCfg::save() {
    JsonObj j;
    j["multi_layer"] = multi_layer;
    j["active_layer"] = double(active_layer);
    j["force_topmost"] = force_topmost;
    j["overlay_visible"] = overlay_visible;
    JsonArr la;
    for (int i = 0; i < 3; i++) la.push_back(layers[i].to_json());
    j["layers"] = la;
    j["presets"] = JsonObj(presets);
    std::ofstream f(cfg_path());
    f << json_dump(j, 2);
}

// ---- helpers ----
std::string wstr_to_utf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, NULL, 0, NULL, NULL);
    std::string r(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &r[0], len, NULL, NULL);
    r.pop_back();
    return r;
}
std::wstring utf8_to_wstr(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    std::wstring r(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &r[0], len);
    r.pop_back();
    return r;
}

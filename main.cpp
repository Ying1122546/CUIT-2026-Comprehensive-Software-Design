#define _CRT_SECURE_NO_WARNINGS
#pragma execution_character_set("utf-8")

#include <graphics.h>
#include <conio.h>
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <limits>

namespace fs = std::filesystem;
using namespace std;

// ====================== 基本配置 ======================
const wstring SOURCE_ROOT = L"D:\\自动气象站data\\自动气象站data";
const wstring PROJECT_ROOT = L".\\RZ2026";
const wstring DX24_ROOT = PROJECT_ROOT + L"\\dx24";
const wstring TREE_FILE = PROJECT_ROOT + L"\\tree.txt";
const wstring STATION_ID = L"58265";

const vector<wchar_t> TYPE_CHARS = { L'A', L'H', L'Z', L'P', L'T', L'U', L'W', L'R' };
const vector<wstring> TYPE_DIRS = {
    L"A58265", L"H58265", L"Z58265", L"P58265",
    L"T58265", L"U58265", L"W58265", L"R58265"
};

const int WIN_W = 1380;
const int WIN_H = 820;

// ====================== 主题配色 ======================
const COLORREF C_BG = RGB(241, 245, 249);
const COLORREF C_SIDEBAR = RGB(30, 41, 59);
const COLORREF C_SIDEBAR_LIGHT = RGB(51, 65, 85);
const COLORREF C_CARD = RGB(255, 255, 255);
const COLORREF C_CARD2 = RGB(248, 250, 252);
const COLORREF C_PRIMARY = RGB(37, 99, 235);
const COLORREF C_PRIMARY_DARK = RGB(29, 78, 216);
const COLORREF C_ACCENT = RGB(14, 165, 233);
const COLORREF C_SUCCESS = RGB(34, 197, 94);
const COLORREF C_DANGER = RGB(239, 68, 68);
const COLORREF C_TEXT = RGB(15, 23, 42);
const COLORREF C_TEXT2 = RGB(71, 85, 105);
const COLORREF C_BORDER = RGB(203, 213, 225);
const COLORREF C_LOG_BG = RGB(15, 23, 42);
const COLORREF C_LOG_TEXT = RGB(226, 232, 240);

// ====================== UI结构 ======================
struct Button {
    int id;
    int x1, y1, x2, y2;
    wstring text;
    COLORREF fillc;
    COLORREF textc;
};

enum class Page {
    MENU,
    QUERY,
    ANALYSIS,
    TREE,
    CHART
};

Page g_page = Page::MENU;
vector<Button> g_buttons;
vector<wstring> g_logs;
vector<int> g_completeYears;
vector<double> g_chartValues;
wstring g_chartTitle = L"";
wstring g_status = L"系统已启动";

int g_queryYear = 2010;
int g_queryMonth = 1;
int g_queryTypeIdx = 4;   // 默认 T

int g_anYear = 2010;
int g_anMonth = 1;
int g_anDay = 1;
int g_anHour = 0;
int g_anMinute = 0;

bool inRect(int x, int y, const Button& b) {
    return x >= b.x1 && x <= b.x2 && y >= b.y1 && y <= b.y2;
}

wstring twoDigit(int x) {
    wstringstream ss;
    ss << setw(2) << setfill(L'0') << x;
    return ss.str();
}

wstring threeDigit(int x) {
    wstringstream ss;
    ss << setw(3) << setfill(L'0') << x;
    return ss.str();
}

wstring toW(int x) {
    wstringstream ss;
    ss << x;
    return ss.str();
}

string trimAscii(const string& s) {
    size_t l = 0, r = s.size();
    while (l < r && (s[l] == ' ' || s[l] == '\r' || s[l] == '\n' || s[l] == '\t')) l++;
    while (r > l && (s[r - 1] == ' ' || s[r - 1] == '\r' || s[r - 1] == '\n' || s[r - 1] == '\t')) r--;
    return s.substr(l, r - l);
}

bool isLeapYear(int y) {
    return (y % 400 == 0) || (y % 4 == 0 && y % 100 != 0);
}

int daysInMonth(int y, int m) {
    static int mdays[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (m == 2) return isLeapYear(y) ? 29 : 28;
    return mdays[m - 1];
}

void clampAnalysisDate() {
    if (g_anMonth < 1) g_anMonth = 1;
    if (g_anMonth > 12) g_anMonth = 12;
    int dim = daysInMonth(g_anYear, g_anMonth);
    if (g_anDay < 1) g_anDay = 1;
    if (g_anDay > dim) g_anDay = dim;
    if (g_anHour < 0) g_anHour = 0;
    if (g_anHour > 23) g_anHour = 23;
    if (g_anMinute < 0) g_anMinute = 0;
    if (g_anMinute > 59) g_anMinute = 59;
}

void pushLog(const wstring& s) {
    g_logs.push_back(s);
    if (g_logs.size() > 20) g_logs.erase(g_logs.begin());
}

bool ensureDir(const fs::path& p) {
    try {
        if (!fs::exists(p)) fs::create_directories(p);
        return true;
    }
    catch (...) {
        return false;
    }
}

void initProjectDirs() {
    ensureDir(PROJECT_ROOT);
    ensureDir(DX24_ROOT);
    for (const auto& d : TYPE_DIRS) {
        ensureDir(fs::path(DX24_ROOT) / d);
    }
}

int typeIndexFromChar(wchar_t ch) {
    ch = towupper(ch);
    for (int i = 0; i < (int)TYPE_CHARS.size(); ++i) {
        if (TYPE_CHARS[i] == ch) return i;
    }
    return -1;
}

wstring buildFileName(wchar_t typeCh, int year, int month) {
    return wstring(1, typeCh) + STATION_ID + twoDigit(month) + L"." + threeDigit(year % 1000);
}

bool parseDataFileName(const wstring& name, wchar_t typeNeed, int& year, int& month) {
    if (name.size() != 12) return false;
    if (towupper(name[0]) != towupper(typeNeed)) return false;
    if (name.substr(1, 5) != STATION_ID) return false;
    if (name[8] != L'.') return false;

    for (int i = 6; i <= 7; ++i) if (!iswdigit(name[i])) return false;
    for (int i = 9; i <= 11; ++i) if (!iswdigit(name[i])) return false;

    month = stoi(name.substr(6, 2));
    int y3 = stoi(name.substr(9, 3));
    year = 2000 + y3;
    return (month >= 1 && month <= 12);
}

fs::path getClassifiedFolderByType(wchar_t typeCh) {
    int idx = typeIndexFromChar(typeCh);
    if (idx < 0) return fs::path();
    return fs::path(DX24_ROOT) / TYPE_DIRS[idx];
}

fs::path findFilePath(wchar_t typeCh, int year, int month) {
    wstring fname = buildFileName(typeCh, year, month);

    fs::path p1 = getClassifiedFolderByType(typeCh) / fname;
    if (fs::exists(p1)) return p1;

    fs::path p2 = fs::path(SOURCE_ROOT) / fname;
    if (fs::exists(p2)) return p2;

    return fs::path();
}

// ====================== 绘图基础 ======================
void fillRoundRect2(int x1, int y1, int x2, int y2, int r, COLORREF fillc, COLORREF linec) {
    setlinecolor(linec);
    setfillcolor(fillc);
    solidroundrect(x1, y1, x2, y2, r, r);
}

void clearScene() {
    setbkcolor(C_BG);
    cleardevice();
}

void drawTextXY(int x, int y, const wstring& s, int h = 24, COLORREF c = C_TEXT, const wchar_t* font = L"微软雅黑") {
    setbkmode(TRANSPARENT);
    settextcolor(c);
    settextstyle(h, 0, font);
    outtextxy(x, y, s.c_str());
}

void drawShadowCard(int x1, int y1, int x2, int y2, COLORREF fillc = C_CARD) {
    setlinecolor(RGB(226, 232, 240));
    setfillcolor(RGB(226, 232, 240));
    solidroundrect(x1 + 4, y1 + 4, x2 + 4, y2 + 4, 16, 16);

    setlinecolor(C_BORDER);
    setfillcolor(fillc);
    solidroundrect(x1, y1, x2, y2, 16, 16);
}

void addButton(int id, int x1, int y1, int x2, int y2, const wstring& text,
    COLORREF fillc = C_PRIMARY, COLORREF textc = WHITE) {
    Button b{ id, x1, y1, x2, y2, text, fillc, textc };
    g_buttons.push_back(b);

    fillRoundRect2(x1, y1, x2, y2, 12, fillc, fillc);

    RECT r{ x1, y1, x2, y2 };
    setbkmode(TRANSPARENT);
    settextcolor(textc);
    settextstyle(22, 0, L"微软雅黑");
    drawtext(text.c_str(), &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void addOutlineButton(int id, int x1, int y1, int x2, int y2, const wstring& text,
    COLORREF linec = C_BORDER, COLORREF textc = C_TEXT) {
    Button b{ id, x1, y1, x2, y2, text, C_CARD, textc };
    g_buttons.push_back(b);

    setlinecolor(linec);
    setfillcolor(C_CARD);
    solidroundrect(x1, y1, x2, y2, 12, 12);

    RECT r{ x1, y1, x2, y2 };
    setbkmode(TRANSPARENT);
    settextcolor(textc);
    settextstyle(20, 0, L"微软雅黑");
    drawtext(text.c_str(), &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void drawTopBar(const wstring& title, const wstring& subtitle = L"") {
    fillRoundRect2(210, 18, WIN_W - 20, 86, 18, C_CARD, C_BORDER);
    drawTextXY(240, 30, title, 30, C_TEXT, L"微软雅黑");
    if (!subtitle.empty()) {
        drawTextXY(242, 62, subtitle, 18, C_TEXT2);
    }
}

void drawSidebar() {
    fillRoundRect2(0, 0, 190, WIN_H, 0, C_SIDEBAR, C_SIDEBAR);

    drawTextXY(24, 26, L"RZ2026", 34, WHITE, L"微软雅黑");
    drawTextXY(24, 68, L"自动气象站数据系统", 18, RGB(203, 213, 225));

    auto navBtn = [&](int id, int y, const wstring& text, bool active) {
        COLORREF fc = active ? C_PRIMARY : C_SIDEBAR_LIGHT;
        COLORREF tc = WHITE;
        addButton(id, 18, y, 172, y + 46, text, fc, tc);
        };

    navBtn(901, 130, L"主菜单", g_page == Page::MENU);
    navBtn(902, 188, L"文件查询", g_page == Page::QUERY);
    navBtn(903, 246, L"T文件分析", g_page == Page::ANALYSIS);
    navBtn(904, 304, L"tree预览", g_page == Page::TREE);
    navBtn(905, 362, L"曲线图", g_page == Page::CHART);

    drawTextXY(24, WIN_H - 110, L"站号：58265", 20, RGB(191, 219, 254));
    drawTextXY(24, WIN_H - 80, L"EasyX 图形界面版", 18, RGB(148, 163, 184));
}

void drawStatusBar() {
    fillRoundRect2(210, WIN_H - 54, WIN_W - 20, WIN_H - 16, 14, C_CARD, C_BORDER);
    drawTextXY(230, WIN_H - 45, L"状态： " + g_status, 20, C_TEXT2);
}

void drawSectionTitle(int x, int y, const wstring& title, const wstring& sub = L"") {
    drawTextXY(x, y, title, 26, C_TEXT);
    if (!sub.empty()) drawTextXY(x, y + 30, sub, 18, C_TEXT2);
}

void drawLabelValueRow(int x, int y, const wstring& label, const wstring& val) {
    drawTextXY(x, y, label, 22, C_TEXT2);
    drawTextXY(x + 110, y, val, 22, C_TEXT);
}

void drawLogsPanel(int x1, int y1, int x2, int y2, const wstring& title) {
    drawShadowCard(x1, y1, x2, y2, C_LOG_BG);
    drawTextXY(x1 + 18, y1 + 16, title, 24, RGB(125, 211, 252));

    setlinecolor(RGB(51, 65, 85));
    line(x1 + 16, y1 + 52, x2 - 16, y1 + 52);

    int y = y1 + 66;
    for (size_t i = 0; i < g_logs.size(); ++i) {
        if (y > y2 - 26) break;
        drawTextXY(x1 + 18, y, g_logs[i], 18, C_LOG_TEXT, L"Consolas");
        y += 26;
    }
}

void drawInfoCard(int x1, int y1, int x2, int y2, const wstring& title, const vector<wstring>& lines) {
    drawShadowCard(x1, y1, x2, y2, C_CARD);
    drawTextXY(x1 + 18, y1 + 16, title, 24, C_TEXT);
    int y = y1 + 54;
    for (auto& s : lines) {
        drawTextXY(x1 + 18, y, s, 19, C_TEXT2);
        y += 28;
        if (y > y2 - 20) break;
    }
}

// ====================== tree.txt ======================
void buildTreeRecursive(const fs::path& root, wofstream& fout, int depth) {
    vector<fs::directory_entry> items;
    for (auto& e : fs::directory_iterator(root)) items.push_back(e);

    sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        return a.path().filename().wstring() < b.path().filename().wstring();
        });

    for (auto& e : items) {
        for (int i = 0; i < depth; ++i) fout << L"│   ";
        fout << L"├── " << e.path().filename().wstring() << L"\n";
        if (e.is_directory()) buildTreeRecursive(e.path(), fout, depth + 1);
    }
}

bool generateTreeFile() {
    try {
        ensureDir(PROJECT_ROOT);

        wofstream fout{ fs::path(TREE_FILE) };
        if (!fout.is_open()) return false;

        fout << L"RZ2026\n";
        fout << L"├── dx24\n";
        buildTreeRecursive(fs::path(DX24_ROOT), fout, 1);

        fout.close();
        return true;
    }
    catch (...) {
        return false;
    }
}

void loadTreePreview() {
    g_logs.clear();

    wifstream fin{ fs::path(TREE_FILE) };
    if (!fin.is_open()) {
        pushLog(L"tree.txt 尚未生成。");
        return;
    }

    wstring line;
    int cnt = 0;
    while (getline(fin, line)) {
        g_logs.push_back(line);
        cnt++;
        if (cnt >= 20) break;
    }

    fin.close();
}

// ====================== 文件分类 ======================
void classifyFiles() {
    initProjectDirs();
    g_logs.clear();

    if (!fs::exists(SOURCE_ROOT)) {
        g_status = L"源数据目录不存在";
        pushLog(L"未找到源目录： " + SOURCE_ROOT);
        return;
    }

    int copied = 0, skipped = 0;
    for (auto& e : fs::directory_iterator(fs::path(SOURCE_ROOT))) {
        if (!e.is_regular_file()) continue;

        wstring name = e.path().filename().wstring();
        if (name.empty()) continue;

        wchar_t typeCh = towupper(name[0]);
        int idx = typeIndexFromChar(typeCh);
        if (idx < 0) {
            skipped++;
            continue;
        }

        if (name.size() < 6 || name.substr(1, 5) != STATION_ID) {
            skipped++;
            continue;
        }

        fs::path dst = fs::path(DX24_ROOT) / TYPE_DIRS[idx] / name;
        try {
            fs::copy_file(e.path(), dst, fs::copy_options::overwrite_existing);
            copied++;
        }
        catch (...) {
            pushLog(L"复制失败： " + name);
        }
    }

    bool ok = generateTreeFile();
    g_status = L"文件分类完成";
    pushLog(L"源目录： " + SOURCE_ROOT);
    pushLog(L"成功分类文件数： " + toW(copied));
    pushLog(L"跳过文件数： " + toW(skipped));
    pushLog(ok ? L"tree.txt 已生成" : L"tree.txt 生成失败");
}

// ====================== T文件解析 ======================
double parseTempField4(const string& raw4) {
    string s = trimAscii(raw4);
    if (s.empty()) return numeric_limits<double>::quiet_NaN();

    bool allMinus = true;
    for (char c : s) {
        if (c != '-') { allMinus = false; break; }
    }
    if (allMinus) return numeric_limits<double>::quiet_NaN();

    bool ok = true;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (!(isdigit((unsigned char)c) || (i == 0 && (c == '+' || c == '-')))) {
            ok = false;
            break;
        }
    }
    if (!ok) return numeric_limits<double>::quiet_NaN();

    try {
        int v = stoi(s);
        return v / 10.0;
    }
    catch (...) {
        return numeric_limits<double>::quiet_NaN();
    }
}

bool queryTemperatureAt(const fs::path& filePath, int day, int hour, int minute, double& outTemp) {
    if (minute < 0 || minute > 59) return false;

    ifstream fin(filePath, ios::binary);
    if (!fin.is_open()) return false;

    string rec(246, '\0');
    long long recIdx = 0;

    while (fin.read(&rec[0], 246)) {
        recIdx++;
        if (recIdx == 1) continue;

        string dh = rec.substr(0, 4);
        if (dh.size() < 4 || !isdigit((unsigned char)dh[0]) || !isdigit((unsigned char)dh[1]) ||
            !isdigit((unsigned char)dh[2]) || !isdigit((unsigned char)dh[3])) {
            continue;
        }

        int d = stoi(dh.substr(0, 2));
        int h = stoi(dh.substr(2, 2));
        if (d != day || h != hour) continue;

        int pos = 4 + minute * 4;
        if (pos + 4 > (int)rec.size()) return false;

        outTemp = parseTempField4(rec.substr(pos, 4));
        return !std::isnan(outTemp);
    }

    return false;
}

map<int, pair<double, int>> calcMonthDailyAvg(const fs::path& filePath, int year, int month) {
    map<int, pair<double, int>> daily;

    ifstream fin(filePath, ios::binary);
    if (!fin.is_open()) return daily;

    string rec(246, '\0');
    long long recIdx = 0;
    int dim = daysInMonth(year, month);

    while (fin.read(&rec[0], 246)) {
        recIdx++;
        if (recIdx == 1) continue;

        string dh = rec.substr(0, 4);
        if (dh.size() < 4 || !isdigit((unsigned char)dh[0]) || !isdigit((unsigned char)dh[1]) ||
            !isdigit((unsigned char)dh[2]) || !isdigit((unsigned char)dh[3])) {
            continue;
        }

        int d = stoi(dh.substr(0, 2));
        if (d < 1 || d > dim) continue;

        for (int m = 0; m < 60; ++m) {
            int pos = 4 + m * 4;
            if (pos + 4 > (int)rec.size()) break;
            double t = parseTempField4(rec.substr(pos, 4));
            if (!std::isnan(t)) {
                daily[d].first += t;
                daily[d].second += 1;
            }
        }
    }

    return daily;
}

vector<int> scanCompleteYearsForT() {
    set<int> years;
    fs::path tdir = getClassifiedFolderByType(L'T');

    if (fs::exists(tdir)) {
        for (auto& e : fs::directory_iterator(tdir)) {
            if (!e.is_regular_file()) continue;
            int y, m;
            if (parseDataFileName(e.path().filename().wstring(), L'T', y, m)) {
                years.insert(y);
            }
        }
    }

    if (years.empty() && fs::exists(SOURCE_ROOT)) {
        for (auto& e : fs::directory_iterator(fs::path(SOURCE_ROOT))) {
            if (!e.is_regular_file()) continue;
            int y, m;
            if (parseDataFileName(e.path().filename().wstring(), L'T', y, m)) {
                years.insert(y);
            }
        }
    }

    vector<int> fullYears;
    for (int y : years) {
        bool full = true;
        for (int m = 1; m <= 12; ++m) {
            if (findFilePath(L'T', y, m).empty()) {
                full = false;
                break;
            }
        }
        if (full) fullYears.push_back(y);
    }

    sort(fullYears.begin(), fullYears.end());
    return fullYears;
}

bool buildYearDailyCurve(int year, vector<double>& outVals) {
    outVals.clear();

    for (int m = 1; m <= 12; ++m) {
        fs::path fp = findFilePath(L'T', year, m);
        if (fp.empty()) return false;

        auto mp = calcMonthDailyAvg(fp, year, m);
        int dim = daysInMonth(year, m);

        for (int d = 1; d <= dim; ++d) {
            if (mp.count(d) && mp[d].second > 0) {
                outVals.push_back(mp[d].first / mp[d].second);
            }
            else {
                outVals.push_back(numeric_limits<double>::quiet_NaN());
            }
        }
    }

    return !outVals.empty();
}

// ====================== 页面绘制 ======================
void drawMenuPage() {
    clearScene();
    g_buttons.clear();
    drawSidebar();
    drawTopBar(L"自动气象站数据分类、查询与 T 文件分析系统", L"电信246黄仁卿 马文博");

    drawShadowCard(220, 110, 610, 730, C_CARD);
    drawSectionTitle(245, 135, L"系统功能", L"点击左侧或下方按钮进入对应操作");

    addButton(1, 250, 210, 580, 260, L"分类文件并生成 tree.txt");
    addButton(2, 250, 285, 580, 335, L"文件查询");
    addButton(3, 250, 360, 580, 410, L"T 文件分析");
    addButton(4, 250, 435, 580, 485, L"查看 tree.txt");
    addButton(5, 250, 510, 580, 560, L"退出系统", C_DANGER, WHITE);

    vector<wstring> infoLines = {
        L"源目录： " + SOURCE_ROOT,
        L"项目目录： " + PROJECT_ROOT,
        L"站号： " + STATION_ID,
        L"支持文件类型：A H Z P T U W R",
        L"建议首次使用先执行文件分类"
    };
    drawInfoCard(640, 110, WIN_W - 20, 250, L"项目信息", infoLines);

    drawLogsPanel(640, 270, WIN_W - 20, 730, L"运行日志 / 结果");
    drawStatusBar();
}

void drawQueryPage() {
    clearScene();
    g_buttons.clear();
    drawSidebar();
    drawTopBar(L"文件查询", L"按 年份 + 月份 + 文件类型 查询对应文件");

    drawShadowCard(220, 110, 620, 500, C_CARD);
    drawSectionTitle(245, 135, L"查询条件");

    drawLabelValueRow(250, 205, L"查询年份", toW(g_queryYear));
    addOutlineButton(101, 430, 195, 480, 232, L"-");
    addButton(102, 495, 195, 545, 232, L"+", C_ACCENT, WHITE);

    drawLabelValueRow(250, 270, L"查询月份", toW(g_queryMonth));
    addOutlineButton(103, 430, 260, 480, 297, L"-");
    addButton(104, 495, 260, 545, 297, L"+", C_ACCENT, WHITE);

    wstring typeStr(1, TYPE_CHARS[g_queryTypeIdx]);
    drawLabelValueRow(250, 335, L"文件类型", typeStr);
    addOutlineButton(105, 430, 325, 480, 362, L"-");
    addButton(106, 495, 325, 545, 362, L"+", C_ACCENT, WHITE);

    addButton(107, 250, 410, 405, 458, L"执行查询", C_PRIMARY, WHITE);
    addOutlineButton(108, 420, 410, 575, 458, L"返回主菜单");

    vector<wstring> tips = {
        L"文件名规则：类型 + 58265 + 月 + . + 年后3位",
        L"例如：T5826501.010",
        L"程序会同时检查源目录与分类目录"
    };
    drawInfoCard(220, 525, 620, 730, L"说明", tips);

    drawLogsPanel(650, 110, WIN_W - 20, 730, L"查询结果");
    drawStatusBar();
}

void drawAnalysisPage() {
    clearScene();
    g_buttons.clear();
    drawSidebar();
    drawTopBar(L"T 文件分析（气温）", L"支持扫描完整年份、查询指定时刻气温、绘制全年曲线");

    drawShadowCard(220, 110, 700, 560, C_CARD);
    drawSectionTitle(245, 135, L"参数设置");

    drawLabelValueRow(255, 200, L"年份", toW(g_anYear));
    addOutlineButton(201, 420, 190, 470, 227, L"-");
    addButton(202, 485, 190, 535, 227, L"+", C_ACCENT, WHITE);

    drawLabelValueRow(255, 255, L"月份", toW(g_anMonth));
    addOutlineButton(203, 420, 245, 470, 282, L"-");
    addButton(204, 485, 245, 535, 282, L"+", C_ACCENT, WHITE);

    drawLabelValueRow(255, 310, L"日期", toW(g_anDay));
    addOutlineButton(205, 420, 300, 470, 337, L"-");
    addButton(206, 485, 300, 535, 337, L"+", C_ACCENT, WHITE);

    drawLabelValueRow(255, 365, L"小时", toW(g_anHour));
    addOutlineButton(207, 420, 355, 470, 392, L"-");
    addButton(208, 485, 355, 535, 392, L"+", C_ACCENT, WHITE);

    drawLabelValueRow(255, 420, L"分钟", toW(g_anMinute));
    addOutlineButton(209, 420, 410, 470, 447, L"-");
    addButton(210, 485, 410, 535, 447, L"+", C_ACCENT, WHITE);

    addButton(211, 245, 495, 385, 542, L"扫描完整年份", C_SUCCESS, WHITE);
    addButton(212, 400, 495, 550, 542, L"查询该时刻气温", C_PRIMARY, WHITE);
    addButton(213, 565, 495, 680, 542, L"画全年曲线", C_ACCENT, WHITE);

    drawInfoCard(220, 585, 700, 730, L"提示", {
        L"全年曲线要求该年份 12 个月 T 文件齐全",
        L"若查询不到气温，可能需微调 T 文件解码规则",
        L"Esc 键可返回主菜单"
        });

    drawLogsPanel(730, 110, WIN_W - 20, 730, L"分析结果");
    addOutlineButton(214, 220, 748, 390, 790, L"返回主菜单");
    drawStatusBar();
}

void drawTreePage() {
    clearScene();
    g_buttons.clear();
    drawSidebar();
    drawTopBar(L"tree.txt 预览", L"显示 tree.txt 文件前 20 行");

    addOutlineButton(301, WIN_W - 210, 28, WIN_W - 40, 72, L"返回主菜单");
    drawLogsPanel(220, 110, WIN_W - 20, 730, L"tree.txt 内容预览");
    drawStatusBar();
}

void drawChartPage() {
    clearScene();
    g_buttons.clear();
    drawSidebar();
    drawTopBar(g_chartTitle.empty() ? L"全年日平均气温曲线图" : g_chartTitle, L"红色折线表示全年日平均气温");

    addOutlineButton(401, WIN_W - 210, 28, WIN_W - 40, 72, L"返回分析页");

    drawShadowCard(220, 110, WIN_W - 20, 730, C_CARD);

    int left = 280, top = 170, right = WIN_W - 70, bottom = 650;

    setlinecolor(C_BORDER);
    line(left, bottom, right, bottom);
    line(left, top, left, bottom);

    if (g_chartValues.empty()) {
        drawTextXY(300, 240, L"没有可绘制的数据。", 30, C_DANGER);
        drawStatusBar();
        return;
    }

    double vmin = 1e9, vmax = -1e9;
    int validCount = 0;
    for (double v : g_chartValues) {
        if (!std::isnan(v)) {
            vmin = min(vmin, v);
            vmax = max(vmax, v);
            validCount++;
        }
    }

    if (validCount == 0) {
        drawTextXY(300, 240, L"全年数据为空。", 30, C_DANGER);
        drawStatusBar();
        return;
    }

    if (fabs(vmax - vmin) < 0.1) {
        vmax += 1.0;
        vmin -= 1.0;
    }

    drawTextXY(250, 128, L"温度曲线图", 28, C_TEXT);
    drawTextXY(250, 155, L"Y 轴：气温(℃)    X 轴：全年日期", 18, C_TEXT2);

    setlinecolor(RGB(226, 232, 240));
    for (int i = 0; i <= 5; ++i) {
        int y = bottom - (bottom - top) * i / 5;
        line(left, y, right, y);

        double val = vmin + (vmax - vmin) * i / 5.0;
        wstringstream ss;
        ss << fixed << setprecision(1) << val << L" ℃";
        drawTextXY(230, y - 10, ss.str(), 18, C_TEXT2);
    }

    vector<int> monthDayAccum;
    monthDayAccum.push_back(0);
    int total = 0;
    for (int m = 1; m <= 12; ++m) {
        total += daysInMonth(g_anYear, m);
        monthDayAccum.push_back(total);
    }

    setlinecolor(RGB(203, 213, 225));
    for (int m = 1; m <= 12; ++m) {
        int dayIdx = monthDayAccum[m - 1];
        int x = left + (right - left) * dayIdx / max(1, (int)g_chartValues.size() - 1);
        line(x, top, x, bottom);
        drawTextXY(x + 2, bottom + 10, toW(m) + L"月", 17, C_TEXT2);
    }

    setlinecolor(C_DANGER);
    setlinestyle(PS_SOLID, 2);

    bool hasPrev = false;
    int px = 0, py = 0;
    int n = (int)g_chartValues.size();

    for (int i = 0; i < n; ++i) {
        double v = g_chartValues[i];
        if (std::isnan(v)) {
            hasPrev = false;
            continue;
        }

        int x = left + (right - left) * i / max(1, n - 1);
        int y = bottom - (int)((v - vmin) / (vmax - vmin) * (bottom - top));

        if (hasPrev) line(px, py, x, y);
        setlinecolor(C_DANGER);
        setfillcolor(C_DANGER);
        solidcircle(x, y, 2);

        px = x;
        py = y;
        hasPrev = true;
    }

    drawInfoCard(230, 620, 520, 710, L"图例", {
        L"红色折线：全年日平均气温",
        L"缺失值位置自动断开",
        L"年份：" + toW(g_anYear)
        });

    drawStatusBar();
}

void drawCurrentPage() {
    switch (g_page) {
    case Page::MENU: drawMenuPage(); break;
    case Page::QUERY: drawQueryPage(); break;
    case Page::ANALYSIS: drawAnalysisPage(); break;
    case Page::TREE: drawTreePage(); break;
    case Page::CHART: drawChartPage(); break;
    }
}

// ====================== 业务动作 ======================
void doQueryFile() {
    g_logs.clear();

    wchar_t t = TYPE_CHARS[g_queryTypeIdx];
    wstring fname = buildFileName(t, g_queryYear, g_queryMonth);
    fs::path classified = getClassifiedFolderByType(t) / fname;
    fs::path source = fs::path(SOURCE_ROOT) / fname;

    pushLog(L"目标文件名： " + fname);
    pushLog(fs::exists(source) ? (L"源目录存在： " + source.wstring()) : L"源目录不存在该文件");
    pushLog(fs::exists(classified) ? (L"分类目录存在： " + classified.wstring()) : L"分类目录不存在该文件");

    if (!fs::exists(classified) && fs::exists(source)) {
        pushLog(L"提示：可以先回主菜单执行“分类文件并生成tree”");
    }

    g_status = L"文件查询完成";
}

void doScanCompleteYears() {
    g_logs.clear();
    g_completeYears = scanCompleteYearsForT();

    if (g_completeYears.empty()) {
        pushLog(L"没有扫描到 12 个月都齐全的年份。");
    }
    else {
        pushLog(L"T文件数据齐全的年份：");
        for (int y : g_completeYears) {
            pushLog(L"  - " + toW(y));
        }
    }

    g_status = L"完整年份扫描完成";
}

void doQueryTemperature() {
    g_logs.clear();
    clampAnalysisDate();

    fs::path fp = findFilePath(L'T', g_anYear, g_anMonth);
    if (fp.empty()) {
        pushLog(L"未找到 T 文件： " + buildFileName(L'T', g_anYear, g_anMonth));
        g_status = L"T文件不存在";
        return;
    }

    double temp = 0.0;
    bool ok = queryTemperatureAt(fp, g_anDay, g_anHour, g_anMinute, temp);

    pushLog(L"文件： " + fp.filename().wstring());
    pushLog(L"时间： " + toW(g_anYear) + L"-" + twoDigit(g_anMonth) + L"-" + twoDigit(g_anDay) +
        L" " + twoDigit(g_anHour) + L":" + twoDigit(g_anMinute));

    if (ok) {
        wstringstream ss;
        ss << fixed << setprecision(1) << temp;
        pushLog(L"气温： " + ss.str() + L" ℃");
        g_status = L"时刻气温查询成功";
    }
    else {
        pushLog(L"该时刻未读到有效温度值。");
        pushLog(L"如果文件存在但未读出，可能需要根据真实 T 文件微调解码规则。");
        g_status = L"未读取到有效温度";
    }
}

void doBuildChart() {
    g_logs.clear();
    clampAnalysisDate();

    vector<double> vals;
    bool ok = buildYearDailyCurve(g_anYear, vals);
    if (!ok) {
        pushLog(L"无法生成全年曲线。");
        pushLog(L"请确认该年份 12 个月 T 文件都存在。");
        g_status = L"全年曲线生成失败";
        return;
    }

    g_chartValues = vals;
    g_chartTitle = toW(g_anYear) + L"年全年日平均气温变化曲线图";
    g_page = Page::CHART;

    pushLog(L"已生成全年日平均气温曲线。");
    g_status = L"全年日平均气温曲线已生成";
}

// ====================== 点击处理 ======================
void handleButtonClick(int id) {
    switch (id) {
    case 1:
        classifyFiles();
        break;
    case 2:
    case 902:
        g_page = Page::QUERY;
        g_status = L"进入文件查询页面";
        break;
    case 3:
    case 903:
        g_page = Page::ANALYSIS;
        g_status = L"进入T文件分析页面";
        break;
    case 4:
    case 904:
        if (!fs::exists(TREE_FILE)) generateTreeFile();
        loadTreePreview();
        g_page = Page::TREE;
        g_status = L"正在查看 tree.txt";
        break;
    case 5:
        closegraph();
        exit(0);
        break;

    case 901:
        g_page = Page::MENU;
        g_status = L"返回主菜单";
        break;

    case 905:
        g_page = Page::CHART;
        g_status = L"进入曲线图页面";
        break;

    case 101: g_queryYear--; break;
    case 102: g_queryYear++; break;
    case 103: g_queryMonth--; if (g_queryMonth < 1) g_queryMonth = 12; break;
    case 104: g_queryMonth++; if (g_queryMonth > 12) g_queryMonth = 1; break;
    case 105: g_queryTypeIdx--; if (g_queryTypeIdx < 0) g_queryTypeIdx = (int)TYPE_CHARS.size() - 1; break;
    case 106: g_queryTypeIdx++; if (g_queryTypeIdx >= (int)TYPE_CHARS.size()) g_queryTypeIdx = 0; break;
    case 107: doQueryFile(); break;
    case 108: g_page = Page::MENU; g_status = L"返回主菜单"; break;

    case 201: g_anYear--; clampAnalysisDate(); break;
    case 202: g_anYear++; clampAnalysisDate(); break;
    case 203: g_anMonth--; if (g_anMonth < 1) g_anMonth = 12; clampAnalysisDate(); break;
    case 204: g_anMonth++; if (g_anMonth > 12) g_anMonth = 1; clampAnalysisDate(); break;
    case 205: g_anDay--; clampAnalysisDate(); break;
    case 206: g_anDay++; clampAnalysisDate(); break;
    case 207: g_anHour--; clampAnalysisDate(); break;
    case 208: g_anHour++; clampAnalysisDate(); break;
    case 209: g_anMinute--; clampAnalysisDate(); break;
    case 210: g_anMinute++; clampAnalysisDate(); break;
    case 211: doScanCompleteYears(); break;
    case 212: doQueryTemperature(); break;
    case 213: doBuildChart(); break;
    case 214: g_page = Page::MENU; g_status = L"返回主菜单"; break;

    case 301:
        g_page = Page::MENU;
        g_status = L"返回主菜单";
        break;

    case 401:
        g_page = Page::ANALYSIS;
        g_status = L"返回T文件分析页面";
        break;
    }
}

void processMouseClick(int x, int y) {
    for (const auto& b : g_buttons) {
        if (inRect(x, y, b)) {
            handleButtonClick(b.id);
            return;
        }
    }
}

// ====================== 主函数 ======================
int main() {
    initProjectDirs();
    initgraph(WIN_W, WIN_H);
    BeginBatchDraw();

    pushLog(L"欢迎使用自动气象站数据分析系统");
    pushLog(L"建议先点击：分类文件并生成tree");
    g_status = L"系统初始化完成";

    while (true) {
        drawCurrentPage();
        FlushBatchDraw();

        ExMessage msg;
        while (peekmessage(&msg, EX_MOUSE | EX_KEY)) {
            if (msg.message == WM_LBUTTONDOWN) {
                processMouseClick(msg.x, msg.y);
            }
            else if (msg.message == WM_KEYDOWN) {
                if (msg.vkcode == VK_ESCAPE) {
                    if (g_page == Page::MENU) {
                        EndBatchDraw();
                        closegraph();
                        return 0;
                    }
                    else {
                        g_page = Page::MENU;
                        g_status = L"按 Esc 返回主菜单";
                    }
                }
            }
        }
        Sleep(15);
    }

    EndBatchDraw();
    closegraph();
    return 0;
}
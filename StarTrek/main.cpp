com// Терминал 24x80 поверх изображения монитора (Windows, Win32 + GDI + GDI+).
// Внешних библиотек не нужно: всё входит в Windows.
//
// ВСЕ РЕСУРСЫ ВСТРОЕНЫ В EXE (см. resource.rc, исходные файлы лежат в папке Files\ проекта):
//   MONITOR_PNG - изображение монитора (Files\monitor.png)
//   START_WAV   - стартовый звук       (Files\start_1db.wav)
//   LOOP_WAV    - фоновый звук         (Files\loop_1db.wav)
//   KLIC_WAV    - звук клавиши         (Files\klic.wav)
//   STARTREK_MD - справка к игре       (Files\startrek_ru.md)
//   ID 1 (ICON) - иконка приложения    (Files\app.ico): значок exe в проводнике,
//                 в заголовке окна и на панели задач
//
// Использование:
//   StarTrek.exe          - изображение берётся из exe
//   StarTrek.exe путь.png   - (необязательно) показать другое изображение вместо встроенного
//
// Звук: при старте проигрывается START_WAV, по его окончании в бесконечном цикле
// запускается LOOP_WAV. При нажатии любой клавиши играет KLIC_WAV.
// Все звуки берутся из памяти, файлов рядом с exe не требуется.
//
// Игра: Super Star Trek (startrek_ru.c + startrek_port.h) запускается в терминале сразу при старте.
// Справка к игре (startrek_ru.md, ресурс STARTREK_MD) открывается в отдельном окне по F1.
//
// Терминал: область вывода (в координатах картинки, начало - левый верхний угол):
//   (115,125) - (965,745), 24 строки по 80 знакомест, зелёный текст на прозрачном фоне.
//   Ввод с клавиатуры: символы, Enter, Backspace; F1 - справка.
//   Регистр (Shift / CapsLock) и раскладка обрабатываются системой через WM_CHAR.
//   Заполняется экран сверху вниз, затем текст прокручивается вверх.
//
// Сохранения: команда sav (startrek_save.c), файл saves.json рядом с exe.
//
// Сборка (Visual Studio): открыть StarTrek.sln, конфигурация Release | x64, Ctrl+Shift+B.
//   Нужна опция компилятора /utf-8 (в проекте уже включена).
// Сборка (MinGW-w64), из папки StarTrek:
//   windres resource.rc -O coff -o resource.o
//   gcc -O2 -c startrek_ru.c -o startrek.o
//   gcc -O2 -c startrek_save.c -o startrek_save.o
//   g++ -O2 -municode main.cpp startrek.o startrek_save.o resource.o -o StarTrek.exe -mwindows -lgdiplus -lgdi32 -lole32 -lshell32 -lwinmm

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <richedit.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <string>
#include <vector>

using std::min;
using std::max;
#include <gdiplus.h>

// Для Visual Studio / MSVC: библиотеки и подсистема подключаются прямо из кода,
// настраивать свойства проекта не нужно.
#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#endif

// Игра (startrek_ru.c, компилируется как C): точка входа main переименована в startrek_main
extern "C" int startrek_main(void);

namespace {

std::unique_ptr<Gdiplus::Bitmap> g_image;
const wchar_t kClassName[] = L"ImageViewerWindow";

// Имена ресурсов (см. resource.rc)
const wchar_t kResImage[] = L"MONITOR_PNG";
const wchar_t kResStart[] = L"START_WAV";
const wchar_t kResLoop[]  = L"LOOP_WAV";
const wchar_t kResClick[] = L"KLIC_WAV";
const wchar_t kResHelp[]  = L"STARTREK_MD";
const int     kIconId      = 1;                 // иконка приложения (ID 1 в resource.rc)

// ======================= Звук =======================
const UINT WM_START_DONE = WM_APP + 1;     // стартовый звук доиграл
std::vector<BYTE> g_startData;             // WAV-файл start_1db.wav в памяти
std::vector<BYTE> g_loopData;              // WAV-файл loop_1db.wav в памяти

// ======================= Терминал =======================
// Область вывода в координатах исходного изображения (X вправо, Y вниз).
const double kAreaX1 = 115.0, kAreaY1 = 125.0;
const double kAreaX2 = 965.0, kAreaY2 = 745.0;
const int    kRows = 24;
const int    kCols = 80;

const COLORREF kTextColor = RGB(70, 255, 150);   // основной зелёный
const COLORREF kGlowColor = RGB(0, 105, 50);     // слабое свечение вокруг символов

const UINT_PTR kBlinkTimer = 1;
const UINT     kBlinkMs    = 530;

wchar_t g_screen[kRows][kCols];   // содержимое экрана
int  g_curRow = 0, g_curCol = 0;  // позиция курсора
bool g_cursorOn = true;           // фаза мигания

// --- процедуры вывода в область (вызывать из потока окна) ---

void ClearLine(int row)
{
    for (int c = 0; c < kCols; ++c) g_screen[row][c] = L' ';
}

void ClearScreen()
{
    for (int r = 0; r < kRows; ++r) ClearLine(r);
    g_curRow = g_curCol = 0;
}

// Прокрутка вверх на одну строку
void ScrollUp()
{
    for (int r = 1; r < kRows; ++r)
        memcpy(g_screen[r - 1], g_screen[r], sizeof(g_screen[r]));
    ClearLine(kRows - 1);
}

// Возврат каретки + перевод строки (с прокруткой на последней строке)
void NewLine()
{
    g_curCol = 0;
    if (g_curRow < kRows - 1) ++g_curRow;
    else                      ScrollUp();
}

// Вывод одного символа в позицию курсора
void PutChar(wchar_t ch)
{
    g_screen[g_curRow][g_curCol] = ch;
    if (++g_curCol >= kCols)
        NewLine();                       // перенос в конце строки
}

// Backspace: удалить символ слева от курсора
void Backspace()
{
    if (g_curCol > 0) {
        --g_curCol;
    } else if (g_curRow > 0) {
        --g_curRow;
        g_curCol = kCols - 1;
    } else {
        return;
    }
    g_screen[g_curRow][g_curCol] = L' ';
}

void Tab()
{
    int next = (g_curCol / 8 + 1) * 8;
    while (g_curCol < next) {
        PutChar(L' ');
        if (g_curCol == 0) break;        // сработал перенос строки
    }
}

// --- отрисовка области вывода ---
// ox, oy - левый верхний угол нарисованного изображения в окне, scale - масштаб изображения
void DrawTerminal(HDC dc, double ox, double oy, double scale)
{
    const double cellW = (kAreaX2 - kAreaX1) / kCols * scale;   // 10.625 * scale
    const double cellH = (kAreaY2 - kAreaY1) / kRows * scale;   // 25.83 * scale
    const double left  = ox + kAreaX1 * scale;
    const double top   = oy + kAreaY1 * scale;

    // Шрифт: высокий и узкий, как в образце retro-term. Ширина знакоместа задаётся явно.
    int fontH = -(int)std::lround(cellH * 0.84);
    int fontW = (int)std::lround(cellW * 0.98);
    HFONT font = CreateFontW(fontH, fontW, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    HGDIOBJ oldFont = SelectObject(dc, font);
    int oldMode = SetBkMode(dc, TRANSPARENT);
    UINT oldAlign = SetTextAlign(dc, TA_LEFT | TA_TOP);

    // Курсор: сплошной блок, как в образце
    if (g_cursorOn) {
        int x1 = (int)std::lround(left + g_curCol * cellW);
        int x2 = (int)std::lround(left + (g_curCol + 1) * cellW);
        int y1 = (int)std::lround(top + g_curRow * cellH + cellH * 0.10);
        int y2 = (int)std::lround(top + (g_curRow + 1) * cellH - cellH * 0.04);
        RECT cr = { x1, y1, x2, y2 };
        HBRUSH br = CreateSolidBrush(kTextColor);
        FillRect(dc, &cr, br);
        DeleteObject(br);
    }

    // Межсимвольные интервалы с накоплением, чтобы 80 знакомест точно ложились в область
    int dx[kCols];
    for (int c = 0; c < kCols; ++c)
        dx[c] = (int)(std::lround((c + 1) * cellW) - std::lround(c * cellW));

    const int glowOff = max(1, (int)std::lround(scale));
    for (int r = 0; r < kRows; ++r) {
        int y = (int)std::lround(top + r * cellH + cellH * 0.04);
        int x = (int)std::lround(left);

        // нет смысла рисовать пустые строки
        bool blank = true;
        for (int c = 0; c < kCols; ++c) if (g_screen[r][c] != L' ') { blank = false; break; }
        if (blank) continue;

        SetTextColor(dc, kGlowColor);
        ExtTextOutW(dc, x - glowOff, y, 0, nullptr, g_screen[r], kCols, dx);
        ExtTextOutW(dc, x + glowOff, y, 0, nullptr, g_screen[r], kCols, dx);
        SetTextColor(dc, kTextColor);
        ExtTextOutW(dc, x, y, 0, nullptr, g_screen[r], kCols, dx);
    }

    SetTextAlign(dc, oldAlign);
    SetBkMode(dc, oldMode);
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

// ======================= Ресурсы =======================

// Копирует данные ресурса RCDATA с указанным именем в память
bool LoadResBytes(const wchar_t* name, std::vector<BYTE>& out)
{
    HRSRC res = FindResourceW(nullptr, name, RT_RCDATA);
    if (!res) return false;
    HGLOBAL glob = LoadResource(nullptr, res);
    DWORD size = SizeofResource(nullptr, res);
    const void* p = glob ? LockResource(glob) : nullptr;
    if (!p || !size) return false;
    out.assign((const BYTE*)p, (const BYTE*)p + size);
    return true;
}

// Изображение из файла (для необязательного аргумента командной строки)
bool LoadImageFile(const std::wstring& path)
{
    std::unique_ptr<Gdiplus::Bitmap> bmp(Gdiplus::Bitmap::FromFile(path.c_str(), FALSE));
    if (!bmp || bmp->GetLastStatus() != Gdiplus::Ok)
        return false;
    g_image = std::move(bmp);
    return true;
}

// Изображение из ресурса exe
bool LoadImageFromResource(const wchar_t* name)
{
    std::vector<BYTE> raw;
    if (!LoadResBytes(name, raw)) return false;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, raw.size());
    if (!hMem) return false;
    void* dst = GlobalLock(hMem);
    if (!dst) { GlobalFree(hMem); return false; }
    memcpy(dst, raw.data(), raw.size());
    GlobalUnlock(hMem);

    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(hMem, TRUE, &stream) != S_OK) {   // TRUE: поток сам освободит hMem
        GlobalFree(hMem);
        return false;
    }

    bool ok = false;
    {
        Gdiplus::Bitmap src(stream, FALSE);
        if (src.GetLastStatus() == Gdiplus::Ok) {
            // Копия в собственной памяти: после этого поток уже не нужен
            std::unique_ptr<Gdiplus::Bitmap> copy(
                src.Clone(0, 0, src.GetWidth(), src.GetHeight(), PixelFormat32bppPARGB));
            if (copy && copy->GetLastStatus() == Gdiplus::Ok) {
                g_image = std::move(copy);
                ok = true;
            }
        }
    }
    stream->Release();
    return ok;
}

// ======================= Звук: запуск =======================

// Поток: проигрывает стартовый звук (из памяти) синхронно, затем сообщает окну.
struct StartCtx { HWND hwnd; };

DWORD WINAPI StartSoundThread(LPVOID param)
{
    StartCtx* ctx = static_cast<StartCtx*>(param);
    HWND hwnd = ctx->hwnd;
    delete ctx;

    PlaySoundW((LPCWSTR)g_startData.data(), nullptr, SND_MEMORY | SND_NODEFAULT | SND_SYNC);
    if (IsWindow(hwnd))
        PostMessageW(hwnd, WM_START_DONE, 0, 0);
    return 0;
}

void StartAudio(HWND hwnd)
{
    if (g_startData.empty()) {
        // стартового звука нет - сразу переходим к циклу
        PostMessageW(hwnd, WM_START_DONE, 0, 0);
        return;
    }
    HANDLE h = CreateThread(nullptr, 0, StartSoundThread, new StartCtx{ hwnd }, 0, nullptr);
    if (h) CloseHandle(h);
}

void StartLoop()
{
    if (!g_loopData.empty())
        PlaySoundW((LPCWSTR)g_loopData.data(), nullptr,
                   SND_MEMORY | SND_NODEFAULT | SND_ASYNC | SND_LOOP);
}

// ======================= Звук клавиш =======================
// KLIC_WAV один раз загружается в память и проигрывается из памяти через waveOut,
// поэтому задержка минимальна.
// PlaySound не подходит: он играет один звук за раз и обрывал бы фоновый loop,
// а отдельные потоки waveOut системный микшер смешивает.
const int kClickVoices = 4;                 // сколько кликов может звучать одновременно

std::vector<BYTE> g_clickData;              // PCM-данные клика в памяти
WAVEFORMATEX      g_clickFmt = {};
HWAVEOUT          g_clickOut[kClickVoices] = {};
WAVEHDR           g_clickHdr[kClickVoices] = {};
int               g_clickNext = 0;

// Разбор WAV (PCM): находим блоки "fmt " и "data"
bool ParseWav(const BYTE* p, size_t n, WAVEFORMATEX& fmt, std::vector<BYTE>& pcm)
{
    if (n < 12 || memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0)
        return false;

    bool haveFmt = false, haveData = false;
    size_t i = 12;
    while (i + 8 <= n) {
        DWORD sz;
        memcpy(&sz, p + i + 4, 4);
        const BYTE* body = p + i + 8;
        size_t avail = n - (i + 8);
        if (sz > avail) sz = (DWORD)avail;

        if (memcmp(p + i, "fmt ", 4) == 0 && sz >= 16) {
            memset(&fmt, 0, sizeof(fmt));
            memcpy(&fmt, body, 16);
            fmt.cbSize = 0;
            haveFmt = true;
        } else if (memcmp(p + i, "data", 4) == 0) {
            pcm.assign(body, body + sz);
            haveData = true;
        }
        i += 8 + sz + (sz & 1);
    }
    return haveFmt && haveData && fmt.wFormatTag == WAVE_FORMAT_PCM;
}

// Вызвать один раз при старте: загрузить звук в память и подготовить устройства
void InitClick()
{
    std::vector<BYTE> raw;
    if (!LoadResBytes(kResClick, raw) ||
        !ParseWav(raw.data(), raw.size(), g_clickFmt, g_clickData))
        return;                                 // нет ресурса - клавиши просто без звука

    for (int i = 0; i < kClickVoices; ++i) {
        if (waveOutOpen(&g_clickOut[i], WAVE_MAPPER, &g_clickFmt, 0, 0, CALLBACK_NULL)
                != MMSYSERR_NOERROR) {
            g_clickOut[i] = nullptr;
            continue;
        }
        g_clickHdr[i].lpData         = (LPSTR)g_clickData.data();
        g_clickHdr[i].dwBufferLength = (DWORD)g_clickData.size();
        waveOutPrepareHeader(g_clickOut[i], &g_clickHdr[i], sizeof(WAVEHDR));
    }
}

// Клавиша-модификатор (Shift, Ctrl, Alt): при удержании шлёт автоповтор
bool IsModifierKey(WPARAM vk)
{
    return vk == VK_SHIFT   || vk == VK_LSHIFT   || vk == VK_RSHIFT   ||
           vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_MENU    || vk == VK_LMENU    || vk == VK_RMENU;
}

// Проиграть клик из памяти (вызывается при нажатии клавиши)
void PlayClick()
{
    // берём свободный голос; если заняты все - перезапускаем следующий по кругу
    int idx = g_clickNext;
    for (int k = 0; k < kClickVoices; ++k) {
        int j = (g_clickNext + k) % kClickVoices;
        if (g_clickOut[j] && !(g_clickHdr[j].dwFlags & WHDR_INQUEUE)) { idx = j; break; }
    }
    g_clickNext = (idx + 1) % kClickVoices;

    if (!g_clickOut[idx]) return;
    waveOutReset(g_clickOut[idx]);              // прервать, если ещё звучит
    waveOutWrite(g_clickOut[idx], &g_clickHdr[idx], sizeof(WAVEHDR));
}

void ShutdownClick()
{
    for (int i = 0; i < kClickVoices; ++i) {
        if (!g_clickOut[i]) continue;
        waveOutReset(g_clickOut[i]);
        waveOutUnprepareHeader(g_clickOut[i], &g_clickHdr[i], sizeof(WAVEHDR));
        waveOutClose(g_clickOut[i]);
        g_clickOut[i] = nullptr;
    }
}

// ======================= Игра Super Star Trek =======================
// Игра работает в отдельном потоке и использует блокирующий ввод/вывод, как в консоли.
// Её printf/gets/getchar/fopen/exit заменены на функции term_* (см. startrek_port.h):
// они пишут в область терминала и ждут ввода строки с клавиатуры окна.
// Экран терминала и строка ввода защищены критической секцией g_lock.
const UINT WM_SHOW_HELP  = WM_APP + 2;    // игра запросила справку (бывший showfile)
const UINT kGameCodePage = CP_UTF8;       // кодировка текстов игры: CP_UTF8, 1251 или 866

CRITICAL_SECTION g_lock;
HWND   g_mainWnd     = nullptr;
HANDLE g_inputEvent  = nullptr;           // сигнал: игрок нажал Enter
bool   g_inputActive = false;             // игра ждёт ввод строки
std::wstring g_inputBuf;                  // вводимая строка
std::string  g_inputResult;               // готовая строка в кодировке игры

// Вывод текста в терминал (вызывается из потока игры)
void TermWriteW(const std::wstring& w)
{
    EnterCriticalSection(&g_lock);
    for (wchar_t ch : w) {
        if (ch == L'\n')      NewLine();
        else if (ch == L'\r') g_curCol = 0;
        else if (ch == L'\t') Tab();
        else if (ch >= 32)    PutChar(ch);
    }
    g_cursorOn = true;
    LeaveCriticalSection(&g_lock);
    if (g_mainWnd) InvalidateRect(g_mainWnd, nullptr, FALSE);
}

void TermWrite(const char* text)
{
    int n = MultiByteToWideChar(kGameCodePage, 0, text, -1, nullptr, 0);
    if (n <= 1) return;
    std::wstring w(n - 1, L'\0');
    MultiByteToWideChar(kGameCodePage, 0, text, -1, &w[0], n);
    TermWriteW(w);
}

// Ждёт, пока игрок введёт строку и нажмёт Enter (поток игры)
std::string TermReadLine()
{
    EnterCriticalSection(&g_lock);
    g_inputBuf.clear();
    g_inputActive = true;
    LeaveCriticalSection(&g_lock);

    WaitForSingleObject(g_inputEvent, INFINITE);

    EnterCriticalSection(&g_lock);
    std::string r = g_inputResult;
    LeaveCriticalSection(&g_lock);
    return r;
}

// --- обработка клавиш в потоке окна ---
void InputChar(wchar_t ch)
{
    EnterCriticalSection(&g_lock);
    if (g_inputActive && g_inputBuf.size() < 250) {
        g_inputBuf.push_back(ch);
        PutChar(ch);                          // эхо на экране
    }
    LeaveCriticalSection(&g_lock);
}

void InputBackspace()
{
    EnterCriticalSection(&g_lock);
    if (g_inputActive && !g_inputBuf.empty()) {
        g_inputBuf.pop_back();
        Backspace();
    }
    LeaveCriticalSection(&g_lock);
}

void InputEnter()
{
    EnterCriticalSection(&g_lock);
    if (g_inputActive) {
        g_inputActive = false;
        NewLine();
        g_inputResult.clear();
        int n = WideCharToMultiByte(kGameCodePage, 0, g_inputBuf.c_str(),
                                    (int)g_inputBuf.size(), nullptr, 0, nullptr, nullptr);
        if (n > 0) {
            g_inputResult.resize(n);
            WideCharToMultiByte(kGameCodePage, 0, g_inputBuf.c_str(),
                                (int)g_inputBuf.size(), &g_inputResult[0], n, nullptr, nullptr);
        }
        SetEvent(g_inputEvent);
    }
    LeaveCriticalSection(&g_lock);
}

DWORD WINAPI GameThread(LPVOID)
{
    startrek_main();
    return 0;
}

// ======================= Справка: Markdown в отдельном окне (F1) =======================
// Текст startrek_ru.md берётся из ресурса STARTREK_MD (UTF-8), преобразуется в RTF
// и показывается в элементе RichEdit. Поддерживается: # заголовки, **жирный**, _курсив_,
// `код`, блоки ```, списки "- " / "* ", экранирование обратным слэшем.
const wchar_t kHelpClass[] = L"HelpWindow";
HWND g_helpWnd = nullptr;

void RtfChar(std::string& out, wchar_t ch)
{
    if (ch == L'\\' || ch == L'{' || ch == L'}') { out += '\\'; out += (char)ch; }
    else if (ch == L'\t')                        out += "\\tab ";
    else if (ch >= 32 && ch < 127)               out += (char)ch;
    else if (ch >= 127)                          out += "\\u" + std::to_string((int)(short)ch) + "?";
}

// Разметка внутри строки: **жирный**, _курсив_, `код`
void RtfInline(std::string& out, const std::wstring& s)
{
    bool bold = false, ital = false, code = false;
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c    = s[i];
        wchar_t prev = i ? s[i - 1] : L' ';
        wchar_t next = (i + 1 < s.size()) ? s[i + 1] : L' ';

        if (c == L'\\' && i + 1 < s.size() && wcschr(L"\\*_`", next)) {
            RtfChar(out, next);               // экранированный символ
            ++i;
            continue;
        }
        if (c == L'*' && next == L'*') {
            bold = !bold; out += bold ? "\\b " : "\\b0 "; ++i; continue;
        }
        if (c == L'`') {
            code = !code; out += code ? "\\f1 " : "\\f0 "; continue;
        }
        if (c == L'_' && !code) {
            bool opening = !ital && !iswalnum(prev) && !iswspace(next) && next != L'_';
            bool closing = ital && !iswspace(prev) && !iswalnum(next);
            if (opening || closing) {
                ital = !ital; out += ital ? "\\i " : "\\i0 "; continue;
            }
        }
        RtfChar(out, c);
    }
    if (bold) out += "\\b0 ";
    if (ital) out += "\\i0 ";
    if (code) out += "\\f0 ";
}

std::string MarkdownToRtf(const std::wstring& md)
{
    std::string out = "{\\rtf1\\ansi\\deff0\\uc1{\\fonttbl{\\f0\\fnil Segoe UI;}"
                      "{\\f1\\fmodern Consolas;}}\\fs22 ";
    bool fence = false;
    size_t pos = 0;
    while (pos <= md.size()) {
        size_t e = md.find(L'\n', pos);
        if (e == std::wstring::npos) e = md.size();
        std::wstring line = md.substr(pos, e - pos);
        pos = e + 1;
        if (!line.empty() && line.back() == L'\r') line.pop_back();

        size_t f = line.find_first_not_of(L" \t");
        std::wstring t = (f == std::wstring::npos) ? std::wstring() : line.substr(f);

        if (t.compare(0, 3, L"```") == 0) { fence = !fence; continue; }

        if (fence) {                                      // блок кода: моноширинный шрифт
            out += "\\f1\\fs20 ";
            for (wchar_t c : line) RtfChar(out, c);
            out += "\\f0\\fs22\\par\n";
            continue;
        }
        if (t.empty()) { out += "\\par\n"; continue; }

        if (t[0] == L'#') {                               // заголовок
            size_t n = 0;
            while (n < t.size() && t[n] == L'#') ++n;
            const char* fs = (n == 1) ? "\\fs36" : (n == 2) ? "\\fs30" : "\\fs26";
            out += "\\b"; out += fs; out += " ";
            RtfInline(out, t.substr(n < t.size() && t[n] == L' ' ? n + 1 : n));
            out += "\\b0\\fs22\\par\n";
            continue;
        }
        if (t.size() > 2 && (t[0] == L'-' || t[0] == L'*' || t[0] == L'+') &&
            t[1] == L' ' && t[2] != L' ') {               // "*  = Звезда" - не список
            out += "\\u8226? ";                           // элемент списка
            RtfInline(out, t.substr(2));
            out += "\\par\n";
            continue;
        }
        RtfInline(out, line);                             // обычная строка (пробелы сохраняются)
        out += "\\par\n";
    }
    out += "}";
    return out;
}

std::wstring LoadHelpText()
{
    std::vector<BYTE> raw;
    if (!LoadResBytes(kResHelp, raw) || raw.empty())
        return L"Справка недоступна.";
    size_t off = (raw.size() >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) ? 3 : 0;
    const char* p = (const char*)raw.data() + off;
    int len = (int)(raw.size() - off);
    int n = MultiByteToWideChar(CP_UTF8, 0, p, len, nullptr, 0);
    std::wstring w(n, L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, p, len, &w[0], n);
    return w;
}

struct RtfCookie { const std::string* s; size_t pos; };

DWORD CALLBACK RtfReader(DWORD_PTR cookie, LPBYTE buf, LONG cb, LONG* pcb)
{
    RtfCookie* c = (RtfCookie*)cookie;
    size_t n = min((size_t)cb, c->s->size() - c->pos);
    memcpy(buf, c->s->data() + c->pos, n);
    c->pos += n;
    *pcb = (LONG)n;
    return 0;
}

LRESULT CALLBACK HelpProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        LoadLibraryW(L"Msftedit.dll");                    // RichEdit 5.0
        HWND ed = CreateWindowExW(0, L"RICHEDIT50W", nullptr,
                                  WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                                  ES_READONLY | ES_AUTOVSCROLL,
                                  0, 0, 0, 0, hwnd, (HMENU)1, GetModuleHandleW(nullptr), nullptr);
        if (ed) {
            SendMessageW(ed, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(14, 14));
            std::string rtf = MarkdownToRtf(LoadHelpText());
            RtfCookie ck = { &rtf, 0 };
            EDITSTREAM es = {};
            es.dwCookie    = (DWORD_PTR)&ck;
            es.pfnCallback = RtfReader;
            SendMessageW(ed, EM_STREAMIN, SF_RTF, (LPARAM)&es);
            SendMessageW(ed, EM_SETSEL, 0, 0);
        }
        return 0;
    }
    case WM_SIZE:
        MoveWindow(GetDlgItem(hwnd, 1), 0, 0, LOWORD(lp), HIWORD(lp), TRUE);
        return 0;
    case WM_SETFOCUS:
        SetFocus(GetDlgItem(hwnd, 1));
        return 0;
    case WM_DESTROY:
        g_helpWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// F1: открыть справку, повторное нажатие закрывает. onlyOpen - только открыть.
void ToggleHelp(HWND owner, bool onlyOpen)
{
    if (g_helpWnd) {
        if (onlyOpen) SetForegroundWindow(g_helpWnd);
        else          DestroyWindow(g_helpWnd);
        return;
    }
    HINSTANCE inst = GetModuleHandleW(nullptr);
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = HelpProc;
        wc.hInstance     = inst;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon         = LoadIconW(inst, MAKEINTRESOURCEW(kIconId));
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = kHelpClass;
        RegisterClassExW(&wc);
        registered = true;
    }
    g_helpWnd = CreateWindowExW(0, kHelpClass, L"Справка - Super Star Trek",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 760, 680,
                                owner, nullptr, inst, nullptr);
    if (g_helpWnd) ShowWindow(g_helpWnd, SW_SHOW);
}

// ======================= Окно =======================

void Paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;

    if (cw > 0 && ch > 0) {
        // Двойная буферизация, чтобы не мерцало при изменении размера
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, cw, ch);
        HGDIOBJ old = SelectObject(mem, bmp);

        HBRUSH bg = CreateSolidBrush(RGB(32, 32, 32));
        FillRect(mem, &rc, bg);
        DeleteObject(bg);

        if (g_image) {
            double iw = g_image->GetWidth();
            double ih = g_image->GetHeight();
            double scale = min(cw / iw, ch / ih);   // вписываем с сохранением пропорций
            int dw = (int)(iw * scale);
            int dh = (int)(ih * scale);
            int dx = (cw - dw) / 2;
            int dy = (ch - dh) / 2;

            {
                Gdiplus::Graphics g(mem);
                g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                g.DrawImage(g_image.get(), dx, dy, dw, dh);
            }

            // Текст поверх изображения, масштаб привязан к изображению
            EnterCriticalSection(&g_lock);
            DrawTerminal(mem, dx, dy, scale);
            LeaveCriticalSection(&g_lock);
        }

        BitBlt(hdc, 0, 0, cw, ch, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
    }

    EndPaint(hwnd, &ps);
}

// Обновить экран; после ввода курсор показываем сразу
void Redraw(HWND hwnd)
{
    g_cursorOn = true;
    SetTimer(hwnd, kBlinkTimer, kBlinkMs, nullptr);   // сброс фазы мигания
    InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, kBlinkTimer, kBlinkMs, nullptr);
        return 0;
    case WM_TIMER:
        if (wp == kBlinkTimer) {
            g_cursorOn = !g_cursorOn;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;                       // фон рисуем сами в Paint()
    case WM_PAINT:
        Paint(hwnd);
        return 0;
    case WM_SIZE:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_START_DONE:
        StartLoop();                    // стартовый звук закончился - зацикливаем loop
        return 0;

    // Клавиши: ввод строки для игры (Enter, Backspace), F1 - справка
    // Alt приходит как WM_SYSKEYDOWN: клик только при первом нажатии, не при удержании.
    // Остальное (Alt+F4, Alt+Пробел) отдаём системе как обычно.
    case WM_SYSKEYDOWN:
        if (IsModifierKey(wp) && !(lp & (1 << 30)))
            PlayClick();
        break;                                  // дальше - в DefWindowProc

    case WM_KEYDOWN:
        // Shift / Ctrl при удержании шлют автоповтор - звук только на первое нажатие
        if (IsModifierKey(wp) && (lp & (1 << 30)))
            return 0;
        PlayClick();                            // звук при нажатии любой клавиши
        switch (wp) {
        case VK_F1:     ToggleHelp(hwnd, false); return 0;
        case VK_RETURN: InputEnter();     break;
        case VK_BACK:   InputBackspace(); break;
        default:        return 0;               // остальные клавиши сами ничего не выводят
        }
        Redraw(hwnd);
        return 0;

    // Отображаемые символы. Система уже учла Shift / CapsLock и раскладку клавиатуры.
    case WM_CHAR:
        if (wp >= 32 && wp != 127 && !(wp >= 0xD800 && wp <= 0xDFFF)) {
            InputChar((wchar_t)wp);
            Redraw(hwnd);
        }
        return 0;

    case WM_SHOW_HELP:                          // игра запросила справку
        ToggleHelp(hwnd, true);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, kBlinkTimer);
        PlaySoundW(nullptr, nullptr, 0);    // остановить любой звук
        ShutdownClick();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

// ======================= Интерфейс игры с терминалом =======================
// Эти функции вызывает код игры вместо printf / putchar / gets / getchar / fopen / fprintf / exit
// (подмена макросами в startrek_port.h). Работают в потоке игры.
extern "C" {

int term_printf(const char* fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;
    TermWrite(buf);
    return n;
}

int term_putchar(int c)
{
    char b[2] = { (char)c, 0 };
    TermWrite(b);
    return c;
}

// Ввод строки с клавиатуры окна (до 255 знаков, как string в игре)
char* term_gets(char* s)
{
    std::string r = TermReadLine();
    size_t n = min(r.size(), (size_t)254);
    memcpy(s, r.data(), n);
    s[n] = 0;
    return s;
}

// Ввод "одного символа": игрок вводит строку, берётся первый знак (пустая строка = Enter)
int term_getchar(void)
{
    std::string r = TermReadLine();
    return r.empty() ? '\n' : (unsigned char)r[0];
}

// Бывшее чтение файла справки startrek.doc: открывает окно справки, файл не открывается
void* term_fopen(const char*, const char*)
{
    if (g_mainWnd) PostMessageW(g_mainWnd, WM_SHOW_HELP, 0, 0);
    return nullptr;
}

int term_fprintf(void*, const char*, ...)
{
    return 0;                                   // сообщения об ошибках файла не нужны
}

// Конец игры: ждём Enter, чтобы игрок успел прочитать итог, затем закрываем окно
void term_exit(int)
{
    TermWriteW(L"\n[Игра окончена. Нажмите Enter, чтобы закрыть окно]");
    TermReadLine();
    if (g_mainWnd) PostMessageW(g_mainWnd, WM_CLOSE, 0, 0);
    ExitThread(0);
}

// ---- файлы рядом с exe (для saves.json): текст в UTF-8 ----

// Полный путь к файлу в папке exe; имя файла задано в UTF-8
static std::wstring ExeFilePath(const char* utf8name)
{
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf, n);
    size_t pos = p.find_last_of(L"\\/");
    p = (pos == std::wstring::npos) ? std::wstring() : p.substr(0, pos + 1);
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8name, -1, nullptr, 0);
    if (len > 1) {
        std::wstring w(len - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8name, -1, &w[0], len);
        p += w;
    }
    return p;
}

// Читает файл целиком; возвращает malloc-буфер с текстом (с завершающим 0) или NULL
char* term_load_text(const char* name)
{
    HANDLE h = CreateFileW(ExeFilePath(name).c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return nullptr;

    char* buf = nullptr;
    DWORD size = GetFileSize(h, nullptr);
    if (size != INVALID_FILE_SIZE && size < 64u * 1024u * 1024u) {
        buf = (char*)malloc((size_t)size + 1);
        DWORD got = 0;
        if (buf && (!ReadFile(h, buf, size, &got, nullptr) || got != size)) {
            free(buf);
            buf = nullptr;
        }
        if (buf) buf[size] = 0;
    }
    CloseHandle(h);
    return buf;
}

// Записывает текст в файл: сначала во временный, потом заменяет им целевой,
// поэтому при сбое старое содержимое не теряется. Возвращает 1 при успехе.
int term_store_text(const char* name, const char* text)
{
    std::wstring path = ExeFilePath(name);
    std::wstring tmp  = path + L".tmp";

    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return 0;

    DWORD len = (DWORD)strlen(text), wrote = 0;
    BOOL ok = WriteFile(h, text, len, &wrote, nullptr) && wrote == len && FlushFileBuffers(h);
    CloseHandle(h);
    if (ok)
        ok = MoveFileExW(tmp.c_str(), path.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) DeleteFileW(tmp.c_str());
    return ok ? 1 : 0;
}

} // extern "C"


// Аннотации SAL (_In_, _In_opt_) как в winbase.h - иначе MSVC выдаёт предупреждение C28251.
// Для компиляторов без SAL (MinGW) макросы определяются пустыми.
#ifndef _In_
#define _In_
#endif
#ifndef _In_opt_
#define _In_opt_
#endif

int WINAPI wWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
    Gdiplus::GdiplusStartupInput gdiInput;
    ULONG_PTR gdiToken = 0;
    Gdiplus::GdiplusStartup(&gdiToken, &gdiInput, nullptr);

    int exitCode = 0;
    {
        // --- изображение: из exe, либо из файла, если путь передан аргументом ---
        std::wstring path;
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv && argc > 1)
            path = argv[1];
        if (argv) LocalFree(argv);

        bool loaded = path.empty() ? LoadImageFromResource(kResImage)
                                   : LoadImageFile(path);
        if (!loaded) {
            MessageBoxW(nullptr, L"Не удалось загрузить изображение.", L"Ошибка",
                        MB_OK | MB_ICONERROR);
            Gdiplus::GdiplusShutdown(gdiToken);
            return 1;
        }

        ClearScreen();
        InitializeCriticalSection(&g_lock);
        g_inputEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);   // автосброс

        // --- окно ---
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kClassName;
        // Иконка из ресурса exe (ID 1): большая для панели задач / Alt+Tab, малая для заголовка окна
        wc.hIcon   = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(kIconId), IMAGE_ICON,
                                       GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON),
                                       LR_DEFAULTCOLOR);
        wc.hIconSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(kIconId), IMAGE_ICON,
                                       GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                       LR_DEFAULTCOLOR);
        RegisterClassExW(&wc);

        // Размер клиентской области = размер картинки, но не больше 90% экрана
        int iw = (int)g_image->GetWidth();
        int ih = (int)g_image->GetHeight();
        double maxW = GetSystemMetrics(SM_CXSCREEN) * 0.9;
        double maxH = GetSystemMetrics(SM_CYSCREEN) * 0.9;
        double k = min(1.0, min(maxW / iw, maxH / ih));
        RECT r = { 0, 0, (LONG)(iw * k), (LONG)(ih * k) };
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

        HWND hwnd = CreateWindowExW(0, kClassName, L"PDP-11 Terminal: StarTrek",
                                    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                    r.right - r.left, r.bottom - r.top,
                                    nullptr, nullptr, hInst, nullptr);
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        // --- звуки: загрузить из exe в память ---
        LoadResBytes(kResStart, g_startData);
        LoadResBytes(kResLoop,  g_loopData);
        InitClick();

        // --- звук: сразу при старте ---
        StartAudio(hwnd);

        // --- игра Star Trek стартует сразу, в отдельном потоке ---
        g_mainWnd = hwnd;
        HANDLE game = CreateThread(nullptr, 0, GameThread, nullptr, 0, nullptr);
        if (game) CloseHandle(game);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        exitCode = (int)msg.wParam;

        g_image.reset();    // освободить до GdiplusShutdown
    }

    Gdiplus::GdiplusShutdown(gdiToken);
    return exitCode;
}

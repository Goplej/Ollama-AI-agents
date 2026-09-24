#pragma once
#include <string>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace encoding {

inline void setup_console() {
#ifdef _WIN32
    // Set console to UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    // Enable virtual terminal processing for ANSI colors
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            dwMode |= ENABLE_PROCESSED_OUTPUT;
            SetConsoleMode(hOut, dwMode);
        }
    }
    
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hIn, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
            SetConsoleMode(hIn, dwMode);
        }
    }
    
    // Set font to support Unicode if possible
    // This is best effort
#endif
}

inline bool is_unicode_supported() {
#ifdef _WIN32
    // Check if console supports Unicode
    // Windows 10 1903+ generally supports UTF-8
    OSVERSIONINFOEXW osvi = { sizeof(osvi), 0, 0, 0, 0, {0}, 0, 0 };
    // Simplified check - assume modern Windows supports Unicode if we set CP to UTF8
    UINT cp = GetConsoleOutputCP();
    return cp == CP_UTF8;
#else
    // Linux/macOS generally support UTF-8
    const char* lang = std::getenv("LANG");
    if (lang && std::string(lang).find("UTF-8") != std::string::npos) return true;
    if (lang && std::string(lang).find("UTF8") != std::string::npos) return true;
    return true; // Assume yes on Unix
#endif
}

inline std::string fix_host(const std::string& host) {
    // Fix common mistakes
    std::string fixed = host;
    
    // 0.0.0.0 is for binding, not connecting - replace with 127.0.0.1
    if (fixed.find("0.0.0.0") != std::string::npos) {
        size_t pos = fixed.find("0.0.0.0");
        fixed.replace(pos, 7, "127.0.0.1");
    }
    
    // Ensure http:// prefix
    if (fixed.find("http://") == std::string::npos && fixed.find("https://") == std::string::npos) {
        // Check if it's just host:port
        if (fixed.find(":") != std::string::npos || fixed == "localhost") {
            fixed = "http://" + fixed;
        }
    }
    
    // Remove trailing slash
    while (!fixed.empty() && fixed.back() == '/') fixed.pop_back();
    
    return fixed;
}

inline std::string get_safe_host(const std::string& host) {
    std::string fixed = fix_host(host);
    
    // If still 0.0.0.0 after fix, use localhost
    if (fixed.find("0.0.0.0") != std::string::npos) {
        return "http://127.0.0.1:11434";
    }
    
    return fixed;
}

// Safe box drawing - ASCII fallback for Windows if Unicode not supported
struct SafeBox {
    static std::string horizontal(bool unicode) { return unicode ? "─" : "-"; }
    static std::string vertical(bool unicode) { return unicode ? "│" : "|"; }
    static std::string top_left(bool unicode) { return unicode ? "╭" : "+"; }
    static std::string top_right(bool unicode) { return unicode ? "╮" : "+"; }
    static std::string bottom_left(bool unicode) { return unicode ? "╰" : "+"; }
    static std::string bottom_right(bool unicode) { return unicode ? "╯" : "+"; }
    static std::string cross(bool unicode) { return unicode ? "┼" : "+"; }
    static std::string t_down(bool unicode) { return unicode ? "┬" : "+"; }
    static std::string t_up(bool unicode) { return unicode ? "┴" : "+"; }
    static std::string t_right(bool unicode) { return unicode ? "├" : "+"; }
    static std::string t_left(bool unicode) { return unicode ? "┤" : "+"; }
    
    static std::string light_h(bool unicode) { return unicode ? "─" : "-"; }
    static std::string light_v(bool unicode) { return unicode ? "│" : "|"; }
    static std::string light_tl(bool unicode) { return unicode ? "┌" : "+"; }
    static std::string light_tr(bool unicode) { return unicode ? "┐" : "+"; }
    static std::string light_bl(bool unicode) { return unicode ? "└" : "+"; }
    static std::string light_br(bool unicode) { return unicode ? "┘" : "+"; }
};

// Safe icons - ASCII fallback
struct SafeIcons {
    static std::string robot(bool unicode) { return unicode ? "[AI]" : "[AI]"; }
    static std::string user(bool unicode) { return unicode ? "[YOU]" : "[YOU]"; }
    static std::string tool(bool unicode) { return unicode ? "[TOOL]" : "[TOOL]"; }
    static std::string think(bool unicode) { return unicode ? "[THINK]" : "[THINK]"; }
    static std::string search(bool unicode) { return unicode ? "[SEARCH]" : "[SEARCH]"; }
    static std::string file(bool unicode) { return unicode ? "[FILE]" : "[FILE]"; }
    static std::string check(bool unicode) { return unicode ? "[OK]" : "[OK]"; }
    static std::string cross(bool unicode) { return unicode ? "[FAIL]" : "[FAIL]"; }
    static std::string arrow(bool unicode) { return unicode ? "->" : "->"; }
    static std::string bullet(bool unicode) { return unicode ? "*" : "*"; }
    static std::string warning(bool unicode) { return unicode ? "[!]" : "[!]"; }
    static std::string info(bool unicode) { return unicode ? "[i]" : "[i]"; }
    static std::string star(bool unicode) { return unicode ? "*" : "*"; }
};

}

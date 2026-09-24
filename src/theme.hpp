#pragma once
#include <string>
#include <map>
#include "utils.hpp"
#include "encoding.hpp"

namespace theme {

// Professional Claude Code inspired theme with ASCII fallback
struct Colors {
    static constexpr const char* RESET = "0";
    static constexpr const char* BOLD = "1";
    static constexpr const char* DIM = "2";
    static constexpr const char* ITALIC = "3";
    static constexpr const char* UNDERLINE = "4";
    static constexpr const char* BLACK = "30";
    static constexpr const char* RED = "31";
    static constexpr const char* GREEN = "32";
    static constexpr const char* YELLOW = "33";
    static constexpr const char* BLUE = "34";
    static constexpr const char* MAGENTA = "35";
    static constexpr const char* CYAN = "36";
    static constexpr const char* WHITE = "37";
    static constexpr const char* GRAY = "90";
    static constexpr const char* BRIGHT_RED = "91";
    static constexpr const char* BRIGHT_GREEN = "92";
    static constexpr const char* BRIGHT_YELLOW = "93";
    static constexpr const char* BRIGHT_BLUE = "94";
    static constexpr const char* BRIGHT_MAGENTA = "95";
    static constexpr const char* BRIGHT_CYAN = "96";
    static constexpr const char* BRIGHT_WHITE = "97";
    static constexpr const char* BG_BLACK = "40";
    static constexpr const char* BG_RED = "41";
    static constexpr const char* BG_GREEN = "42";
    static constexpr const char* BG_YELLOW = "43";
    static constexpr const char* BG_BLUE = "44";
};

struct Theme {
    std::string name;
    std::map<std::string, std::string> colors;
    bool use_unicode = true;
    bool use_colors = true;
    
    std::string get(const std::string& key) const {
        auto it = colors.find(key);
        return it != colors.end() ? it->second : Colors::WHITE;
    }
};

inline Theme get_claude_theme(bool unicode=true) {
    Theme t;
    t.name = "claude";
    t.use_unicode = unicode;
    t.use_colors = true;
    t.colors = {
        {"primary", Colors::BRIGHT_CYAN},
        {"secondary", Colors::GRAY},
        {"success", Colors::BRIGHT_GREEN},
        {"warning", Colors::BRIGHT_YELLOW},
        {"error", Colors::BRIGHT_RED},
        {"info", Colors::BRIGHT_BLUE},
        {"muted", Colors::GRAY},
        {"accent", Colors::BRIGHT_MAGENTA},
        {"border", Colors::GRAY},
        {"text", Colors::WHITE},
        {"highlight", Colors::BRIGHT_WHITE},
        {"code", Colors::BRIGHT_CYAN},
        {"tool", Colors::BRIGHT_YELLOW},
        {"thought", Colors::MAGENTA},
        {"user", Colors::BRIGHT_GREEN},
        {"agent", Colors::BRIGHT_CYAN},
        {"system", Colors::GRAY}
    };
    return t;
}

inline Theme get_professional_theme(bool unicode=true) {
    Theme t;
    t.name = "professional";
    t.use_unicode = unicode;
    t.use_colors = true;
    t.colors = {
        {"primary", "38;5;81"},
        {"secondary", "38;5;245"},
        {"success", "38;5;114"},
        {"warning", "38;5;221"},
        {"error", "38;5;203"},
        {"info", "38;5;75"},
        {"muted", "38;5;240"},
        {"accent", "38;5;177"},
        {"border", "38;5;238"},
        {"text", "38;5;252"},
        {"highlight", "38;5;255"},
        {"code", "38;5;81"},
        {"tool", "38;5;221"},
        {"thought", "38;5;141"},
        {"user", "38;5;114"},
        {"agent", "38;5;81"},
        {"system", "38;5;245"}
    };
    return t;
}

inline Theme get_ascii_theme() {
    Theme t;
    t.name = "ascii";
    t.use_unicode = false;
    t.use_colors = true;
    t.colors = {
        {"primary", Colors::CYAN},
        {"secondary", Colors::GRAY},
        {"success", Colors::GREEN},
        {"warning", Colors::YELLOW},
        {"error", Colors::RED},
        {"info", Colors::BLUE},
        {"muted", Colors::GRAY},
        {"accent", Colors::MAGENTA},
        {"border", Colors::GRAY},
        {"text", Colors::WHITE},
        {"highlight", Colors::WHITE},
        {"code", Colors::CYAN},
        {"tool", Colors::YELLOW},
        {"thought", Colors::MAGENTA},
        {"user", Colors::GREEN},
        {"agent", Colors::CYAN},
        {"system", Colors::GRAY}
    };
    return t;
}

// Icons with ASCII fallback
struct Icons {
    static std::string get(const std::string& name, bool unicode) {
        if (!unicode) {
            if (name=="robot") return "[AI]";
            if (name=="user") return "[YOU]";
            if (name=="tool") return "[TOOL]";
            if (name=="think") return "[THINK]";
            if (name=="search") return "[SEARCH]";
            if (name=="file") return "[FILE]";
            if (name=="check") return "[OK]";
            if (name=="cross") return "[FAIL]";
            if (name=="arrow") return "->";
            if (name=="bullet") return "*";
            if (name=="warning") return "[!]";
            if (name=="info") return "[i]";
            return "*";
        }
        if (name=="robot") return "🤖";
        if (name=="user") return "👤";
        if (name=="tool") return "🔧";
        if (name=="think") return "🧠";
        if (name=="search") return "🔍";
        if (name=="file") return "📁";
        if (name=="check") return "✓";
        if (name=="cross") return "✗";
        if (name=="arrow") return "→";
        if (name=="bullet") return "•";
        if (name=="warning") return "⚠";
        if (name=="info") return "ℹ";
        if (name=="star") return "★";
        if (name=="fire") return "🔥";
        if (name=="rocket") return "🚀";
        return "*";
    }
};

// Professional box drawing with ASCII fallback
struct BoxChars {
    bool unicode;
    BoxChars(bool u=true) : unicode(u) {}
    
    std::string h() const { return unicode ? "─" : "-"; }
    std::string v() const { return unicode ? "│" : "|"; }
    std::string tl() const { return unicode ? "╭" : "+"; }
    std::string tr() const { return unicode ? "╮" : "+"; }
    std::string bl() const { return unicode ? "╰" : "+"; }
    std::string br() const { return unicode ? "╯" : "+"; }
    std::string cross() const { return unicode ? "┼" : "+"; }
    std::string t_down() const { return unicode ? "┬" : "+"; }
    std::string t_up() const { return unicode ? "┴" : "+"; }
    std::string t_right() const { return unicode ? "├" : "+"; }
    std::string t_left() const { return unicode ? "┤" : "+"; }
    std::string light_h() const { return unicode ? "─" : "-"; }
    std::string light_v() const { return unicode ? "│" : "|"; }
    std::string light_tl() const { return unicode ? "┌" : "+"; }
    std::string light_tr() const { return unicode ? "┐" : "+"; }
    std::string light_bl() const { return unicode ? "└" : "+"; }
    std::string light_br() const { return unicode ? "┘" : "+"; }
    
    std::string repeat_h(int n) const {
        std::string s;
        for (int i=0;i<n;i++) s += h();
        return s;
    }
};

inline std::string styled(const std::string& text, const std::string& fg, bool bold=false, bool dim=false, bool use_color=true) {
    if (!use_color) return text;
    std::string code;
    if (bold) { code += Colors::BOLD; code += ";"; }
    if (dim) { code += Colors::DIM; code += ";"; }
    code += fg;
    return utils::color(text, code);
}

inline std::string primary(const std::string& t, bool bold=false, bool use_color=true) { return styled(t, Colors::BRIGHT_CYAN, bold, false, use_color); }
inline std::string success(const std::string& t, bool bold=false, bool use_color=true) { return styled(t, Colors::BRIGHT_GREEN, bold, false, use_color); }
inline std::string warning(const std::string& t, bool bold=false, bool use_color=true) { return styled(t, Colors::BRIGHT_YELLOW, bold, false, use_color); }
inline std::string error(const std::string& t, bool bold=false, bool use_color=true) { return styled(t, Colors::BRIGHT_RED, bold, false, use_color); }
inline std::string muted(const std::string& t, bool use_color=true) { return styled(t, Colors::GRAY, false, true, use_color); }
inline std::string accent(const std::string& t, bool bold=false, bool use_color=true) { return styled(t, Colors::BRIGHT_MAGENTA, bold, false, use_color); }
inline std::string info(const std::string& t, bool use_color=true) { return styled(t, Colors::BRIGHT_BLUE, false, false, use_color); }
inline std::string highlight(const std::string& t, bool bold=true, bool use_color=true) { return styled(t, Colors::BRIGHT_WHITE, bold, false, use_color); }
inline std::string tool_color(const std::string& t, bool bold=false, bool use_color=true) { return styled(t, Colors::BRIGHT_YELLOW, bold, false, use_color); }
inline std::string thought_color(const std::string& t, bool bold=false, bool use_color=true) { return styled(t, Colors::MAGENTA, bold, false, use_color); }
inline std::string user_color(const std::string& t, bool bold=true, bool use_color=true) { return styled(t, Colors::BRIGHT_GREEN, bold, false, use_color); }
inline std::string agent_color(const std::string& t, bool bold=true, bool use_color=true) { return styled(t, Colors::BRIGHT_CYAN, bold, false, use_color); }

inline std::string status_dot(const std::string& status, bool use_color=true) {
    if (status == "success" || status == "ok") return success("●", false, use_color);
    if (status == "error" || status == "fail") return error("●", false, use_color);
    if (status == "warning") return warning("●", false, use_color);
    if (status == "running" || status == "thinking") return primary("●", false, use_color);
    return muted("●", use_color);
}

// Professional banner - ASCII only for compatibility
inline std::string professional_banner(bool use_color=true, bool unicode=true) {
    std::string banner = R"(
   ____  _ _                        _                  _   
  / __ \| | | __ _ _ __ ___   __ _ / \   __ _  ___ _ _| |_ 
 | |  | | | |/ _` | '_ ` _ \ / _` / _ \ / _` |/ _ \ '_| __|
 | |__| | | | (_| | | | | | | (_/ ___ \ (_| |  __/ | | |_ 
  \____/|_|_|\__,_|_| |_| |_|\__,_/_/  \_\__, |\___|_|  \__|
                                         |___/             
)";
    std::string subtitle = " ULTRA-POWERFUL AI AGENT v3.0 PROFESSIONAL | Claude Code Level";
    std::string sub2 = " Supports ANY Ollama model | 22 files | 15k+ LOC | Production Ready";
    
    if (use_color) {
        banner = styled(banner, Colors::BRIGHT_CYAN, true, false, true);
        subtitle = styled(subtitle, Colors::BRIGHT_MAGENTA, true, false, true);
        sub2 = styled(sub2, Colors::GRAY, false, true, true);
    }
    
    return banner + "\n" + subtitle + "\n" + sub2 + "\n";
}

}

#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <functional>
#include "utils.hpp"
#include "theme.hpp"
#include "encoding.hpp"

namespace ui {

// ============================================================================
// BOX - Professional with ASCII fallback
// ============================================================================
class Box {
    int width = 80;
    std::string title;
    std::string border_color = theme::Colors::GRAY;
    bool rounded = false; // Use ASCII by default for Windows compatibility
    bool use_unicode = false;
    bool use_color = true;

public:
    Box(int w=80) : width(w) {
        use_unicode = encoding::is_unicode_supported();
    }
    Box& set_title(const std::string& t) { title = t; return *this; }
    Box& set_width(int w) { width = w; return *this; }
    Box& set_border_color(const std::string& c) { border_color = c; return *this; }
    Box& set_rounded(bool r) { rounded = r; return *this; }
    Box& set_unicode(bool u) { use_unicode = u; return *this; }
    Box& set_color(bool c) { use_color = c; return *this; }

    std::string render(const std::string& content) const {
        std::ostringstream oss;
        theme::BoxChars bc(use_unicode);
        
        std::string tl = bc.tl();
        std::string tr = bc.tr();
        std::string bl = bc.bl();
        std::string br = bc.br();
        std::string h = bc.h();
        std::string v = bc.v();

        // Top border with title
        std::string top = tl;
        std::string dash;
        for (int i=0;i<width-2;i++) dash += h;
        
        if (!title.empty()) {
            std::string title_str = " " + title + " ";
            int remaining = width - 2 - (int)title_str.size();
            if (remaining < 0) remaining = 0;
            std::string rest;
            for (int i=0;i<remaining;i++) rest += h;
            top = tl + title_str + rest + tr;
        } else {
            top = tl + dash + tr;
        }
        
        oss << (use_color ? utils::color(top, border_color) : top) << "\n";

        // Content
        auto lines = utils::split_lines(content);
        for (auto& line : lines) {
            std::vector<std::string> wrapped;
            if ((int)line.size() > width-4) {
                for (size_t i=0;i<line.size();i+=width-4) {
                    wrapped.push_back(line.substr(i, width-4));
                }
            } else wrapped.push_back(line);

            for (auto& wline : wrapped) {
                std::string row = v + " " + wline;
                int pad = width - 3 - (int)wline.size();
                if (pad < 0) pad = 0;
                for (int i=0;i<pad;i++) row += " ";
                row += v;
                oss << (use_color ? utils::color(row, border_color) : row) << "\n";
            }
        }

        // Bottom border
        std::string bottom = bl + dash + br;
        oss << (use_color ? utils::color(bottom, border_color) : bottom) << "\n";
        return oss.str();
    }

    std::string render_lines(const std::vector<std::string>& lines) const {
        return render(utils::join(lines, "\n"));
    }
};

// ============================================================================
// TABLE
// ============================================================================
class Table {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
    std::string border_color = theme::Colors::GRAY;
    bool use_color = true;

public:
    Table& set_headers(const std::vector<std::string>& h) { headers = h; return *this; }
    Table& add_row(const std::vector<std::string>& r) { rows.push_back(r); return *this; }
    Table& set_border_color(const std::string& c) { border_color = c; return *this; }
    Table& set_color(bool c) { use_color = c; return *this; }

    std::string render() const {
        if (headers.empty() && rows.empty()) return "";

        size_t cols = headers.empty() ? (rows.empty() ? 0 : rows[0].size()) : headers.size();
        std::vector<int> widths(cols, 0);
        
        for (size_t i=0;i<headers.size();++i) widths[i] = std::max(widths[i], (int)headers[i].size());
        for (auto& row : rows) for (size_t i=0;i<row.size() && i<widths.size();++i) widths[i] = std::max(widths[i], (int)row[i].size());
        
        for (auto& w : widths) w = std::min(w, 30);

        std::ostringstream oss;
        auto sep = [&](){
            oss << (use_color ? utils::color("+", border_color) : "+");
            for (size_t i=0;i<widths.size();++i) {
                std::string dash;
                for (int j=0;j<widths[i]+2;j++) dash += "-";
                oss << (use_color ? utils::color(dash, border_color) : dash) << (use_color ? utils::color("+", border_color) : "+");
            }
            oss << "\n";
        };

        sep();
        if (!headers.empty()) {
            oss << (use_color ? utils::color("|", border_color) : "|");
            for (size_t i=0;i<headers.size();++i) {
                std::string cell = " " + utils::pad_right(headers[i], widths[i]) + " ";
                oss << (use_color ? theme::highlight(cell, true, use_color) : cell) << (use_color ? utils::color("|", border_color) : "|");
            }
            oss << "\n";
            sep();
        }

        for (auto& row : rows) {
            oss << (use_color ? utils::color("|", border_color) : "|");
            for (size_t i=0;i<row.size() && i<widths.size();++i) {
                std::string cell = " " + utils::pad_right(utils::truncate(row[i], widths[i]), widths[i]) + " ";
                oss << cell << (use_color ? utils::color("|", border_color) : "|");
            }
            oss << "\n";
        }
        sep();
        return oss.str();
    }
};

// ============================================================================
// LIST
// ============================================================================
class List {
    std::vector<std::string> items;
    std::string bullet = "*";
    std::string bullet_color = theme::Colors::BRIGHT_CYAN;
    bool numbered = false;
    bool use_color = true;

public:
    List& add(const std::string& item) { items.push_back(item); return *this; }
    List& set_bullet(const std::string& b) { bullet = b; return *this; }
    List& set_numbered(bool n) { numbered = n; return *this; }
    List& set_color(bool c) { use_color = c; return *this; }

    std::string render() const {
        std::ostringstream oss;
        for (size_t i=0;i<items.size();++i) {
            if (numbered) {
                oss << (use_color ? theme::primary(std::to_string(i+1) + ". ", false, use_color) : std::to_string(i+1) + ". ") << items[i] << "\n";
            } else {
                oss << (use_color ? utils::color(bullet + " ", bullet_color) : bullet + " ") << items[i] << "\n";
            }
        }
        return oss.str();
    }
};

// ============================================================================
// CODE BLOCK
// ============================================================================
class CodeBlock {
    std::string code;
    std::string language;
    bool show_line_numbers = true;
    std::string border_color = theme::Colors::GRAY;
    bool use_color = true;
    bool use_unicode = false;

public:
    CodeBlock(const std::string& c, const std::string& lang="") : code(c), language(lang) {
        use_unicode = encoding::is_unicode_supported();
    }
    CodeBlock& set_line_numbers(bool b) { show_line_numbers = b; return *this; }
    CodeBlock& set_color(bool c) { use_color = c; return *this; }

    std::string render() const {
        std::ostringstream oss;
        std::string header = language.empty() ? "code" : language;
        std::string dash;
        for (int i=0;i<70;i++) dash += "-";
        oss << (use_color ? utils::color("+- " + header + " " + dash, border_color) : "+- " + header + " " + dash) << "\n";
        
        auto lines = utils::split_lines(code);
        for (size_t i=0;i<lines.size();++i) {
            std::string line_no = show_line_numbers ? utils::pad_left(std::to_string(i+1), 3) + " | " : "| ";
            oss << (use_color ? utils::color(line_no, theme::Colors::GRAY) : line_no);
            oss << (use_color ? theme::primary(lines[i], false, use_color) : lines[i]) << "\n";
        }
        
        std::string dash2;
        for (int i=0;i<75;i++) dash2 += "-";
        oss << (use_color ? utils::color("+" + dash2, border_color) : "+" + dash2) << "\n";
        return oss.str();
    }
};

// ============================================================================
// STATUS BAR
// ============================================================================
class StatusBar {
    std::map<std::string, std::string> items;
    int width = 80;
    bool use_color = true;
    bool use_unicode = false;

public:
    StatusBar(int w=80) : width(w) {
        use_unicode = encoding::is_unicode_supported();
    }
    StatusBar& set(const std::string& key, const std::string& value) { items[key] = value; return *this; }
    StatusBar& set_width(int w) { width = w; return *this; }
    StatusBar& set_color(bool c) { use_color = c; return *this; }

    std::string render() const {
        std::ostringstream oss;
        std::string dash;
        for (int i=0;i<width;i++) dash += "-";
        oss << (use_color ? utils::color(dash, theme::Colors::GRAY) : dash) << "\n";
        bool first=true;
        for (auto& kv : items) {
            if (!first) oss << (use_color ? theme::muted(" | ", use_color) : " | ");
            oss << (use_color ? theme::muted(kv.first + ": ", use_color) : kv.first + ": ") 
                << (use_color ? theme::primary(kv.second, false, use_color) : kv.second);
            first=false;
        }
        oss << "\n" << (use_color ? utils::color(dash, theme::Colors::GRAY) : dash) << "\n";
        return oss.str();
    }
};

// ============================================================================
// MARKDOWN RENDERER
// ============================================================================
class MarkdownRenderer {
public:
    static std::string render(const std::string& md, bool use_color=true) {
        std::ostringstream oss;
        auto lines = utils::split_lines(md);
        bool in_code_block = false;
        std::string code_lang;
        std::string code_content;

        for (auto& line : lines) {
            if (utils::starts_with(line, "```")) {
                if (!in_code_block) {
                    in_code_block = true;
                    code_lang = line.size() > 3 ? utils::trim(line.substr(3)) : "";
                    code_content.clear();
                } else {
                    in_code_block = false;
                    CodeBlock cb(code_content, code_lang);
                    cb.set_color(use_color);
                    oss << cb.render() << "\n";
                    code_content.clear();
                }
                continue;
            }

            if (in_code_block) {
                code_content += line + "\n";
                continue;
            }

            std::string trimmed = utils::trim(line);
            if (trimmed.empty()) { oss << "\n"; continue; }

            if (utils::starts_with(trimmed, "# ")) {
                oss << (use_color ? theme::highlight(trimmed.substr(2), true, use_color) : trimmed.substr(2)) << "\n";
            } else if (utils::starts_with(trimmed, "## ")) {
                oss << (use_color ? theme::primary(trimmed.substr(3), true, use_color) : trimmed.substr(3)) << "\n";
            } else if (utils::starts_with(trimmed, "### ")) {
                oss << (use_color ? theme::accent(trimmed.substr(4), true, use_color) : trimmed.substr(4)) << "\n";
            } else if (utils::starts_with(trimmed, "- ") || utils::starts_with(trimmed, "* ")) {
                oss << (use_color ? theme::primary("* ", false, use_color) : "* ") << trimmed.substr(2) << "\n";
            } else {
                oss << line << "\n";
            }
        }

        return oss.str();
    }
};

inline std::string banner(bool use_color=true, bool unicode=true) {
    std::string b = R"(
   ____  _ _                        _                  _   
  / __ \| | | __ _ _ __ ___   __ _ / \   __ _  ___ _ _| |_ 
 | |  | | | |/ _` | '_ ` _ \ / _` / _ \ / _` |/ _ \ '_| __|
 | |__| | | | (_| | | | | | | (_/ ___ \ (_| |  __/ | | |_ 
  \____/|_|_|\__,_|_| |_| |_|\__,_/_/  \_\__, |\___|_|  \__|
                                         |___/             
)";
    std::string subtitle = " ULTRA-POWERFUL AI AGENT v3.0 PROFESSIONAL | Claude Code Level";
    std::string sub2 = " Supports ANY Ollama model | Production Ready | Fixed Encoding";
    
    if (use_color) {
        b = theme::primary(b, true, true);
        subtitle = theme::accent(subtitle, true, true);
        sub2 = theme::muted(sub2, true);
    }
    
    return b + "\n" + subtitle + "\n" + sub2 + "\n";
}

}

#pragma once
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include "utils.hpp"
#include "theme.hpp"
#include "encoding.hpp"
#include "config.hpp"
#include "ui_components.hpp"

namespace ui {

class ProfessionalUI {
    config::AppConfig& cfg;
    int width = 80;
    bool use_colors = true;
    bool use_unicode = true;
    theme::BoxChars box_chars;
    std::atomic<bool> thinking{false};
    std::thread thinking_thread;
    std::string current_thinking_msg = "Thinking...";
    std::vector<std::string> thinking_frames = {"|", "/", "-", "\\"};
    std::vector<std::string> thinking_frames_unicode = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};

public:
    ProfessionalUI(config::AppConfig& c) : cfg(c), box_chars(true) {
        width = cfg.ui.terminal_width > 0 ? cfg.ui.terminal_width : 80;
        use_colors = cfg.ui.use_colors;
        use_unicode = cfg.ui.theme != "ascii";
        
        // Setup console for Windows
        encoding::setup_console();
        use_unicode = encoding::is_unicode_supported() && use_unicode;
        box_chars = theme::BoxChars(use_unicode);
        
        if (use_unicode) thinking_frames = thinking_frames_unicode;
    }

    ~ProfessionalUI() {
        stop_thinking();
    }

    void clear_screen() {
        std::cout << "\033[2J\033[H" << std::flush;
    }

    void print_banner() {
        std::cout << theme::professional_banner(use_colors, use_unicode) << "\n";
    }

    void print_separator(const std::string& title="", const std::string& color=theme::Colors::GRAY) {
        std::string dash;
        for (int i=0;i<width;i++) dash += box_chars.h();
        
        if (title.empty()) {
            std::cout << (use_colors ? utils::color(dash, color) : dash) << "\n";
        } else {
            std::string sep = box_chars.h() + " " + title + " ";
            int remaining = width - (int)sep.size();
            if (remaining < 0) remaining = 0;
            std::string rest;
            for (int i=0;i<remaining;i++) rest += box_chars.h();
            sep += rest;
            std::cout << (use_colors ? utils::color(sep, color) : sep) << "\n";
        }
    }

    void print_user_message(const std::string& msg) {
        std::string dash;
        for (int i=0;i<width-2;i++) dash += box_chars.h();
        
        std::cout << "\n" << (use_colors ? theme::user_color("+-- You", true, use_colors) : "+-- You") << "\n";
        auto lines = utils::split_lines(msg);
        for (auto& line : lines) {
            std::cout << (use_colors ? theme::user_color("| ", true, use_colors) : "| ") << line << "\n";
        }
        std::cout << (use_colors ? theme::user_color("+" + dash, true, use_colors) : "+" + dash) << "\n";
    }

    void print_agent_message(const std::string& msg, bool is_final=false) {
        std::string header = is_final ? "Agent [Final]" : "Agent";
        std::string dash;
        for (int i=0;i<width-2;i++) dash += box_chars.h();
        
        std::cout << "\n" << (use_colors ? theme::agent_color("+-- " + header, true, use_colors) : "+-- " + header) << "\n";
        
        auto lines = utils::split_lines(msg);
        for (auto& line : lines) {
            std::string truncated = line;
            if ((int)truncated.size() > width-4) truncated = truncated.substr(0, width-7) + "...";
            std::cout << (use_colors ? theme::agent_color("| ", true, use_colors) : "| ") << truncated << "\n";
        }
        std::cout << (use_colors ? theme::agent_color("+" + dash, true, use_colors) : "+" + dash) << "\n";
    }

    void print_thinking(const std::string& thought) {
        std::cout << (use_colors ? theme::thought_color("  [THINK] ", false, use_colors) : "  [THINK] ") 
                  << (use_colors ? theme::muted(thought.substr(0,200), use_colors) : thought.substr(0,200)) << "\n";
    }

    void print_tool_call(const std::string& tool_name, const std::map<std::string,std::string>& args) {
        std::string dash;
        for (int i=0;i<width-2;i++) dash += box_chars.h();
        
        std::cout << "\n" << (use_colors ? theme::tool_color("+-- [TOOL] " + tool_name, true, use_colors) : "+-- [TOOL] " + tool_name) << "\n";
        for (auto& kv : args) {
            std::string val = kv.second;
            if (val.size() > 80) val = val.substr(0,80) + "...";
            std::cout << (use_colors ? theme::tool_color("| ", true, use_colors) : "| ") 
                      << (use_colors ? theme::muted(kv.first + ": ", use_colors) : kv.first + ": ") << val << "\n";
        }
        std::cout << (use_colors ? theme::tool_color("+" + dash, true, use_colors) : "+" + dash) << "\n";
    }

    void print_tool_result(const std::string& result, bool success=true, long long elapsed_ms=0) {
        std::string status = success ? 
            (use_colors ? theme::success("[OK] Success", false, use_colors) : "[OK] Success") :
            (use_colors ? theme::error("[FAIL] Failed", false, use_colors) : "[FAIL] Failed");
        if (elapsed_ms > 0) status += (use_colors ? theme::muted(" (" + std::to_string(elapsed_ms) + "ms)", use_colors) : " (" + std::to_string(elapsed_ms) + "ms)");
        
        std::cout << "  +-- " << status << "\n";
        std::string preview = result.substr(0, 400);
        if (result.size() > 400) preview += " ...[" + std::to_string(result.size()) + " chars]";
        auto lines = utils::split_lines(preview);
        for (auto& line : lines) {
            if ((int)line.size() > width-6) line = line.substr(0, width-9) + "...";
            std::cout << "  | " << (use_colors ? theme::muted(line, use_colors) : line) << "\n";
        }
        std::cout << "\n";
    }

    void print_error(const std::string& err) {
        std::cout << (use_colors ? theme::error("[ERROR] ", true, use_colors) : "[ERROR] ") << err << "\n";
    }

    void print_success(const std::string& msg) {
        std::cout << (use_colors ? theme::success("[OK] " + msg, false, use_colors) : "[OK] " + msg) << "\n";
    }

    void print_warning(const std::string& msg) {
        std::cout << (use_colors ? theme::warning("[WARN] " + msg, false, use_colors) : "[WARN] " + msg) << "\n";
    }

    void print_info(const std::string& msg) {
        std::cout << (use_colors ? theme::info("[INFO] " + msg, use_colors) : "[INFO] " + msg) << "\n";
    }

    void start_thinking(const std::string& msg="Thinking...") {
        if (thinking) return;
        thinking = true;
        current_thinking_msg = msg;
        thinking_thread = std::thread([this](){
            int i=0;
            while (thinking) {
                std::string frame = thinking_frames[i % thinking_frames.size()];
                std::cout << "\r" << (use_colors ? theme::primary(frame, false, use_colors) : frame) << " " 
                          << (use_colors ? theme::muted(current_thinking_msg, use_colors) : current_thinking_msg) << "   " << std::flush;
                std::this_thread::sleep_for(std::chrono::milliseconds(120));
                i++;
            }
            std::cout << "\r" << std::string(current_thinking_msg.size()+10, ' ') << "\r" << std::flush;
        });
    }

    void stop_thinking() {
        if (!thinking) return;
        thinking = false;
        if (thinking_thread.joinable()) thinking_thread.join();
    }

    void update_thinking(const std::string& msg) {
        current_thinking_msg = msg;
    }

    void print_status_bar(const std::map<std::string,std::string>& items) {
        std::string dash;
        for (int i=0;i<width;i++) dash += box_chars.h();
        std::cout << (use_colors ? utils::color(dash, theme::Colors::GRAY) : dash) << "\n";
        bool first=true;
        for (auto& kv : items) {
            if (!first) std::cout << (use_colors ? theme::muted(" | ", use_colors) : " | ");
            std::cout << (use_colors ? theme::muted(kv.first + ": ", use_colors) : kv.first + ": ") 
                      << (use_colors ? theme::primary(kv.second, false, use_colors) : kv.second);
            first=false;
        }
        std::cout << "\n" << (use_colors ? utils::color(dash, theme::Colors::GRAY) : dash) << "\n";
    }

    void print_model_info(const std::string& model_name, const std::string& host, bool connected) {
        std::ostringstream oss;
        oss << (use_colors ? theme::muted("Model: ", use_colors) : "Model: ") 
            << (use_colors ? theme::primary(model_name, true, use_colors) : model_name);
        oss << (use_colors ? theme::muted(" | Host: ", use_colors) : " | Host: ") << host;
        oss << " " << (connected ? 
            (use_colors ? theme::success("[Connected]", false, use_colors) : "[Connected]") : 
            (use_colors ? theme::error("[Disconnected]", false, use_colors) : "[Disconnected]"));
        std::cout << oss.str() << "\n";
    }

    void print_help() {
        std::cout << "\n" << (use_colors ? theme::primary("Commands:", true, use_colors) : "Commands:") << "\n";
        std::cout << "  " << (use_colors ? theme::success("/model <name>", false, use_colors) : "/model <name>") << "     - Switch model (e.g. /model qwen2.5:7b)\n";
        std::cout << "  " << (use_colors ? theme::success("/models", false, use_colors) : "/models") << "            - List available models\n";
        std::cout << "  " << (use_colors ? theme::success("/model info <name>", false, use_colors) : "/model info <name>") << " - Show model details\n";
        std::cout << "  " << (use_colors ? theme::success("/clear", false, use_colors) : "/clear") << "             - Clear conversation history\n";
        std::cout << "  " << (use_colors ? theme::success("/tools", false, use_colors) : "/tools") << "             - List available tools\n";
        std::cout << "  " << (use_colors ? theme::success("/memory", false, use_colors) : "/memory") << "            - Show memory\n";
        std::cout << "  " << (use_colors ? theme::success("/sessions", false, use_colors) : "/sessions") << "          - List chat sessions\n";
        std::cout << "  " << (use_colors ? theme::success("/config", false, use_colors) : "/config") << "            - Show config\n";
        std::cout << "  " << (use_colors ? theme::success("/help", false, use_colors) : "/help") << "              - Show this help\n";
        std::cout << "  " << (use_colors ? theme::success("/exit", false, use_colors) : "/exit") << "              - Exit\n";
        std::cout << "\n" << (use_colors ? theme::primary("Tips:", true, use_colors) : "Tips:") << "\n";
        std::cout << "  " << (use_colors ? theme::muted("Just type your task and agent will do it autonomously", use_colors) : "Just type your task and agent will do it autonomously") << "\n";
        std::cout << "  " << (use_colors ? theme::muted("Agent can search web, write code, run commands, manage files", use_colors) : "Agent can search web, write code, run commands, manage files") << "\n";
        std::cout << "\n";
    }

    std::string input(const std::string& prompt="You: ") {
        std::cout << (use_colors ? theme::user_color(prompt, true, use_colors) : prompt);
        std::string line;
        std::getline(std::cin, line);
        return line;
    }

    void print_table(const std::vector<std::string>& headers, const std::vector<std::vector<std::string>>& rows) {
        // Simple ASCII table
        if (headers.empty() && rows.empty()) return;
        
        size_t cols = headers.empty() ? (rows.empty() ? 0 : rows[0].size()) : headers.size();
        std::vector<int> widths(cols, 0);
        
        for (size_t i=0;i<headers.size();++i) widths[i] = std::max(widths[i], (int)headers[i].size());
        for (auto& row : rows) for (size_t i=0;i<row.size() && i<widths.size();++i) widths[i] = std::max(widths[i], (int)row[i].size());
        
        auto sep = [&](){
            std::cout << "+";
            for (size_t i=0;i<widths.size();++i) {
                for (int j=0;j<widths[i]+2;j++) std::cout << "-";
                std::cout << "+";
            }
            std::cout << "\n";
        };
        
        sep();
        if (!headers.empty()) {
            std::cout << "|";
            for (size_t i=0;i<headers.size();++i) {
                std::cout << " " << utils::pad_right(headers[i], widths[i]) << " |";
            }
            std::cout << "\n";
            sep();
        }
        for (auto& row : rows) {
            std::cout << "|";
            for (size_t i=0;i<row.size() && i<widths.size();++i) {
                std::cout << " " << utils::pad_right(utils::truncate(row[i], widths[i]), widths[i]) << " |";
            }
            std::cout << "\n";
        }
        sep();
    }

    void print_box(const std::string& content, const std::string& title="", const std::string& border_color=theme::Colors::GRAY) {
        std::string dash;
        for (int i=0;i<width-2;i++) dash += box_chars.h();
        
        std::cout << box_chars.tl() << dash << box_chars.tr() << "\n";
        if (!title.empty()) {
            std::cout << box_chars.v() << " " << title << "\n";
            std::cout << box_chars.t_right() << dash << box_chars.t_left() << "\n";
        }
        auto lines = utils::split_lines(content);
        for (auto& line : lines) {
            std::cout << box_chars.v() << " " << line << "\n";
        }
        std::cout << box_chars.bl() << dash << box_chars.br() << "\n";
    }

    void print_markdown(const std::string& md) {
        // Simple markdown - just print with colors
        auto lines = utils::split_lines(md);
        for (auto& line : lines) {
            std::string trimmed = utils::trim(line);
            if (utils::starts_with(trimmed, "# ")) {
                std::cout << (use_colors ? theme::highlight(trimmed.substr(2), true, use_colors) : trimmed.substr(2)) << "\n";
            } else if (utils::starts_with(trimmed, "## ")) {
                std::cout << (use_colors ? theme::primary(trimmed.substr(3), true, use_colors) : trimmed.substr(3)) << "\n";
            } else {
                std::cout << line << "\n";
            }
        }
    }

    void print_session_start(const std::string& model, const std::string& host) {
        clear_screen();
        print_banner();
        print_separator();
        std::cout << (use_colors ? theme::muted("  Model: ", use_colors) : "  Model: ") 
                  << (use_colors ? theme::success(model, true, use_colors) : model)
                  << (use_colors ? theme::muted("  Host: ", use_colors) : "  Host: ") << host << "\n";
        std::cout << (use_colors ? theme::muted("  Type /help for commands, /models to list models, /exit to quit", use_colors) : "  Type /help for commands") << "\n";
        print_separator();
        std::cout << "\n";
    }

    void print_session_end() {
        print_separator();
        std::cout << (use_colors ? theme::success("Session ended. Memory saved.", true, use_colors) : "Session ended.") << "\n";
        print_separator();
    }

    void print_prompt_hint() {
        std::cout << (use_colors ? theme::muted("  Try: 'create game', 'find news', 'analyze files', 'write code'", use_colors) : "  Try: 'create game', 'find news'") << "\n";
    }

    int get_width() const { return width; }
    void set_width(int w) { width = w; }
};

}

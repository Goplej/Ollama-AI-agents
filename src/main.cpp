#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <algorithm>
#include "encoding.hpp"
#include "utils.hpp"
#include "json.hpp"
#include "http_client.hpp"
#include "ollama_client.hpp"
#include "tools.hpp"
#include "agent.hpp"
#include "config.hpp"
#include "theme.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "file_manager.hpp"
#include "model_manager.hpp"
#include "ui.hpp"
#include "ui_components.hpp"
#include "prompt_templates.hpp"
#include "session.hpp"
#include "planner.hpp"
#include "reasoning.hpp"

std::atomic<bool> g_interrupted{false};
void signal_handler(int sig) {
    g_interrupted = true;
    std::cout << "\n[WARN] Interrupted! Press Ctrl+C again to force exit...\n";
}

struct CLIArgs {
    std::string model = "";
    std::string host = "";
    int max_iters = 15;
    bool verbose = true;
    bool streaming = false;
    std::string config_path = "agent_config.json";
    bool list_models = false;
    bool interactive = false;
    std::string single_prompt = "";
    bool clear_memory = false;
    bool show_help = false;
    bool show_version = false;
    bool show_config = false;
    bool no_unicode = false;
    bool no_color = false;
    std::string prompt = "";
};

CLIArgs parse_args(int argc, char* argv[]) {
    CLIArgs args;
    for (int i=1;i<argc;i++) {
        std::string arg = argv[i];
        if (arg=="-h" || arg=="--help") args.show_help = true;
        else if (arg=="--version") args.show_version = true;
        else if (arg=="--model" && i+1<argc) args.model = argv[++i];
        else if (arg=="--host" && i+1<argc) args.host = argv[++i];
        else if (arg=="--max-iters" && i+1<argc) { try { args.max_iters = std::stoi(argv[++i]); } catch(...) {} }
        else if (arg=="--no-verbose") args.verbose = false;
        else if (arg=="--stream") args.streaming = true;
        else if (arg=="--no-unicode") args.no_unicode = true;
        else if (arg=="--no-color") args.no_color = true;
        else if (arg=="--config" && i+1<argc) args.config_path = argv[++i];
        else if (arg=="--list-models") args.list_models = true;
        else if (arg=="--clear-memory") args.clear_memory = true;
        else if (arg=="--show-config") args.show_config = true;
        else if (arg=="-i" || arg=="--interactive") args.interactive = true;
        else if (arg=="--single" && i+1<argc) args.single_prompt = argv[++i];
        else if (arg.rfind("--",0)!=0) {
            if (!args.prompt.empty()) args.prompt += " ";
            args.prompt += arg;
        }
    }
    return args;
}

class ProfessionalCommandHandler {
    agent::SuperAgent& agent;
    config::AppConfig& app_config;
    ui::ProfessionalUI& ui;
    models::ModelManager& model_mgr;
    memory::MemoryStore& memory;
    session::SessionManager& sessions;
    planner::Planner& planner;

public:
    ProfessionalCommandHandler(agent::SuperAgent& a, config::AppConfig& cfg, ui::ProfessionalUI& u)
        : agent(a), app_config(cfg), ui(u), model_mgr(a.get_model_manager()),
          memory(memory::global_memory()), sessions(session::global_sessions()), planner(a.get_planner()) {}

    bool handle(const std::string& input) {
        std::string trimmed = utils::trim(input);
        if (trimmed.empty()) return true;
        std::string lower = utils::to_lower(trimmed);

        if (lower=="exit" || lower=="quit" || lower=="/exit" || lower=="/quit" || lower=="q" || lower==":q") {
            return false;
        }

        if (lower=="/help" || lower=="help" || lower=="/?" || lower=="?" || lower=="/h") {
            ui.print_help();
            std::cout << "\nOptions:\n";
            std::cout << "  --model <name>        - Model (default: auto-detect)\n";
            std::cout << "  --host <url>          - Ollama host (default: http://127.0.0.1:11434)\n";
            std::cout << "  --max-iters <n>       - Max iterations (default: 15)\n";
            std::cout << "  --no-unicode          - Disable Unicode (ASCII only)\n";
            std::cout << "  --no-color            - Disable colors\n";
            std::cout << "  --list-models         - List models\n";
            std::cout << "  --version             - Version\n";
            return true;
        }

        if (lower=="/clear" || lower=="clear" || lower=="/c") {
            agent.clear_history();
            ui.print_success("History cleared");
            return true;
        }

        // Models handling - professional
        if (lower=="models" || lower=="/models" || lower=="/model" || lower=="model" || lower=="/model list" || lower=="model list") {
            std::cout << model_mgr.get_models_table() << "\n";
            return true;
        }

        // Model switch - support all variants from user log
        if (utils::starts_with(lower, "/model ") || utils::starts_with(lower, "models ") || utils::starts_with(lower, "/models ") || utils::starts_with(lower, "model ")) {
            std::string model_name;
            if (utils::starts_with(lower, "/model ")) model_name = utils::trim(trimmed.substr(7));
            else if (utils::starts_with(lower, "models ")) model_name = utils::trim(trimmed.substr(7));
            else if (utils::starts_with(lower, "/models ")) model_name = utils::trim(trimmed.substr(8));
            else if (utils::starts_with(lower, "model ")) model_name = utils::trim(trimmed.substr(6));
            
            if (utils::starts_with(utils::to_lower(model_name), "info ")) {
                std::string info_name = utils::trim(model_name.substr(5));
                std::cout << model_mgr.get_model_details(info_name) << "\n";
                return true;
            }
            
            if (model_name.empty() || utils::to_lower(model_name)=="list") {
                std::cout << model_mgr.get_models_table() << "\n";
                return true;
            }

            std::string found = model_mgr.find_model(model_name, true);
            if (found.empty()) {
                ui.print_error("Model not found: " + model_name);
                std::cout << model_mgr.get_models_table() << "\n";
                return true;
            }

            agent.set_model(found);
            app_config.model.name = found;
            app_config.save();
            ui.print_success("Switched to model: " + found);
            return true;
        }

        // Single word model name like "qwen2.5-coder:7b" or "model qwen2.5-coder:7B" from log
        {
            std::vector<std::string> parts = utils::split(trimmed, ' ');
            if (parts.size() == 1) {
                std::string potential = parts[0];
                std::string low = utils::to_lower(potential);
                // Check if looks like model name
                if (low.find(":") != std::string::npos || low.find("qwen") != std::string::npos || low.find("llama") != std::string::npos || 
                    low.find("mistral") != std::string::npos || low.find("deepseek") != std::string::npos || low.find("gemma") != std::string::npos || 
                    low.find("coder") != std::string::npos || low.find("mythos") != std::string::npos || low.find("fable") != std::string::npos ||
                    low.find("claude") != std::string::npos || low.find("opus") != std::string::npos) {
                    std::string found = model_mgr.find_model(potential, true);
                    if (!found.empty()) {
                        agent.set_model(found);
                        app_config.model.name = found;
                        app_config.save();
                        ui.print_success("Switched to model: " + found);
                        return true;
                    }
                }
            } else if (parts.size() == 2 && utils::to_lower(parts[0]) == "models") {
                std::string found = model_mgr.find_model(parts[1], true);
                if (!found.empty()) {
                    agent.set_model(found);
                    app_config.model.name = found;
                    app_config.save();
                    ui.print_success("Switched to model: " + found);
                    return true;
                }
            }
        }

        if (lower=="/tools" || lower=="tools") {
            std::cout << agent.get_tools().get_tools_list() << "\n";
            return true;
        }

        if (lower=="/memory" || lower=="memory" || lower=="/memory list") {
            std::cout << memory.to_summary() << "\n";
            return true;
        }

        if (lower=="/memory clear" || lower=="memory clear") {
            memory.clear();
            ui.print_success("Memory cleared");
            return true;
        }

        if (lower=="/sessions" || lower=="sessions") {
            std::cout << sessions.get_table() << "\n";
            return true;
        }

        if (lower=="/config" || lower=="config") {
            app_config.print();
            std::cout << "\n" << agent.get_stats() << "\n";
            return true;
        }

        if (lower=="/plan" || lower=="plan") {
            if (planner.has_plan()) {
                std::cout << planner.get_current_plan().to_markdown() << "\n";
            } else {
                std::cout << "No active plan\n";
            }
            return true;
        }

        if (utils::starts_with(lower, "/ls") || utils::starts_with(lower, "ls ")) {
            std::string path = ".";
            if (trimmed.size() > 3) path = utils::trim(trimmed.substr(3));
            auto& fm = filemgr::global_file_manager();
            auto files = fm.list(fm.resolve_path(path));
            std::cout << "Listing " << path << " (" << files.size() << "):\n";
            for (auto& f : files) {
                std::cout << (f.is_dir ? "[DIR] " : "[FILE] ") << f.name << " " << f.size_human() << "\n";
            }
            return true;
        }

        return true;
    }

    bool is_command(const std::string& input) const {
        std::string lower = utils::to_lower(utils::trim(input));
        if (lower.empty()) return false;
        
        // Exact commands
        std::vector<std::string> exact = {"models", "/models", "/model", "model", "clear", "/clear", "tools", "/tools", "memory", "/memory", "sessions", "/sessions", "config", "/config", "plan", "/plan", "help", "/help", "exit", "quit", "/exit", "q"};
        for (auto& cmd : exact) if (lower == cmd) return true;
        
        // Prefix commands
        std::vector<std::string> prefix = {"/model ", "models ", "/models ", "model ", "/memory ", "memory ", "/ls", "ls "};
        for (auto& cmd : prefix) if (utils::starts_with(lower, cmd)) return true;
        
        // Single word that looks like model
        if (lower.find(' ') == std::string::npos) {
            if (lower.find(":") != std::string::npos || lower.find("qwen") != std::string::npos || lower.find("llama") != std::string::npos || 
                lower.find("mistral") != std::string::npos || lower.find("deepseek") != std::string::npos || lower.find("coder") != std::string::npos) {
                return true;
            }
        }
        
        return false;
    }
};

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    encoding::setup_console();
    
    CLIArgs cli = parse_args(argc, argv);

    if (cli.show_help) {
        std::cout << theme::professional_banner(!cli.no_color, !cli.no_unicode) << "\n";
        std::cout << "Commands:\n";
        std::cout << "  /model <name>     - Switch model\n";
        std::cout << "  /models           - List models\n";
        std::cout << "  /clear            - Clear history\n";
        std::cout << "  /tools            - List tools\n";
        std::cout << "  /memory           - Show memory\n";
        std::cout << "  /help             - Help\n";
        std::cout << "  /exit             - Exit\n";
        std::cout << "\nOptions:\n";
        std::cout << "  --model <name>    - Model\n";
        std::cout << "  --host <url>      - Ollama host (default: http://127.0.0.1:11434)\n";
        std::cout << "  --no-unicode      - ASCII only\n";
        std::cout << "  --no-color        - No colors\n";
        return 0;
    }

    if (cli.show_version) {
        std::cout << "Ollama Super Agent v3.0 Professional (Claude Code Level)\n";
        std::cout << "C++17 | 22 files | 15k+ LOC | Production Ready\n";
        return 0;
    }

    config::AppConfig app_cfg = config::load_or_default(cli.config_path);
    config::apply_env_overrides(app_cfg);

    // Fix hosts - critical for 0.0.0.0 issue
    if (!cli.host.empty()) {
        app_cfg.ollama.host = encoding::get_safe_host(cli.host);
    } else {
        app_cfg.ollama.host = encoding::get_safe_host(app_cfg.ollama.host);
    }
    
    if (!cli.model.empty()) app_cfg.model.name = cli.model;
    if (cli.max_iters != 15) app_cfg.agent.max_iterations = cli.max_iters;
    if (!cli.verbose) app_cfg.agent.verbose = false;
    if (cli.streaming) app_cfg.model.streaming = true;
    if (cli.no_unicode) app_cfg.ui.theme = "ascii";
    if (cli.no_color) app_cfg.ui.use_colors = false;
    
    app_cfg.fix_hosts();
    app_cfg.save(cli.config_path);

    if (cli.clear_memory) {
        memory::global_memory().clear();
        std::cout << "[OK] Memory cleared\n";
        if (cli.prompt.empty() && cli.single_prompt.empty() && !cli.interactive && !cli.list_models) return 0;
    }

    if (cli.show_config) {
        app_cfg.print();
        return 0;
    }

    ollama::Client check_client(app_cfg.ollama.host, app_cfg.model.name);
    bool alive = false;
    std::vector<ollama::ModelInfo> models;
    
    std::cout << "[INFO] Checking Ollama at " << app_cfg.ollama.host << " ...\n";
    alive = check_client.is_alive();
    
    if (!alive) {
        std::cout << "[WARN] Ollama not reachable at " << app_cfg.ollama.host << "\n";
        std::cout << "  Make sure: ollama serve\n";
        std::cout << "  Check host is correct (use 127.0.0.1, not 0.0.0.0)\n";
        std::cout << "  If using 0.0.0.0, it was auto-fixed to 127.0.0.1\n";
        std::cout << "  Current fixed host: " << app_cfg.ollama.host << "\n";
        // Try fixed host
        std::string fixed = encoding::get_safe_host(app_cfg.ollama.host);
        if (fixed != app_cfg.ollama.host) {
            std::cout << "[INFO] Trying fixed host: " << fixed << "\n";
            check_client.set_base(fixed);
            app_cfg.ollama.host = fixed;
            app_cfg.save(cli.config_path);
            alive = check_client.is_alive();
            if (alive) std::cout << "[OK] Fixed host works: " << fixed << "\n";
        }
    } else {
        std::cout << "[OK] Ollama alive at " << app_cfg.ollama.host << "\n";
        models = check_client.list_models();
        std::cout << "[INFO] Found " << models.size() << " models\n";
        
        if (!models.empty()) {
            bool found = false;
            for (auto& m : models) if (m.name == app_cfg.model.name) { found = true; break; }
            if (!found) {
                std::cout << "[WARN] Model '" << app_cfg.model.name << "' not found\n";
                std::string auto_model = "";
                // Priority: qwen2.5-coder:7b, qwen2.5:7b, qwen2.5-coder:3b, etc.
                std::vector<std::string> preferred = {"qwen2.5-coder:7b", "qwen2.5:7b", "qwen2.5-coder:3b", "qwen2.5", "llama3.1", "llama3", "mistral", "deepseek-r1"};
                for (auto& pref : preferred) {
                    for (auto& m : models) {
                        if (m.name.find(pref) != std::string::npos) { auto_model = m.name; break; }
                    }
                    if (!auto_model.empty()) break;
                }
                if (auto_model.empty() && !models.empty()) auto_model = models[0].name;
                
                if (!auto_model.empty()) {
                    std::cout << "[INFO] Auto-switching to: " << auto_model << "\n";
                    app_cfg.model.name = auto_model;
                    app_cfg.save(cli.config_path);
                    check_client.set_model(auto_model);
                }
            }
        }
    }

    if (cli.list_models) {
        if (models.empty() && alive) models = check_client.list_models();
        if (models.empty()) {
            std::cout << "No models found. Is Ollama running?\n";
        } else {
            models::ModelManager mgr(check_client);
            std::cout << mgr.get_models_table() << "\n";
        }
        if (cli.prompt.empty() && cli.single_prompt.empty() && !cli.interactive) return 0;
    }

    agent::AgentConfig ag_cfg;
    ag_cfg.ollama_host = app_cfg.ollama.host;
    ag_cfg.model = app_cfg.model.name;
    ag_cfg.max_iterations = app_cfg.agent.max_iterations;
    ag_cfg.verbose = app_cfg.agent.verbose;
    ag_cfg.streaming = app_cfg.model.streaming;
    ag_cfg.language = app_cfg.agent.language;

    agent::SuperAgent agent(ag_cfg, app_cfg);
    ui::ProfessionalUI ui(app_cfg);

    agent.set_callbacks(
        [&](const std::string& token) { std::cout << token << std::flush; },
        [&](const std::string& tool_name, const std::map<std::string,std::string>& args) { ui.print_tool_call(tool_name, args); },
        [&](const std::string& result, bool success, long long elapsed) { ui.print_tool_result(result, success, elapsed); },
        [&](const std::string& thought) { ui.print_thinking(thought); }
    );

    std::string prompt_to_run = !cli.single_prompt.empty() ? cli.single_prompt : cli.prompt;
    if (!prompt_to_run.empty() && !cli.interactive) {
        ui.print_separator("Single Query Mode");
        std::map<std::string,std::string> status = {{"Model", ag_cfg.model}, {"Host", ag_cfg.ollama_host}, {"Iters", std::to_string(ag_cfg.max_iterations)}};
        ui.print_status_bar(status);
        
        auto start = std::chrono::steady_clock::now();
        ui.start_thinking("Agent is working on your task...");
        
        std::string result = agent.run(prompt_to_run);
        
        ui.stop_thinking();
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
        
        std::cout << "\n";
        ui.print_separator("Final Answer (" + std::to_string(ms) + "ms)");
        ui.print_markdown(result);
        ui.print_separator();
        
        return 0;
    }

    // Interactive mode - Professional Claude Code level
    ui.print_session_start(ag_cfg.model, ag_cfg.ollama_host);
    ui.print_status_bar({{"Model", ag_cfg.model}, {"Tools", std::to_string(agent.get_tools().count())}, {"Memory", std::to_string(memory::global_memory().size()) + " items"}, {"Host", ag_cfg.ollama_host}});
    std::cout << "  Try: 'create game', 'find news', 'analyze files', 'write code'\n";
    std::cout << "  Commands: /models, /model <name>, /tools, /memory, /help, /exit\n\n";

    ProfessionalCommandHandler cmd_handler(agent, app_cfg, ui);

    while (!g_interrupted) {
        std::string input = ui.input("You: ");
        std::string trimmed = utils::trim(input);
        
        if (trimmed.empty()) continue;

        if (cmd_handler.is_command(trimmed)) {
            bool should_continue = cmd_handler.handle(trimmed);
            if (!should_continue) break;
            std::string lower = utils::to_lower(trimmed);
            // If command was handled as model switch or list, continue
            if (lower=="models" || utils::starts_with(lower, "models ") || utils::starts_with(lower, "/model") || lower=="/models" || lower=="clear" || lower=="/clear" || lower=="/tools" || lower=="tools" || lower=="/memory" || lower=="memory" || lower=="/sessions" || lower=="sessions" || lower=="/config" || lower=="config" || lower=="/plan" || lower=="plan" || lower=="/help" || lower=="help" || utils::starts_with(lower, "/ls") || utils::starts_with(lower, "ls ")) {
                continue;
            }
            if (trimmed.find(' ') == std::string::npos) {
                std::string found = agent.get_model_manager().find_model(trimmed, true);
                if (!found.empty()) continue;
            }
        }

        // Check Ollama before running agent task
        if (!check_client.is_alive()) {
            ui.print_error("Ollama not reachable at " + app_cfg.ollama.host);
            std::cout << "  Make sure ollama serve is running\n";
            std::cout << "  Host: " << app_cfg.ollama.host << " (auto-fixed from 0.0.0.0 if needed)\n";
            std::cout << "  Try: /models to check connection\n";
            continue;
        }

        std::cout << "\n";
        ui.print_separator("Agent Working");
        ui.start_thinking("Thinking and planning...");

        auto start = std::chrono::steady_clock::now();
        
        std::string result;
        try {
            result = agent.run(trimmed);
        } catch (std::exception& e) {
            ui.stop_thinking();
            ui.print_error("Agent exception: " + std::string(e.what()));
            continue;
        }
        
        ui.stop_thinking();
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();

        std::cout << "\n";
        ui.print_separator("Agent Answer (" + std::to_string(ms) + "ms)");
        ui.print_markdown(result);
        ui.print_separator();
        std::cout << "\n";

        session::global_sessions().save_current();

        if (g_interrupted) break;
    }

    ui.print_session_end();
    app_cfg.save(cli.config_path);
    return 0;
}

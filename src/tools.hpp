#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include "utils.hpp"
#include "http_client.hpp"
#include "json.hpp"
#include "logger.hpp"
#include "theme.hpp"
#include "memory.hpp"
#include "file_manager.hpp"
#include "web_search_engine.hpp"
#include "code_analyzer.hpp"

namespace tools {

struct ToolResult {
    bool success = true;
    std::string content;
    std::string error;
    long long elapsed_ms = 0;
    std::map<std::string,std::string> metadata;

    static ToolResult ok(const std::string& c) { return {true, c, "", 0, {}}; }
    static ToolResult fail(const std::string& e) { return {false, "", e, 0, {}}; }
};

struct Tool {
    std::string name;
    std::string description;
    std::string parameters; // JSON schema description
    std::string category; // search, file, code, system, memory, reasoning
    std::vector<std::string> examples;
    bool requires_confirmation = false;
    int timeout_ms = 30000;
    std::function<ToolResult(const std::map<std::string,std::string>&)> func;
};

class Registry {
    std::map<std::string, Tool> tools;
    memory::MemoryStore& memory;
    filemgr::FileManager& file_mgr;
    websearch::SearchEngine& search_engine;

public:
    Registry() : memory(memory::global_memory()), file_mgr(filemgr::global_file_manager()), search_engine(websearch::global_search_engine()) {
        register_all();
    }

    void register_tool(const Tool& tool) {
        tools[tool.name] = tool;
        LOG_DEBUG("Registered tool: " + tool.name);
    }

    void register_all() {
        // ==================== SEARCH TOOLS ====================
        tools["web_search"] = Tool{
            "web_search",
            "Поиск в интернете. Ищет актуальную информацию в сети. Вход: query (строка поиска), max_results (опционально, количество результатов). Возвращает топ результатов с заголовками, URL и сниппетами.",
            "{\"query\": \"поисковый запрос\", \"max_results\": 5}",
            "search",
            {"web_search(query=\"AI news 2024\")", "web_search(query=\"Python tutorial\", max_results=10)"},
            false,
            20000,
            [this](const std::map<std::string,std::string>& args) -> ToolResult {
                auto it = args.find("query");
                if (it == args.end() || it->second.empty()) return ToolResult::fail("Missing query");
                std::string q = it->second;
                int max_res = 8;
                auto it2 = args.find("max_results");
                if (it2 != args.end()) { try { max_res = std::stoi(it2->second); } catch(...) {} }
                
                websearch::SearchOptions opts;
                opts.max_results = max_res;
                opts.timeout_ms = 15000;
                search_engine.set_options(opts);
                
                auto results = search_engine.search(q);
                if (results.empty()) {
                    return ToolResult::ok("Web search for '" + q + "' returned no results. Possibly no internet or search engine blocked. Use your knowledge but note it may be outdated.");
                }
                
                std::string out = search_engine.results_to_string(results);
                return ToolResult::ok(out);
            }
        };

        tools["web_fetch"] = Tool{
            "web_fetch",
            "Скачивает и читает содержимое веб-страницы по URL. Вход: url (обязательно), max_length (опционально). Возвращает текстовое содержимое страницы, очищенное от HTML.",
            "{\"url\": \"https://example.com\", \"max_length\": 10000}",
            "search",
            {"web_fetch(url=\"https://example.com\")", "web_fetch(url=\"https://news.ycombinator.com\", max_length=5000)"},
            false,
            20000,
            [this](const std::map<std::string,std::string>& args) -> ToolResult {
                auto it = args.find("url");
                if (it == args.end()) return ToolResult::fail("Missing url");
                std::string url = it->second;
                if (!utils::is_valid_url(url)) return ToolResult::fail("Invalid URL: " + url);
                
                int max_len = 12000;
                auto it2 = args.find("max_length");
                if (it2 != args.end()) { try { max_len = std::stoi(it2->second); } catch(...) {} }
                
                std::string content = search_engine.fetch_page(url, max_len);
                return ToolResult::ok("Content of " + url + ":\n\n" + content);
            }
        };

        // ==================== FILE TOOLS ====================
        tools["file_read"] = Tool{
            "file_read",
            "Читает содержимое файла с диска. Вход: path (путь к файлу). Возвращает содержимое файла с информацией о размере и типе.",
            "{\"path\": \"path/to/file.txt\"}",
            "file",
            {"file_read(path=\"README.md\")", "file_read(path=\"src/main.cpp\")"},
            false,
            10000,
            [this](const std::map<std::string,std::string>& args) -> ToolResult {
                auto it = args.find("path");
                if (it == args.end()) return ToolResult::fail("Missing path");
                std::string path = it->second;
                std::string full = file_mgr.resolve_path(path);
                if (!utils::file_exists(full)) return ToolResult::fail("File not found: " + full);
                if (utils::is_directory(full)) {
                    auto files = file_mgr.list(full);
                    std::ostringstream oss;
                    oss << "Path is directory: " << full << "\nListing:\n";
                    for (auto& f : files) oss << (f.is_dir ? "[DIR] " : "[FILE] ") << f.name << " " << f.size_human() << "\n";
                    return ToolResult::ok(oss.str());
                }
                std::string content = file_mgr.read(full, 50000);
                long long sz = utils::file_size(full);
                std::string header = "File: " + full + " (" + std::to_string(sz) + " bytes, ext: " + utils::get_extension(full) + ")\n";
                header += std::string(60, '-') + "\n";
                return ToolResult::ok(header + content);
            }
        };

        tools["file_write"] = Tool{
            "file_write",
            "Записывает содержимое в файл. Создает файл если не существует, перезаписывает если существует. Вход: path (путь), content (содержимое), backup (опционально, создавать бэкап). ОЧЕНЬ МОЩНЫЙ - используй для создания кода, отчетов, любых файлов.",
            "{\"path\": \"file.txt\", \"content\": \"содержимое файла\", \"backup\": false}",
            "file",
            {"file_write(path=\"hello.py\", content=\"print('hello')\")", "file_write(path=\"report.md\", content=\"# Report\\nContent\")"},
            false,
            10000,
            [this](const std::map<std::string,std::string>& args) -> ToolResult {
                auto it1 = args.find("path");
                auto it2 = args.find("content");
                if (it1 == args.end() || it2 == args.end()) return ToolResult::fail("Missing path or content");
                bool backup = false;
                auto it3 = args.find("backup");
                if (it3 != args.end()) backup = it3->second == "true" || it3->second == "1";
                bool ok = file_mgr.write(it1->second, it2->second, backup);
                if (!ok) return ToolResult::fail("Failed to write file: " + it1->second);
                return ToolResult::ok("File written successfully: " + file_mgr.resolve_path(it1->second) + " (" + std::to_string(it2->second.size()) + " bytes)" + (backup ? " [backup created]" : ""));
            }
        };

        tools["file_list"] = Tool{
            "file_list",
            "Список файлов и папок в директории. Вход: path (директория, по умолчанию текущая), show_hidden (bool), filter (фильтр по имени). Возвращает детальный список с размерами и датами.",
            "{\"path\": \".\", \"show_hidden\": false, \"filter\": \"\"}",
            "file",
            {"file_list(path=\".\")", "file_list(path=\"src\", filter=\".cpp\")"},
            false,
            10000,
            [this](const std::map<std::string,std::string>& args) -> ToolResult {
                std::string path = ".";
                auto it = args.find("path");
                if (it != args.end() && !it->second.empty()) path = it->second;
                bool show_hidden = false;
                auto it2 = args.find("show_hidden");
                if (it2 != args.end()) show_hidden = it2->second == "true";
                std::string filter = "";
                auto it3 = args.find("filter");
                if (it3 != args.end()) filter = it3->second;
                
                std::string full = file_mgr.resolve_path(path);
                if (!utils::file_exists(full)) return ToolResult::fail("Path not exists: " + full);
                if (!utils::is_directory(full)) return ToolResult::fail("Not a directory: " + full);
                
                auto files = file_mgr.list(full, show_hidden, filter);
                std::ostringstream oss;
                oss << "Listing of " << full << " (" << files.size() << " items):\n";
                oss << std::string(80, '-') << "\n";
                for (auto& f : files) {
                    oss << f.icon(false) << " " << (f.is_dir ? "[DIR] " : "[FILE]") << " " << utils::pad_right(f.name, 30) << " " << utils::pad_right(f.size_human(), 12) << " " << f.modified << "\n";
                }
                return ToolResult::ok(oss.str());
            }
        };

        tools["file_delete"] = Tool{
            "file_delete",
            "Удаляет файл или пустую директорию. Вход: path. ОСТОРОЖНО - необратимо!",
            "{\"path\": \"file/to/delete.txt\"}",
            "file",
            {"file_delete(path=\"temp.txt\")"},
            true,
            5000,
            [this](const std::map<std::string,std::string>& args) -> ToolResult {
                auto it = args.find("path");
                if (it == args.end()) return ToolResult::fail("Missing path");
                std::string full = file_mgr.resolve_path(it->second);
                if (!utils::file_exists(full)) return ToolResult::fail("File not found: " + full);
                bool ok = file_mgr.remove(full);
                if (!ok) return ToolResult::fail("Failed to delete: " + full);
                return ToolResult::ok("Deleted: " + full);
            }
        };

        tools["file_search"] = Tool{
            "file_search",
            "Ищет текст внутри файлов в директории. Вход: query (что искать), path (где искать), ext (фильтр по расширению). Возвращает список файлов с количеством совпадений.",
            "{\"query\": \"TODO\", \"path\": \".\", \"ext\": \".cpp\"}",
            "file",
            {"file_search(query=\"TODO\")", "file_search(query=\"function\", path=\"src\", ext=\".js\")"},
            false,
            15000,
            [this](const std::map<std::string,std::string>& args) -> ToolResult {
                auto it = args.find("query");
                if (it == args.end()) return ToolResult::fail("Missing query");
                std::string query = it->second;
                std::string path = ".";
                auto it2 = args.find("path");
                if (it2 != args.end() && !it2->second.empty()) path = it2->second;
                std::string ext = "";
                auto it3 = args.find("ext");
                if (it3 != args.end()) ext = it3->second;
                
                auto results = file_mgr.search_in_files(query, file_mgr.resolve_path(path), ext);
                if (results.empty()) return ToolResult::ok("No files found containing: " + query);
                
                std::ostringstream oss;
                oss << "Found " << results.size() << " files containing '" << query << "':\n";
                for (auto& r : results) {
                    oss << "  " << r.first << " (" << r.second << " matches)\n";
                }
                return ToolResult::ok(oss.str());
            }
        };

        // ==================== CODE TOOLS ====================
        tools["code_write"] = Tool{
            "code_write",
            "Пишет код в файл с автоматическим определением языка и созданием директорий. Вход: path (путь), code (код), language (опционально, язык). Создает директории если нужно.",
            "{\"path\": \"script.py\", \"code\": \"print('hello')\", \"language\": \"python\"}",
            "code",
            {"code_write(path=\"app.py\", code=\"print('hi')\", language=\"python\")"},
            false,
            10000,
            [this](const std::map<std::string,std::string>& args) -> ToolResult {
                auto it1 = args.find("path");
                auto it2 = args.find("code");
                if (it1==args.end() || it2==args.end()) return ToolResult::fail("Missing path/code");
                std::string lang = "";
                auto it3 = args.find("language");
                if (it3 != args.end()) lang = it3->second;
                
                bool ok = file_mgr.write(it1->second, it2->second, false);
                if (!ok) return ToolResult::fail("Failed to write code file");
                
                // Analyze code
                code::Language detected = code::Analyzer::detect_language(it1->second);
                if (lang.empty()) lang = code::Analyzer::language_name(detected);
                auto metrics = code::Analyzer::analyze(it2->second, detected);
                
                std::ostringstream oss;
                oss << "Code written to " << file_mgr.resolve_path(it1->second) << " (" << lang << ", " << it2->second.size() << " bytes)\n";
                oss << "Metrics: " << metrics.lines_code << " code lines, " << metrics.functions << " functions, complexity " << metrics.complexity << "\n";
                return ToolResult::ok(oss.str());
            }
        };

        tools["code_analyze"] = Tool{
            "code_analyze",
            "Анализирует код в файле: метрики, проблемы, предложения. Вход: path (путь к файлу).",
            "{\"path\": \"src/main.cpp\"}",
            "code",
            {"code_analyze(path=\"main.cpp\")", "code_analyze(path=\"app.py\")"},
            false,
            10000,
            [this](const std::map<std::string,std::string>& args) -> ToolResult {
                auto it = args.find("path");
                if (it == args.end()) return ToolResult::fail("Missing path");
                std::string full = file_mgr.resolve_path(it->second);
                if (!utils::file_exists(full)) return ToolResult::fail("File not found: " + full);
                std::string report = code::Analyzer::generate_report(full, true);
                return ToolResult::ok(report);
            }
        };

        // ==================== SYSTEM TOOLS ====================
        tools["shell_exec"] = Tool{
            "shell_exec",
            "Выполняет shell команду в системе и возвращает вывод. ОЧЕНЬ МОЩНЫЙ инструмент. Вход: command (команда), timeout (опционально, секунды). Используй для любых задач: установка ПО, компиляция, запуск скриптов, анализ данных, git и т.д.",
            "{\"command\": \"ls -la\", \"timeout\": 30}",
            "system",
            {"shell_exec(command=\"ls -la\")", "shell_exec(command=\"python script.py\")", "shell_exec(command=\"g++ -o app main.cpp && ./app\")"},
            false,
            60000,
            [](const std::map<std::string,std::string>& args) -> ToolResult {
                auto it = args.find("command");
                if (it == args.end()) return ToolResult::fail("Missing command");
                int timeout = 30;
                auto it2 = args.find("timeout");
                if (it2 != args.end()) { try { timeout = std::stoi(it2->second); } catch(...) {} }
                std::string out = utils::shell_exec(it->second, timeout);
                return ToolResult::ok("Command: " + it->second + "\nOutput (" + std::to_string(out.size()) + " chars):\n" + out);
            }
        };

        tools["calculator"] = Tool{
            "calculator",
            "Вычисляет математическое выражение. Поддерживает + - * / % ^ sqrt sin cos log и т.д. Вход: expression. Для сложных вычислений используй python через shell_exec.",
            "{\"expression\": \"2+2*2\"}",
            "system",
            {"calculator(expression=\"2+2*2\")", "calculator(expression=\"sqrt(16) + sin(0.5)\")"},
            false,
            5000,
            [](const std::map<std::string,std::string>& args) -> ToolResult {
                auto it = args.find("expression");
                if (it == args.end()) return ToolResult::fail("Missing expression");
                std::string expr = it->second;
                // Try python
                std::string cmd = "python3 -c \"import math; print(" + expr + ")\" 2>&1";
                std::string out = utils::shell_exec(cmd, 5);
                if (out.find("ModuleNotFound") != std::string::npos || out.find("SyntaxError") != std::string::npos || out.empty() || out.find("NameError") != std::string::npos) {
                    cmd = "python -c \"import math; print(" + expr + ")\" 2>&1";
                    out = utils::shell_exec(cmd, 5);
                }
                if (out.empty()) out = "Could not calculate, try shell_exec with python";
                return ToolResult::ok("Calculator result for '" + expr + "': " + utils::trim(out));
            }
        };

        tools["datetime"] = Tool{
            "datetime",
            "Возвращает текущую дату и время, день недели, timestamp. Без параметров.",
            "{}",
            "system",
            {"datetime()"},
            false,
            1000,
            [](const std::map<std::string,std::string>&) -> ToolResult {
                auto now = std::chrono::system_clock::now();
                std::time_t t = std::chrono::system_clock::to_time_t(now);
                char buf[128];
                std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %A (UTC%z)", std::localtime(&t));
                long long ts = utils::timestamp_ms();
                std::ostringstream oss;
                oss << "Current datetime: " << buf << "\n";
                oss << "Timestamp ms: " << ts << "\n";
                oss << "ISO: " << utils::now_iso() << "\n";
                return ToolResult::ok(oss.str());
            }
        };

        // ==================== MEMORY TOOLS ====================
        tools["memory_store"] = Tool{
            "memory_store",
            "Сохраняет информацию в долговременную память агента. Вход: key (ключ), value (значение), type (опционально: general, user, fact, preference). Память сохраняется между сессиями.",
            "{\"key\": \"user_name\", \"value\": \"Alex\", \"type\": \"user\"}",
            "memory",
            {"memory_store(key=\"user_name\", value=\"Alex\")", "memory_store(key=\"project\", value=\"MyApp\", type=\"fact\")"},
            false,
            5000,
            [this](const std::map<std::string,std::string>& args) -> ToolResult {
                auto k = args.find("key");
                auto v = args.find("value");
                if (k==args.end() || v==args.end()) return ToolResult::fail("Missing key/value");
                std::string type = "general";
                auto it = args.find("type");
                if (it != args.end()) type = it->second;
                memory.store(k->second, v->second, type);
                return ToolResult::ok("Stored in memory: " + k->second + " = " + v->second + " [" + type + "]");
            }
        };

        tools["memory_recall"] = Tool{
            "memory_recall",
            "Вспоминает информацию из долговременной памяти. Вход: key (ключ, если пустой - возвращает всю память), type (фильтр по типу).",
            "{\"key\": \"\", \"type\": \"\"}",
            "memory",
            {"memory_recall(key=\"user_name\")", "memory_recall(key=\"\", type=\"user\")"},
            false,
            5000,
            [this](const std::map<std::string,std::string>& args) -> ToolResult {
                auto it = args.find("key");
                std::string key = it != args.end() ? it->second : "";
                std::string type = "";
                auto it2 = args.find("type");
                if (it2 != args.end()) type = it2->second;
                
                if (key.empty()) {
                    auto items = type.empty() ? memory.get_all() : memory.get_by_type(type);
                    if (items.empty()) return ToolResult::ok("Memory is empty" + (type.empty() ? "" : " for type " + type));
                    std::ostringstream oss;
                    oss << "Memory contents (" << items.size() << " items" << (type.empty() ? "" : " type=" + type) << "):\n";
                    for (auto& item : items) oss << "  " << item.to_string() << "\n";
                    return ToolResult::ok(oss.str());
                }
                auto val = memory.recall(key);
                if (val.empty()) return ToolResult::ok("Memory key not found: " + key);
                return ToolResult::ok(key + " = " + val);
            }
        };

        tools["memory_search"] = Tool{
            "memory_search",
            "Ищет в памяти по запросу. Вход: query (поисковый запрос).",
            "{\"query\": \"user\"}",
            "memory",
            {"memory_search(query=\"Alex\")"},
            false,
            5000,
            [this](const std::map<std::string,std::string>& args) -> ToolResult {
                auto it = args.find("query");
                if (it == args.end()) return ToolResult::fail("Missing query");
                auto results = memory.search(it->second);
                if (results.empty()) return ToolResult::ok("No memory items found for: " + it->second);
                std::ostringstream oss;
                oss << "Memory search for '" << it->second << "' (" << results.size() << " results):\n";
                for (auto& item : results) oss << "  " << item.to_string() << "\n";
                return ToolResult::ok(oss.str());
            }
        };

        // ==================== REASONING TOOLS ====================
        tools["think"] = Tool{
            "think",
            "Инструмент для глубокого размышления. Используй его чтобы продумать план, проанализировать проблему, разбить задачу на подзадачи, провести рефлексию. Вход: thought (твои размышления), type (опционально: analytical, critical, strategic, creative).",
            "{\"thought\": \"размышления...\", \"type\": \"analytical\"}",
            "reasoning",
            {"think(thought=\"Нужно разбить задачу на 3 шага...\")", "think(thought=\"Проверяю свой план...\", type=\"critical\")"},
            false,
            5000,
            [](const std::map<std::string,std::string>& args) -> ToolResult {
                auto it = args.find("thought");
                std::string thought = it != args.end() ? it->second : "";
                std::string type = "analytical";
                auto it2 = args.find("type");
                if (it2 != args.end()) type = it2->second;
                std::ostringstream oss;
                oss << "Thought recorded [" << type << "]: " << thought << "\n";
                oss << "Продолжай рассуждать и действовать. Если нужно, вызови следующий инструмент.";
                return ToolResult::ok(oss.str());
            }
        };

        tools["plan"] = Tool{
            "plan",
            "Создает план выполнения задачи. Вход: goal (цель), context (контекст). Возвращает структурированный план с шагами.",
            "{\"goal\": \"Создать веб-сервер\", \"context\": \"На Python Flask\"}",
            "reasoning",
            {"plan(goal=\"Создать игру\")"},
            false,
            5000,
            [](const std::map<std::string,std::string>& args) -> ToolResult {
                auto it = args.find("goal");
                if (it == args.end()) return ToolResult::fail("Missing goal");
                std::string goal = it->second;
                std::string context = "";
                auto it2 = args.find("context");
                if (it2 != args.end()) context = it2->second;
                
                std::ostringstream oss;
                oss << "Plan for: " << goal << "\n";
                if (!context.empty()) oss << "Context: " << context << "\n";
                oss << "\nSteps:\n";
                
                std::string lower = utils::to_lower(goal);
                std::vector<std::string> steps;
                if (lower.find("код") != std::string::npos || lower.find("code") != std::string::npos) {
                    steps = {"1. Analyze requirements", "2. Design architecture", "3. Write code", "4. Test", "5. Fix & document"};
                } else if (lower.find("найди") != std::string::npos || lower.find("search") != std::string::npos) {
                    steps = {"1. Understand query", "2. Web search", "3. Fetch pages", "4. Analyze", "5. Report"};
                } else {
                    steps = {"1. Analyze task", "2. Break into subtasks", "3. Execute step by step", "4. Verify", "5. Final answer"};
                }
                
                for (auto& s : steps) oss << s << "\n";
                return ToolResult::ok(oss.str());
            }
        };
    }

    std::vector<Tool> list() const {
        std::vector<Tool> out;
        for (auto& kv : tools) out.push_back(kv.second);
        return out;
    }

    std::vector<Tool> list_by_category(const std::string& cat) const {
        std::vector<Tool> out;
        for (auto& kv : tools) if (kv.second.category == cat) out.push_back(kv.second);
        return out;
    }

    bool has(const std::string& name) const { return tools.find(name) != tools.end(); }

    Tool get(const std::string& name) const {
        auto it = tools.find(name);
        return it == tools.end() ? Tool{} : it->second;
    }

    ToolResult execute(const std::string& name, const std::map<std::string,std::string>& args) {
        auto it = tools.find(name);
        if (it == tools.end()) return ToolResult::fail("Unknown tool: " + name);
        try {
            auto start = std::chrono::steady_clock::now();
            auto result = it->second.func(args);
            auto end = std::chrono::steady_clock::now();
            result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
            return result;
        } catch (std::exception& e) {
            return ToolResult::fail("Tool exception: " + std::string(e.what()));
        }
    }

    std::string get_tools_prompt() const {
        std::ostringstream oss;
        oss << "Доступные инструменты (используй их для достижения цели):\n\n";
        std::map<std::string, std::vector<Tool>> by_cat;
        for (auto& kv : tools) by_cat[kv.second.category].push_back(kv.second);
        
        for (auto& cat : by_cat) {
            oss << "## " << cat.first << ":\n";
            for (auto& tool : cat.second) {
                oss << "- **" << tool.name << "**: " << tool.description << "\n";
                oss << "  Параметры: " << tool.parameters << "\n";
                if (!tool.examples.empty()) {
                    oss << "  Примеры: " << tool.examples[0] << "\n";
                }
            }
            oss << "\n";
        }
        oss << "\nФормат вызова инструмента:\n";
        oss << "```tool\n{\"name\": \"tool_name\", \"arguments\": {\"param\": \"value\"}}\n```\n";
        oss << "Или: ACTION: tool_name | ARGS: {\"param\":\"value\"}\n";
        oss << "После вызова ты получишь OBSERVATION с результатом.\n";
        return oss.str();
    }

    std::string get_tools_list() const {
        std::ostringstream oss;
        oss << "Available tools (" << tools.size() << "):\n";
        for (auto& kv : tools) {
            oss << "  " << utils::pad_right(kv.first, 20) << " [" << kv.second.category << "] " << kv.second.description.substr(0,60) << "\n";
        }
        return oss.str();
    }

    size_t count() const { return tools.size(); }
};

// Tool call extraction - improved
struct ToolCall {
    std::string name;
    std::map<std::string,std::string> args;
    std::string raw;
    double confidence = 1.0;
};

inline std::map<std::string,std::string> parse_tool_args(const std::string& json_str) {
    std::map<std::string,std::string> out;
    auto j = mini_json::parse_json(json_str);
    if (j.is_object()) {
        for (auto& kv : j.as_object()) {
            if (kv.second.is_string()) out[kv.first] = kv.second.as_string();
            else if (kv.second.is_number()) out[kv.first] = std::to_string(kv.second.as_number());
            else if (kv.second.is_bool()) out[kv.first] = kv.second.as_bool() ? "true" : "false";
            else out[kv.first] = kv.second.to_string();
        }
    }
    return out;
}

inline std::vector<ToolCall> extract_tool_calls(const std::string& text) {
    std::vector<ToolCall> calls;
    size_t pos = 0;
    
    while (true) {
        // Pattern 1: ```tool\n{...}\n```
        size_t start_tool = text.find("```tool", pos);
        size_t start_json = text.find("```json", pos);
        size_t start2 = std::string::npos;
        bool is_tool_block = false;
        
        if (start_tool != std::string::npos && (start_json == std::string::npos || start_tool < start_json)) {
            start2 = start_tool;
            is_tool_block = true;
        } else if (start_json != std::string::npos) {
            // Check if json block contains tool call
            size_t json_content_start = text.find('{', start_json);
            size_t json_block_end = text.find("```", json_content_start);
            if (json_content_start != std::string::npos && json_block_end != std::string::npos) {
                std::string potential = text.substr(json_content_start, json_block_end - json_content_start);
                if (potential.find("\"name\"") != std::string::npos || potential.find("\"tool\"") != std::string::npos) {
                    start2 = start_json;
                    is_tool_block = true;
                }
            }
        }
        
        if (is_tool_block) {
            size_t json_start = text.find('{', start2);
            size_t block_end = text.find("```", json_start);
            if (json_start == std::string::npos || block_end == std::string::npos) break;
            
            std::string json_str = text.substr(json_start, block_end - json_start);
            int depth=0;
            size_t end_pos=0;
            for (size_t i=0;i<json_str.size();++i) {
                if (json_str[i]=='{') depth++;
                else if (json_str[i]=='}') { depth--; if (depth==0) { end_pos=i; break; } }
            }
            if (end_pos>0) json_str = json_str.substr(0,end_pos+1);
            
            auto j = mini_json::parse_json(json_str);
            if (j.is_object() && (j.contains("name") || j.contains("tool"))) {
                ToolCall tc;
                tc.name = j.contains("name") ? j.get_string("name") : j.get_string("tool");
                tc.raw = json_str;
                tc.confidence = 0.95;
                
                if (j.contains("arguments") && j["arguments"].is_object()) {
                    for (auto& kv : j["arguments"].as_object()) {
                        if (kv.second.is_string()) tc.args[kv.first] = kv.second.as_string();
                        else tc.args[kv.first] = kv.second.to_string();
                    }
                } else if (j.contains("args") && j["args"].is_object()) {
                    for (auto& kv : j["args"].as_object()) {
                        if (kv.second.is_string()) tc.args[kv.first] = kv.second.as_string();
                        else tc.args[kv.first] = kv.second.to_string();
                    }
                } else {
                    for (auto& kv : j.as_object()) {
                        if (kv.first=="name" || kv.first=="tool") continue;
                        if (kv.second.is_string()) tc.args[kv.first] = kv.second.as_string();
                        else tc.args[kv.first] = kv.second.to_string();
                    }
                }
                calls.push_back(tc);
            }
            pos = block_end + 3;
            continue;
        }

        // Pattern 2: ACTION: tool_name | ARGS: {...}
        size_t action_pos = text.find("ACTION:", pos);
        if (action_pos != std::string::npos) {
            size_t name_start = action_pos + 7;
            size_t pipe = text.find("|", name_start);
            size_t args_pos = text.find("ARGS:", name_start);
            size_t nl = text.find("\n", name_start);
            
            std::string name;
            std::string args_str;
            
            if (pipe != std::string::npos && (args_pos == std::string::npos || pipe < args_pos)) {
                name = utils::trim(text.substr(name_start, pipe - name_start));
                size_t args_start = text.find('{', pipe);
                if (args_start != std::string::npos && (nl == std::string::npos || args_start < nl + 100)) {
                    int depth=0;
                    size_t end=0;
                    for (size_t i=args_start;i<text.size() && i<args_start+2000;++i) {
                        if (text[i]=='{') depth++;
                        else if (text[i]=='}') { depth--; if (depth==0) { end=i; break; } }
                    }
                    if (end>0) args_str = text.substr(args_start, end-args_start+1);
                }
            } else if (args_pos != std::string::npos) {
                name = utils::trim(text.substr(name_start, args_pos - name_start));
                size_t args_start = text.find('{', args_pos);
                if (args_start != std::string::npos) {
                    int depth=0;
                    size_t end=0;
                    for (size_t i=args_start;i<text.size() && i<args_start+2000;++i) {
                        if (text[i]=='{') depth++;
                        else if (text[i]=='}') { depth--; if (depth==0) { end=i; break; } }
                    }
                    if (end>0) args_str = text.substr(args_start, end-args_start+1);
                }
            }
            
            name = utils::trim(name);
            // Clean quotes and special chars
            name.erase(std::remove_if(name.begin(), name.end(), [](char c){ return c=='\"' || c=='\'' || c=='`'; }), name.end());
            name = utils::trim(name);
            
            if (!name.empty() && name.size() < 50) {
                ToolCall tc;
                tc.name = name;
                tc.raw = args_str;
                tc.args = parse_tool_args(args_str);
                tc.confidence = 0.8;
                calls.push_back(tc);
                pos = (args_pos != std::string::npos ? args_pos+5 : (pipe != std::string::npos ? pipe+1 : action_pos+7));
                continue;
            }
        }

        // Pattern 3: <tool_call> or [TOOL]
        break;
    }

    // Fallback: try to parse entire text as single JSON tool call
    if (calls.empty()) {
        std::string trimmed = utils::trim(text);
        if (utils::starts_with(trimmed, "{") && utils::ends_with(trimmed, "}")) {
            auto j = mini_json::parse_json(trimmed);
            if (j.is_object() && j.contains("name")) {
                ToolCall tc;
                tc.name = j.get_string("name");
                tc.confidence = 0.7;
                if (j.contains("arguments") && j["arguments"].is_object()) {
                    for (auto& kv : j["arguments"].as_object()) {
                        tc.args[kv.first] = kv.second.is_string() ? kv.second.as_string() : kv.second.to_string();
                    }
                }
                if (!tc.name.empty()) calls.push_back(tc);
            }
        }
    }

    return calls;
}

inline Registry& global_tools() {
    static Registry reg;
    return reg;
}

}

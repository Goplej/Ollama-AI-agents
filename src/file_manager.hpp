#pragma once
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "utils.hpp"
#include "logger.hpp"
#include "theme.hpp"
#include "encoding.hpp"

namespace filemgr {

struct FileInfo {
    std::string path;
    std::string name;
    std::string extension;
    long long size = 0;
    bool is_dir = false;
    bool is_file = false;
    std::string modified;

    static FileInfo from_path(const std::filesystem::directory_entry& entry) {
        FileInfo info;
        info.path = entry.path().string();
        info.name = entry.path().filename().string();
        info.extension = entry.path().extension().string();
        info.is_dir = entry.is_directory();
        info.is_file = entry.is_regular_file();
        try {
            if (info.is_file) info.size = entry.file_size();
            auto ftime = entry.last_write_time();
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
            std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
            char buf[64];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&tt));
            info.modified = buf;
        } catch(...) {}
        return info;
    }

    std::string size_human() const {
        if (is_dir) return "<DIR>";
        if (size < 1024) return std::to_string(size) + " B";
        if (size < 1024*1024) return std::to_string(size/1024) + " KB";
        if (size < 1024*1024*1024) return std::to_string(size/(1024*1024)) + " MB";
        return std::to_string(size/(1024*1024*1024)) + " GB";
    }

    std::string icon(bool unicode) const {
        if (is_dir) return unicode ? "[DIR]" : "[DIR]";
        if (extension == ".cpp" || extension == ".hpp") return "[CPP]";
        if (extension == ".py") return "[PY]";
        if (extension == ".js" || extension == ".ts") return "[JS]";
        if (extension == ".json") return "[JSON]";
        if (extension == ".md") return "[MD]";
        return "[FILE]";
    }
};

class FileManager {
    std::string current_dir;
    std::vector<std::string> history;
    size_t history_pos = 0;
    std::map<std::string, std::string> file_cache;
    size_t cache_max = 50;
    bool use_unicode = false;

public:
    FileManager(const std::string& start_dir=".") : current_dir(std::filesystem::absolute(start_dir).string()) {
        history.push_back(current_dir);
        use_unicode = encoding::is_unicode_supported();
    }

    std::string get_current() const { return current_dir; }

    bool change_dir(const std::string& path) {
        try {
            std::string new_path = path;
            if (new_path == "~") new_path = std::getenv("HOME") ? std::getenv("HOME") : ".";
            if (new_path == "..") new_path = std::filesystem::path(current_dir).parent_path().string();
            else if (!std::filesystem::path(new_path).is_absolute()) new_path = (std::filesystem::path(current_dir) / new_path).string();
            
            new_path = std::filesystem::canonical(new_path).string();
            if (!std::filesystem::is_directory(new_path)) return false;
            
            current_dir = new_path;
            history.push_back(current_dir);
            history_pos = history.size()-1;
            return true;
        } catch(...) { return false; }
    }

    std::vector<FileInfo> list(const std::string& dir="", bool show_hidden=false, const std::string& filter="") const {
        std::string target = dir.empty() ? current_dir : dir;
        std::vector<FileInfo> files;
        try {
            for (auto& entry : std::filesystem::directory_iterator(target)) {
                std::string name = entry.path().filename().string();
                if (!show_hidden && !name.empty() && name[0]=='.') continue;
                if (!filter.empty() && name.find(filter) == std::string::npos) continue;
                files.push_back(FileInfo::from_path(entry));
            }
            std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b){
                if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
                return utils::to_lower(a.name) < utils::to_lower(b.name);
            });
        } catch(...) {}
        return files;
    }

    std::vector<FileInfo> list_recursive(const std::string& dir="", int max_depth=3, const std::string& ext_filter="") const {
        std::string target = dir.empty() ? current_dir : dir;
        std::vector<FileInfo> files;
        try {
            for (auto& entry : std::filesystem::recursive_directory_iterator(target)) {
                auto rel = std::filesystem::relative(entry.path(), target);
                int cur_depth = std::distance(rel.begin(), rel.end()) - 1;
                if (cur_depth > max_depth) continue;
                if (entry.is_regular_file()) {
                    if (!ext_filter.empty() && entry.path().extension() != ext_filter) continue;
                    files.push_back(FileInfo::from_path(entry));
                }
                if (files.size() > 1000) break;
            }
        } catch(...) {}
        return files;
    }

    std::string read(const std::string& path, size_t max_size=50000) {
        std::string full = resolve_path(path);
        auto it = file_cache.find(full);
        if (it != file_cache.end()) return it->second;
        if (!utils::file_exists(full)) return "";
        long long sz = utils::file_size(full);
        if (sz > (long long)max_size*2) {
            std::ifstream f(full, std::ios::binary);
            std::string content(max_size, '\0');
            f.read(&content[0], max_size);
            content.resize(f.gcount());
            content += "\n...[truncated, file is " + std::to_string(sz) + " bytes]";
            cache_file(full, content);
            return content;
        }
        std::string content = utils::read_file(full);
        if (content.size() > max_size) content = content.substr(0, max_size) + "\n...[truncated]";
        cache_file(full, content);
        return content;
    }

    bool write(const std::string& path, const std::string& content, bool backup=true) {
        std::string full = resolve_path(path);
        try {
            if (backup && utils::file_exists(full)) {
                std::string backup_path = full + ".bak";
                std::filesystem::copy_file(full, backup_path, std::filesystem::copy_options::overwrite_existing);
            }
            bool ok = utils::write_file(full, content);
            if (ok) file_cache.erase(full);
            return ok;
        } catch(...) { return false; }
    }

    bool remove(const std::string& path) {
        std::string full = resolve_path(path);
        try {
            bool ok = std::filesystem::remove(full);
            if (ok) file_cache.erase(full);
            return ok;
        } catch(...) { return false; }
    }

    bool exists(const std::string& path) const {
        return utils::file_exists(resolve_path(path));
    }

    std::string resolve_path(const std::string& path) const {
        if (path.empty()) return current_dir;
        std::filesystem::path p(path);
        if (p.is_absolute()) return p.string();
        return (std::filesystem::path(current_dir) / p).string();
    }

    std::vector<std::pair<std::string,int>> search_in_files(const std::string& query, const std::string& dir="", const std::string& ext="") {
        std::string target = dir.empty() ? current_dir : dir;
        std::vector<std::pair<std::string,int>> results;
        auto files = list_recursive(target, 5, ext);
        for (auto& fi : files) {
            try {
                std::string content = utils::read_file(fi.path);
                if (content.find(query) != std::string::npos) {
                    int count = 0;
                    size_t pos=0;
                    while ((pos=content.find(query, pos)) != std::string::npos) { count++; pos+=query.size(); }
                    results.push_back({fi.path, count});
                }
            } catch(...) {}
            if (results.size() > 100) break;
        }
        std::sort(results.begin(), results.end(), [](auto& a, auto& b){ return a.second > b.second; });
        return results;
    }

    std::string get_tree(const std::string& dir="", int max_depth=3) const {
        std::string target = dir.empty() ? current_dir : dir;
        std::ostringstream oss;
        std::function<void(const std::string&, int, const std::string&)> walk = [&](const std::string& d, int depth, const std::string& prefix){
            if (depth > max_depth) return;
            try {
                auto entries = list(d);
                for (size_t i=0;i<entries.size();++i) {
                    auto& e = entries[i];
                    bool is_last = i+1==entries.size();
                    oss << prefix << (is_last ? "+-- " : "|-- ") << e.name;
                    if (e.is_file) oss << " (" << e.size_human() << ")";
                    oss << "\n";
                    if (e.is_dir && depth+1 <= max_depth) {
                        walk(e.path, depth+1, prefix + (is_last ? "    " : "|   "));
                    }
                }
            } catch(...) {}
        };
        oss << target << "\n";
        walk(target, 1, "");
        return oss.str();
    }

private:
    void cache_file(const std::string& path, const std::string& content) {
        if (file_cache.size() >= cache_max) file_cache.erase(file_cache.begin());
        file_cache[path] = content;
    }
};

inline FileManager& global_file_manager() {
    static FileManager fm(".");
    return fm;
}

}

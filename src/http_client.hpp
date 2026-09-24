#pragma once
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>
#include <functional>
#include "utils.hpp"
#include "logger.hpp"
#include "encoding.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
// Fix ERROR macro conflict
#ifdef ERROR
#undef ERROR
#endif
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#endif

namespace http {

struct Response {
    int status_code = 0;
    std::string body;
    std::map<std::string, std::string> headers;
    bool success = false;
    std::string error;
    long long elapsed_ms = 0;
    std::string url;

    bool is_json() const {
        auto it = headers.find("content-type");
        if (it != headers.end()) return it->second.find("json") != std::string::npos;
        return !body.empty() && (body[0]=='{' || body[0]=='[');
    }

    bool is_html() const {
        auto it = headers.find("content-type");
        if (it != headers.end()) return it->second.find("html") != std::string::npos;
        return false;
    }
};

struct Url {
    std::string scheme;
    std::string host;
    int port = 80;
    std::string path;
    std::string query;
    std::string fragment;

    std::string to_string() const {
        std::string s = scheme + "://" + host;
        if ((scheme=="http" && port!=80) || (scheme=="https" && port!=443)) s += ":" + std::to_string(port);
        s += path;
        if (!query.empty()) s += "?" + query;
        if (!fragment.empty()) s += "#" + fragment;
        return s;
    }

    bool is_https() const { return scheme == "https"; }
};

inline Url parse_url(const std::string& url_str) {
    // Fix 0.0.0.0 before parsing
    std::string fixed_input = encoding::fix_host(url_str);
    Url u;
    std::string s = fixed_input;
    size_t p = s.find("://");
    if (p != std::string::npos) {
        u.scheme = s.substr(0, p);
        s = s.substr(p+3);
    } else {
        u.scheme = "http";
    }
    size_t hash = s.find('#');
    if (hash != std::string::npos) {
        u.fragment = s.substr(hash+1);
        s = s.substr(0, hash);
    }
    size_t slash = s.find('/');
    std::string hostport;
    if (slash != std::string::npos) {
        hostport = s.substr(0, slash);
        u.path = s.substr(slash);
    } else {
        hostport = s;
        u.path = "/";
    }
    size_t q = u.path.find('?');
    if (q != std::string::npos) {
        u.query = u.path.substr(q+1);
        u.path = u.path.substr(0, q);
    }
    size_t colon = hostport.find(':');
    if (colon != std::string::npos) {
        u.host = hostport.substr(0, colon);
        try { u.port = std::stoi(hostport.substr(colon+1)); } catch(...) { u.port = 80; }
    } else {
        u.host = hostport;
        u.port = (u.scheme == "https") ? 443 : 80;
    }
    // Final safety: 0.0.0.0 is not connectable, use 127.0.0.1
    if (u.host == "0.0.0.0") u.host = "127.0.0.1";
    if (u.path.empty()) u.path = "/";
    return u;
}

struct RequestOptions {
    int timeout_ms = 15000;
    int max_retries = 2;
    bool follow_redirects = true;
    int max_redirects = 5;
    std::string user_agent = "OllamaAgent/3.0 Professional";
    bool verify_ssl = false;
    std::map<std::string,std::string> headers;
    std::function<void(size_t,size_t)> progress_callback;
};

#ifdef _WIN32
inline std::string winhttp_error_string(DWORD err) {
    switch(err) {
        case 12029: return "Cannot connect (12029). Ollama not running? Use 127.0.0.1 not 0.0.0.0. Check ollama serve";
        case 12007: return "Name not resolved (12007)";
        case 12002: return "Timeout (12002)";
        case 12030: return "Connection aborted (12030)";
        case 12031: return "Connection reset (12031)";
        case 12152: return "Invalid response (12152)";
        default: return "WinHTTP error " + std::to_string(err);
    }
}
#endif

#ifdef _WIN32
inline Response request_winhttp(const std::string& method, const std::string& url_str, const std::string& body = "", const RequestOptions& opts = RequestOptions()) {
    Response resp;
    resp.url = url_str;
    auto start = std::chrono::steady_clock::now();
    Url u = parse_url(url_str); // parse_url already fixes 0.0.0.0
    std::wstring host_w(u.host.begin(), u.host.end());
    std::wstring path_w;
    std::string full_path = u.path + (u.query.empty() ? "" : "?" + u.query);
    path_w = std::wstring(full_path.begin(), full_path.end());
    std::wstring method_w(method.begin(), method.end());

    HINTERNET hSession = WinHttpOpen(L"OllamaAgent/3.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { resp.error = "WinHttpOpen failed"; return resp; }
    WinHttpSetTimeouts(hSession, opts.timeout_ms, opts.timeout_ms, opts.timeout_ms, opts.timeout_ms);

    HINTERNET hConnect = WinHttpConnect(hSession, host_w.c_str(), u.port, 0);
    if (!hConnect) {
        DWORD err = GetLastError();
        resp.error = "WinHttpConnect failed to " + u.host + ":" + std::to_string(u.port) + " - " + winhttp_error_string(err) + " (" + std::to_string(err) + ")";
        WinHttpCloseHandle(hSession);
        return resp;
    }

    DWORD flags = (u.scheme == "https") ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, method_w.c_str(), path_w.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        DWORD err = GetLastError();
        resp.error = "WinHttpOpenRequest failed: " + winhttp_error_string(err) + " (" + std::to_string(err) + ")";
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return resp;
    }

    std::wstring headers_w;
    headers_w += L"User-Agent: " + std::wstring(opts.user_agent.begin(), opts.user_agent.end()) + L"\r\n";
    for (auto& kv : opts.headers) {
        std::wstring k(kv.first.begin(), kv.first.end());
        std::wstring v(kv.second.begin(), kv.second.end());
        headers_w += k + L": " + v + L"\r\n";
    }
    if (!headers_w.empty()) WinHttpAddRequestHeaders(hRequest, headers_w.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

    if (u.scheme == "https" && !opts.verify_ssl) {
        DWORD sec_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &sec_flags, sizeof(sec_flags));
    }

    DWORD redirect_policy = opts.follow_redirects ? WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS : WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));

    BOOL bResults = FALSE;
    if (body.empty()) {
        bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    } else {
        DWORD len = (DWORD)body.size();
        bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body.c_str(), len, len, 0);
    }
    if (!bResults) {
        DWORD err = GetLastError();
        resp.error = "WinHttpSendRequest failed: " + winhttp_error_string(err) + " [" + std::to_string(err) + "] URL=" + url_str + " Host=" + u.host + ":" + std::to_string(u.port) + ". TIP: If using 0.0.0.0, use 127.0.0.1 instead. Ensure ollama serve is running.";
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return resp;
    }

    bResults = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResults) {
        DWORD err = GetLastError();
        resp.error = "WinHttpReceiveResponse failed: " + winhttp_error_string(err) + " (" + std::to_string(err) + ")";
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return resp;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &statusSize, NULL);
    resp.status_code = (int)status;

    DWORD header_size = 0;
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, NULL, &header_size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && header_size > 0) {
        std::vector<wchar_t> header_buf(header_size/sizeof(wchar_t)+1);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, header_buf.data(), &header_size, WINHTTP_NO_HEADER_INDEX)) {
            std::wstring headers_wstr(header_buf.data());
            std::string headers_str(headers_wstr.begin(), headers_wstr.end());
            std::istringstream hs(headers_str);
            std::string line;
            while (std::getline(hs, line)) {
                line = utils::trim(line);
                if (line.empty()) continue;
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string k = utils::to_lower(utils::trim(line.substr(0, colon)));
                    std::string v = utils::trim(line.substr(colon+1));
                    resp.headers[k] = v;
                }
            }
        }
    }

    std::string out;
    out.reserve(8192);
    DWORD dwSize = 0;
    size_t total_downloaded = 0;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;
        std::vector<char> buf(dwSize);
        DWORD dwDownloaded = 0;
        if (!WinHttpReadData(hRequest, buf.data(), dwSize, &dwDownloaded)) break;
        out.append(buf.data(), dwDownloaded);
        total_downloaded += dwDownloaded;
        if (opts.progress_callback) opts.progress_callback(total_downloaded, 0);
        if (out.size() > 20*1024*1024) break;
    } while (dwSize > 0);

    resp.body = out;
    resp.success = (resp.status_code >= 200 && resp.status_code < 300);

    auto end = std::chrono::steady_clock::now();
    resp.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return resp;
}
#else
inline Response request_socket(const std::string& method, const std::string& url_str, const std::string& body, const RequestOptions& opts) {
    Response resp;
    resp.url = url_str;
    auto start = std::chrono::steady_clock::now();
    Url u = parse_url(url_str);
    
    if (u.scheme == "https") {
        std::string cmd = "curl -s -k -L --max-time " + std::to_string(opts.timeout_ms/1000+5) + " -X " + method + " ";
        cmd += "-A '" + opts.user_agent + "' ";
        for (auto& kv : opts.headers) {
            cmd += "-H '" + kv.first + ": " + kv.second + "' ";
        }
        if (!body.empty()) {
            std::string tmp = "/tmp/curl_body_" + utils::random_id();
            utils::write_file(tmp, body);
            cmd += "--data-binary @" + tmp + " ";
        }
        cmd += "-w '\\n%{http_code}' ";
        cmd += "'" + url_str + "' 2>&1";
        std::string out = utils::shell_exec(cmd, opts.timeout_ms/1000+10);
        utils::shell_exec("rm -f /tmp/curl_body_* 2>/dev/null");
        
        auto lines = utils::split_lines(out);
        if (!lines.empty()) {
            std::string last = utils::trim(lines.back());
            try {
                int code = std::stoi(last);
                if (code >= 100 && code < 600) {
                    resp.status_code = code;
                    lines.pop_back();
                    out = utils::join(lines, "\n");
                } else {
                    resp.status_code = 200;
                }
            } catch(...) {
                resp.status_code = 200;
            }
        }
        
        if (out.rfind("curl:",0)==0) {
            resp.error = "curl failed: " + out.substr(0,500);
            return resp;
        }
        resp.body = out;
        resp.success = resp.status_code >= 200 && resp.status_code < 300;
        auto end = std::chrono::steady_clock::now();
        resp.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
        return resp;
    }

    struct addrinfo hints{}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    std::string port_str = std::to_string(u.port);
    if (getaddrinfo(u.host.c_str(), port_str.c_str(), &hints, &res) != 0) {
        resp.error = "DNS resolve failed for " + u.host;
        return resp;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        resp.error = "socket creation failed";
        return resp;
    }

    struct timeval tv;
    tv.tv_sec = opts.timeout_ms/1000;
    tv.tv_usec = (opts.timeout_ms%1000)*1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        close(sock);
        freeaddrinfo(res);
        resp.error = "connect failed to " + u.host + ":" + port_str + ". TIP: ensure ollama serve running, try 127.0.0.1 not 0.0.0.0";
        return resp;
    }
    freeaddrinfo(res);

    std::string full_path = u.path + (u.query.empty() ? "" : "?" + u.query);
    std::ostringstream req;
    req << method << " " << full_path << " HTTP/1.1\r\n";
    req << "Host: " << u.host << "\r\n";
    req << "User-Agent: " << opts.user_agent << "\r\n";
    req << "Connection: close\r\n";
    req << "Accept: */*\r\n";
    for (auto& kv : opts.headers) {
        req << kv.first << ": " << kv.second << "\r\n";
    }
    if (!body.empty()) {
        req << "Content-Length: " << body.size() << "\r\n";
    }
    req << "\r\n";
    if (!body.empty()) req << body;

    std::string req_str = req.str();
    ssize_t sent = send(sock, req_str.c_str(), req_str.size(), 0);
    if (sent < 0) {
        close(sock);
        resp.error = "send failed";
        return resp;
    }

    std::string raw;
    raw.reserve(8192);
    char buf[4096];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
        raw.append(buf, n);
        if (raw.size() > 20*1024*1024) break;
    }
    close(sock);

    if (raw.empty()) {
        resp.error = "empty response";
        return resp;
    }

    size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        resp.error = "invalid http response";
        resp.body = raw;
        return resp;
    }

    std::string header_part = raw.substr(0, header_end);
    resp.body = raw.substr(header_end+4);

    std::istringstream hs(header_part);
    std::string line;
    if (std::getline(hs, line)) {
        std::istringstream ls(line);
        std::string ver;
        ls >> ver >> resp.status_code;
    }
    while (std::getline(hs, line)) {
        if (line.empty() || line=="\r") continue;
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string k = utils::to_lower(utils::trim(line.substr(0, colon)));
            std::string v = utils::trim(line.substr(colon+1));
            resp.headers[k] = v;
        }
    }

    auto it = resp.headers.find("transfer-encoding");
    if (it != resp.headers.end() && it->second.find("chunked") != std::string::npos) {
        std::string decoded;
        std::istringstream chunk_stream(resp.body);
        while (true) {
            std::string size_line;
            if (!std::getline(chunk_stream, size_line)) break;
            size_line = utils::trim(size_line);
            if (size_line.empty()) continue;
            size_t chunk_size = 0;
            try { chunk_size = std::stoul(size_line, nullptr, 16); } catch(...) { break; }
            if (chunk_size == 0) break;
            std::vector<char> cbuf(chunk_size);
            chunk_stream.read(cbuf.data(), chunk_size);
            decoded.append(cbuf.data(), chunk_stream.gcount());
            std::string crlf;
            std::getline(chunk_stream, crlf);
        }
        resp.body = decoded;
    }

    resp.success = (resp.status_code >= 200 && resp.status_code < 300);
    auto end = std::chrono::steady_clock::now();
    resp.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
    return resp;
}
#endif

inline Response get(const std::string& url, const std::map<std::string,std::string>& headers = {}, int timeout_ms=15000) {
    RequestOptions opts;
    opts.timeout_ms = timeout_ms;
    opts.headers = headers;
#ifdef _WIN32
    return request_winhttp("GET", url, "", opts);
#else
    return request_socket("GET", url, "", opts);
#endif
}

inline Response post(const std::string& url, const std::string& body, const std::map<std::string,std::string>& headers = {}, int timeout_ms=60000) {
    RequestOptions opts;
    opts.timeout_ms = timeout_ms;
    opts.headers = headers;
#ifdef _WIN32
    return request_winhttp("POST", url, body, opts);
#else
    return request_socket("POST", url, body, opts);
#endif
}

inline Response request(const std::string& method, const std::string& url, const std::string& body="", const RequestOptions& opts=RequestOptions()) {
#ifdef _WIN32
    return request_winhttp(method, url, body, opts);
#else
    return request_socket(method, url, body, opts);
#endif
}

inline Response get_with_retry(const std::string& url, int retries=3, int timeout_ms=15000) {
    Response resp;
    for (int i=0;i<retries;i++) {
        resp = get(url, {}, timeout_ms);
        if (resp.success) return resp;
        if (i+1<retries) std::this_thread::sleep_for(std::chrono::milliseconds(500*(i+1)));
    }
    return resp;
}

}

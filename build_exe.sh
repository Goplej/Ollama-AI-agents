#!/bin/bash
set -e
echo "=== Building Windows .exe via Zig ==="

# Check if ziglang available
if python3 -m ziglang version >/dev/null 2>&1; then
    ZIG="python3 -m ziglang"
elif command -v zig >/dev/null 2>&1; then
    ZIG="zig"
else
    echo "Installing ziglang..."
    pip3 install --break-system-packages ziglang
    ZIG="python3 -m ziglang"
fi

echo "Zig version: $($ZIG version)"

echo "Compiling for Windows x86_64..."
$ZIG c++ -target x86_64-windows-gnu -O2 -std=c++17 -o ollama-agent.exe src/main.cpp -lwinhttp -lws2_32 -static

echo "Compiling for Linux x86_64..."
g++ -O2 -std=c++17 -o ollama-agent src/main.cpp -pthread

echo ""
echo "Build done:"
ls -lh ollama-agent.exe ollama-agent 2>&1
echo ""
echo "Check exe type:"
file ollama-agent.exe || echo "file command not available, but exe exists"
file ollama-agent || true
echo ""
echo "Run Linux: ./ollama-agent --help"
echo "Run Windows: ollama-agent.exe --help (on Windows)"

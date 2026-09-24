#!/bin/bash
set -e
echo "=== Building Ollama Super Agent (Linux) ==="
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
echo ""
echo "Build done: build/ollama-agent"
ls -lh ollama-agent
echo ""
echo "Run: ./ollama-agent --help"
echo "Or: ./ollama-agent --model llama3.1 --interactive"

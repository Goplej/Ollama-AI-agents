@echo off
echo === Building Ollama Super Agent (Windows) ===
mkdir build-win 2>nul
cd build-win
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -j4
echo.
echo Build done: build-win\ollama-agent.exe
dir ollama-agent.exe
echo.
echo Run: ollama-agent.exe --help
echo Or: ollama-agent.exe --model llama3.1 --interactive
pause

@echo off
g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -mwindows gui.cpp -o ASLS-calculator.exe
g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ main.cpp -o ASLS-console.exe

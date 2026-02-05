@echo off
windres app.rc -o app.o
compiler.exe config.c -l opengl32 -l gdi32 -l SDL3 -l SDL3_ttf -o bin/game.exe
pause
import os
import sys

if sys.platform == "win32":
    # os.system("gcc editor.c app.o -L./ -llibtcc -o dist/compiler.exe")
    os.system("gcc editor_gui.c include/glad/glad.c -o dist/editor.exe -Iinclude -L./ -lglfw3dll -lopengl32 -lgdi32 -static")
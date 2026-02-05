#include "tcc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <direct.h>
    #include <windows.h>
    #define mkdir(path, mode) _mkdir(path)
    #define popen _popen
    #define pclose _pclose
#else
    #include <unistd.h>
    #include <errno.h>
    #include <dirent.h>
#endif

// Структура для хранения списка файлов
typedef struct {
    char** files;
    int count;
    int capacity;
} FileList;

// Простая функция для проверки существования файла
int file_exists(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

// Проверка, является ли файл исходным кодом C
int is_source_file(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return 0;
    
    return (strcmp(ext, ".c") == 0) ||
           (strcmp(ext, ".cpp") == 0) ||
           (strcmp(ext, ".cc") == 0) ||
           (strcmp(ext, ".cxx") == 0);
}

// Проверка, является ли файл заголовочным
int is_header_file(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return 0;
    
    return (strcmp(ext, ".h") == 0) ||
           (strcmp(ext, ".hpp") == 0) ||
           (strcmp(ext, ".hh") == 0) ||
           (strcmp(ext, ".hxx") == 0);
}

// Добавление файла в список
void filelist_add(FileList* list, const char* file) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 16;
        list->files = realloc(list->files, list->capacity * sizeof(char*));
    }
    list->files[list->count] = strdup(file);
    list->count++;
}

// Освобождение списка файлов
void filelist_free(FileList* list) {
    for (int i = 0; i < list->count; i++) {
        free(list->files[i]);
    }
    free(list->files);
    list->files = NULL;
    list->count = 0;
    list->capacity = 0;
}

// Рекурсивный поиск исходных файлов
void find_source_files(const char* dir, FileList* sources) {
    #ifdef _WIN32
        char pattern[512];
        snprintf(pattern, sizeof(pattern), "%s\\*", dir);
        
        WIN32_FIND_DATA findData;
        HANDLE hFind = FindFirstFile(pattern, &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    // Пропускаем текущую и родительскую директории
                    if (strcmp(findData.cFileName, ".") == 0 || 
                        strcmp(findData.cFileName, "..") == 0) {
                        continue;
                    }
                    
                    // Рекурсивный поиск в поддиректории
                    char subdir[512];
                    snprintf(subdir, sizeof(subdir), "%s\\%s", dir, findData.cFileName);
                    find_source_files(subdir, sources);
                } else {
                    // Проверяем, является ли файл исходным кодом
                    if (is_source_file(findData.cFileName)) {
                        char fullpath[512];
                        snprintf(fullpath, sizeof(fullpath), "%s\\%s", dir, findData.cFileName);
                        filelist_add(sources, fullpath);
                    }
                }
            } while (FindNextFile(hFind, &findData));
            FindClose(hFind);
        }
    #else
        DIR* dp = opendir(dir);
        if (dp == NULL) return;
        
        struct dirent* entry;
        while ((entry = readdir(dp)) != NULL) {
            // Пропускаем скрытые файлы и директории
            if (entry->d_name[0] == '.') continue;
            
            char fullpath[512];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, entry->d_name);
            
            struct stat st;
            if (stat(fullpath, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    // Рекурсивный поиск в поддиректории
                    find_source_files(fullpath, sources);
                } else if (S_ISREG(st.st_mode)) {
                    // Проверяем, является ли файл исходным кодом
                    if (is_source_file(entry->d_name)) {
                        filelist_add(sources, fullpath);
                    }
                }
            }
        }
        closedir(dp);
    #endif
}

// Создание директории если её нет
void ensure_directory(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        #ifdef _WIN32
            _mkdir(path);
        #else
            mkdir(path, 0755);
        #endif
    }
}

// Функция для добавления ВСЕХ библиотек из папки lib
void add_all_library_files(TCCState* tcc, const char* lib_dir, int verbose) {
    if (verbose) printf("Looking for libraries in: %s\n", lib_dir);
    
    if (!file_exists(lib_dir)) {
        if (verbose) printf("  Directory does not exist\n");
        return;
    }
    
    // Создаем команду для поиска файлов библиотек
    char find_cmd[512];
    
    #ifdef _WIN32
        snprintf(find_cmd, sizeof(find_cmd), "dir /b \"%s\\*.lib\" \"%s\\*.a\" 2>nul", lib_dir, lib_dir);
        FILE* pipe = popen(find_cmd, "r");
    #else
        snprintf(find_cmd, sizeof(find_cmd), "ls \"%s\"/*.a \"%s\"/*.lib 2>/dev/null", lib_dir, lib_dir);
        FILE* pipe = popen(find_cmd, "r");
    #endif
    
    if (pipe) {
        char buffer[256];
        int count = 0;
        
        while (fgets(buffer, sizeof(buffer), pipe)) {
            buffer[strcspn(buffer, "\r\n")] = 0;
            
            if (buffer[0]) {
                char full_path[512];
                #ifdef _WIN32
                    snprintf(full_path, sizeof(full_path), "%s\\%s", lib_dir, buffer);
                #else
                    snprintf(full_path, sizeof(full_path), "%s/%s", lib_dir, buffer);
                #endif
                
                if (verbose) printf("  Adding: %s\n", buffer);
                
                if (tcc_add_file(tcc, full_path) != 0 && verbose) {
                    printf("  Warning: Failed to add %s\n", buffer);
                }
                count++;
            }
        }
        
        pclose(pipe);
        
        if (verbose && count == 0) {
            printf("  No library files found\n");
        }
    } else if (verbose) {
        printf("  Could not scan directory\n");
    }
}

// Добавление системных библиотек
void add_system_libraries(TCCState* tcc, int verbose) {
    if (verbose) printf("Adding system libraries\n");
    
    #ifdef _WIN32
        tcc_add_library(tcc, "kernel32");
        tcc_add_library(tcc, "user32");
        tcc_add_library(tcc, "gdi32");
        tcc_add_library(tcc, "opengl32");
        tcc_add_library(tcc, "winmm");
        tcc_add_library(tcc, "ws2_32");
    #else
        tcc_add_library(tcc, "m");
        tcc_add_library(tcc, "dl");
        tcc_add_library(tcc, "pthread");
        tcc_add_library(tcc, "GL");
        tcc_add_library(tcc, "X11");
        
        if (file_exists("/usr/lib/libglfw.so") || 
            file_exists("/usr/local/lib/libglfw.so") ||
            file_exists("/usr/lib/x86_64-linux-gnu/libglfw.so") ||
            file_exists("lib/libglfw.a") ||
            file_exists("lib/libglfw.so")) {
            if (verbose) printf("  Adding: glfw\n");
            tcc_add_library(tcc, "glfw");
        }
        
        #ifdef __APPLE__
            tcc_add_library(tcc, "objc");
            tcc_add_library(tcc, "Cocoa");
            tcc_add_library(tcc, "IOKit");
            tcc_add_library(tcc, "CoreFoundation");
            tcc_add_library(tcc, "CoreVideo");
        #endif
    #endif
}

// Показать справку
void show_help(const char* program_name) {
    printf("TCC Game Compiler\n");
    printf("Usage: %s [options] <source.c> [additional_sources...]\n\n", program_name);
    printf("Options:\n");
    printf("  -o <file>     Output executable name\n");
    printf("  -r <file>     Resource object file\n");
    printf("  -I <path>     Add include path\n");
    printf("  -L <path>     Add library path\n");
    printf("  -l <lib>      Link with library\n");
    printf("  -v            Verbose output\n");
    printf("  -a            Auto-find all .c files in current directory\n");
    printf("  -r            Recursive search for source files\n");
    printf("  -h, --help    Show this help\n");
    printf("\nExamples:\n");
    printf("  %s main.c render.c game.c\n", program_name);
    printf("  %s -o mygame.exe main.c\n", program_name);
    printf("  %s -a -o mygame.exe               # Auto-find all .c files\n", program_name);
    printf("  %s -r -o mygame.exe               # Recursive search\n", program_name);
    printf("  %s -I./include -L./lib -lglfw main.c render.c\n", program_name);
}

int main(int argc, char** argv) {
    const char* output_file = "bin/game.exe";
    const char* resource_file = "app.o";
    
    // Динамические списки
    FileList source_files = {0};
    char** include_paths = NULL;
    char** library_paths = NULL;
    char** libraries = NULL;
    int include_count = 0;
    int library_path_count = 0;
    int library_count = 0;
    int verbose = 0;
    int auto_find = 0;
    int recursive = 0;
    
    // Парсинг аргументов командной строки
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                output_file = argv[++i];
            }
            else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
                resource_file = argv[++i];
            }
            else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
                include_paths = realloc(include_paths, (include_count + 1) * sizeof(char*));
                include_paths[include_count++] = argv[++i];
            }
            else if (strcmp(argv[i], "-L") == 0 && i + 1 < argc) {
                library_paths = realloc(library_paths, (library_path_count + 1) * sizeof(char*));
                library_paths[library_path_count++] = argv[++i];
            }
            else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
                libraries = realloc(libraries, (library_count + 1) * sizeof(char*));
                libraries[library_count++] = argv[++i];
            }
            else if (strcmp(argv[i], "-v") == 0) {
                verbose = 1;
            }
            else if (strcmp(argv[i], "-a") == 0) {
                auto_find = 1;
            }
            else if (strcmp(argv[i], "--recursive") == 0) {
                recursive = 1;
            }
            else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                show_help(argv[0]);
                return 0;
            }
            else {
                fprintf(stderr, "Unknown option: %s\n", argv[i]);
                fprintf(stderr, "Use %s --help for usage information\n", argv[0]);
                return 1;
            }
        }
        else {
            // Добавляем файл в список источников
            if (is_source_file(argv[i])) {
                filelist_add(&source_files, argv[i]);
            } else {
                fprintf(stderr, "Warning: '%s' doesn't look like a C source file (.c, .cpp)\n", argv[i]);
            }
        }
    }
    
    // Если включен автопоиск и нет указанных файлов
    if ((auto_find || recursive) && source_files.count == 0) {
        if (recursive) {
            printf("Recursively searching for source files...\n");
            find_source_files(".", &source_files);
        } else {
            // Автопоиск только в текущей директории
            #ifdef _WIN32
                WIN32_FIND_DATA findData;
                HANDLE hFind = FindFirstFile("*.c", &findData);
                if (hFind != INVALID_HANDLE_VALUE) {
                    do {
                        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                            filelist_add(&source_files, findData.cFileName);
                        }
                    } while (FindNextFile(hFind, &findData));
                    FindClose(hFind);
                }
            #else
                DIR* dir = opendir(".");
                if (dir) {
                    struct dirent* entry;
                    while ((entry = readdir(dir)) != NULL) {
                        if (is_source_file(entry->d_name)) {
                            filelist_add(&source_files, entry->d_name);
                        }
                    }
                    closedir(dir);
                }
            #endif
        }
    }
    
    // Если все еще нет файлов, пробуем config.c или main.c
    if (source_files.count == 0) {
        const char* default_files[] = {"config.c", "main.c", "game.c", "src/main.c", "src/game.c", NULL};
        for (int i = 0; default_files[i] != NULL; i++) {
            if (file_exists(default_files[i])) {
                filelist_add(&source_files, default_files[i]);
                break;
            }
        }
        
        // Если все еще нет файлов
        if (source_files.count == 0) {
            fprintf(stderr, "ERROR: No source files found!\n");
            fprintf(stderr, "Specify source files or use -a to auto-find\n");
            return 1;
        }
    }
    
    printf("Mirlit Compiler\n");
    printf("============\n");
    printf("Output: %s\n", output_file);
    printf("Resource: %s\n", resource_file);
    printf("Source files (%d):\n", source_files.count);
    for (int i = 0; i < source_files.count; i++) {
        printf("  %s\n", source_files.files[i]);
    }
    printf("\n");
    
    // Проверяем исходные файлы
    for (int i = 0; i < source_files.count; i++) {
        if (!file_exists(source_files.files[i])) {
            fprintf(stderr, "ERROR: Source file '%s' not found!\n", source_files.files[i]);
            filelist_free(&source_files);
            free(include_paths);
            free(library_paths);
            free(libraries);
            return 1;
        }
    }
    
    // Создаем выходную директорию
    char output_dir[256];
    strncpy(output_dir, output_file, sizeof(output_dir));
    char* last_slash = strrchr(output_dir, '/');
    if (!last_slash) last_slash = strrchr(output_dir, '\\');
    if (last_slash) {
        *last_slash = '\0';
        ensure_directory(output_dir);
    } else {
        ensure_directory("bin");
    }
    
    // Создаем компилятор
    TCCState* tcc = tcc_new();
    if (!tcc) {
        fprintf(stderr, "ERROR: Cannot create TCC instance\n");
        filelist_free(&source_files);
        free(include_paths);
        free(library_paths);
        free(libraries);
        return 1;
    }
    
    // Настройки
    tcc_set_output_type(tcc, TCC_OUTPUT_EXE);
    
    // Добавляем стандартные пути include
    tcc_add_include_path(tcc, "include");
    tcc_add_include_path(tcc, ".");
    tcc_add_include_path(tcc, "src");
    tcc_add_include_path(tcc, ".");
    tcc_add_include_path(tcc, "mirulit");
    
    // Добавляем пользовательские пути include
    for (int i = 0; i < include_count; i++) {
        if (verbose) printf("Adding include path: %s\n", include_paths[i]);
        tcc_add_include_path(tcc, include_paths[i]);
    }
    
    // Добавляем стандартные пути для библиотек
    tcc_add_library_path(tcc, "lib");
    tcc_add_library_path(tcc, ".");
    
    // Добавляем пользовательские пути для библиотек
    for (int i = 0; i < library_path_count; i++) {
        if (verbose) printf("Adding library path: %s\n", library_paths[i]);
        tcc_add_library_path(tcc, library_paths[i]);
    }
    
    // 1. Компилируем все исходные файлы
    printf("1. Compiling source files:\n");
    int compile_error = 0;
    for (int i = 0; i < source_files.count; i++) {
        printf("  [%d/%d] %s\n", i + 1, source_files.count, source_files.files[i]);
        if (tcc_add_file(tcc, source_files.files[i]) != 0) {
            fprintf(stderr, "ERROR: Failed to compile %s\n", source_files.files[i]);
            compile_error = 1;
            break;
        }
    }
    
    if (compile_error) {
        printf("\nPossible issues:\n");
        printf("  - Missing include files\n");
        printf("  - Syntax errors in source code\n");
        printf("  - Required headers not found\n");
        
        tcc_delete(tcc);
        filelist_free(&source_files);
        free(include_paths);
        free(library_paths);
        free(libraries);
        return 1;
    }
    
    // 2. Добавляем ресурсы
    if (file_exists(resource_file)) {
        printf("2. Adding resource: %s\n", resource_file);
        if (tcc_add_file(tcc, resource_file) != 0) {
            printf("  Warning: Could not add %s (may be incompatible format)\n", resource_file);
        }
    } else {
        printf("2. Resource file not found: %s\n", resource_file);
    }
    
    // 3. Добавляем библиотеки из lib
    if (verbose) printf("3. Adding libraries from lib\n");
    add_all_library_files(tcc, "lib", verbose);
    
    // 4. Добавляем пользовательские библиотеки
    for (int i = 0; i < library_count; i++) {
        if (verbose) printf("  Adding library: %s\n", libraries[i]);
        tcc_add_library(tcc, libraries[i]);
    }
    
    // 5. Добавляем системные библиотеки
    add_system_libraries(tcc, verbose);
    
    // 6. Создаем выходной файл
    printf("\n4. Creating output: %s\n", output_file);
    if (tcc_output_file(tcc, output_file) != 0) {
        fprintf(stderr, "ERROR: Failed to create output file\n");
        
        printf("\nTroubleshooting:\n");
        printf("  - Check that all required libraries exist\n");
        printf("  - Verify library paths are correct\n");
        printf("  - Ensure output directory is writable\n");
        printf("  - Check for duplicate main() functions\n");
        
        tcc_delete(tcc);
        filelist_free(&source_files);
        free(include_paths);
        free(library_paths);
        free(libraries);
        return 1;
    }
    
    // Очистка
    tcc_delete(tcc);
    filelist_free(&source_files);
    free(include_paths);
    free(library_paths);
    free(libraries);
    
    // Проверяем, что файл создан
    if (file_exists(output_file)) {
        printf("\n✅ SUCCESS: Compilation completed!\n");
        printf("   Output: %s\n", output_file);
        
        // Получаем размер файла
        FILE* f = fopen(output_file, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fclose(f);
            
            if (size < 1024) 
                printf("   Size: %ld bytes\n", size);
            else if (size < 1024*1024) 
                printf("   Size: %.1f KB\n", size/1024.0);
            else 
                printf("   Size: %.1f MB\n", size/(1024.0*1024.0));
        }
        
        #ifdef _WIN32
            printf("   Run: %s\n", output_file);
        #else
            if (access(output_file, X_OK) == 0) {
                printf("   Run: ./%s\n", output_file);
            } else {
                printf("   Note: File may not be executable, run: chmod +x %s\n", output_file);
            }
        #endif
    } else {
        fprintf(stderr, "\n❌ ERROR: Output file was not created\n");
        return 1;
    }
    
    return 0;
}
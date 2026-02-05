#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#define NK_IMPLEMENTATION
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_BUTTON_TRIGGER_ON_RELEASE
#define NK_GLFW_GL3_IMPLEMENTATION

#include <nuklear.h>
#include "nuklear_glfw_gl3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define popen _popen
    #define pclose _pclose
    #define mkdir _mkdir
    #define PATH_SEPARATOR '\\'
    #define PATH_SEPARATOR_STR "\\"
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <dirent.h>
    #define PATH_SEPARATOR '/'
    #define PATH_SEPARATOR_STR "/"
#endif

/* ==================== STRUCTURES ==================== */

typedef struct {
    char* buffer;
    size_t buffer_size;
    size_t buffer_length;
    char filename[256];
    char filepath[512];
    int modified;
    int line_count;
    int cursor_line;
    int cursor_column;
} TextEditor;

typedef struct {
    char output_name[256];
    char compiler[256];
    char cflags[512];
    int optimization;
    int debug_info;
    int warnings;
} BuildConfig;

typedef struct {
    /* Windows visibility */
    int show_editor;
    int show_explorer;
    int show_build_settings;
    int show_output;
    
    /* Editor */
    TextEditor editor;
    
    /* File manager */
    char current_dir[512];
    char file_list[256][256];
    int file_count;
    int selected_file;
    
    /* Build */
    BuildConfig build;
    char build_log[4096];
    int building;
    nk_size build_progress;
    
    /* GLFW */
    GLFWwindow* window;
    struct nk_context* nk_ctx;
    struct nk_glfw nk_glfw;
} IDEState;

/* ==================== PROTOTYPES ==================== */

static void init_editor(TextEditor* editor);
static void init_build_config(BuildConfig* config);
static void init_state(IDEState* state);
static int load_file(TextEditor* editor, const char* filepath);
static int save_file(TextEditor* editor);
static void scan_directory(IDEState* state);
static void build_project(IDEState* state);
static void render_ui(IDEState* state);

/* ==================== GLOBAL VARIABLES ==================== */

static IDEState g_state;

/* ==================== ERROR HANDLERS ==================== */

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

/* ==================== INITIALIZATION ==================== */

static void init_editor(TextEditor* editor) {
    editor->buffer_size = 65536;
    editor->buffer = (char*)malloc(editor->buffer_size);
    if (!editor->buffer) {
        fprintf(stderr, "Failed to allocate memory for editor\n");
        exit(1);
    }
    editor->buffer[0] = '\0';
    editor->buffer_length = 0;
    
    strcpy(editor->filename, "untitled.c");
    strcpy(editor->filepath, "");
    editor->modified = 0;
    editor->line_count = 1;
    editor->cursor_line = 1;
    editor->cursor_column = 0;
    
    /* Default C code */
    const char* default_code = 
        "/* Welcome to C Editor */\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n\n"
        "int main(void) {\n"
        "    printf(\"Hello, World!\\n\");\n"
        "    \n"
        "    for (int i = 1; i <= 5; i++) {\n"
        "        printf(\"Counter: %d\\n\", i);\n"
        "    }\n"
        "    \n"
        "    return 0;\n"
        "}\n";
    
    strncpy(editor->buffer, default_code, editor->buffer_size - 1);
    editor->buffer[editor->buffer_size - 1] = '\0';
    editor->buffer_length = strlen(editor->buffer);
    
    /* Count lines */
    editor->line_count = 1;
    for (size_t i = 0; i < editor->buffer_length; i++) {
        if (editor->buffer[i] == '\n') {
            editor->line_count++;
        }
    }
}

static void init_build_config(BuildConfig* config) {
    strcpy(config->output_name, "program");
    #ifdef _WIN32
        strcpy(config->compiler, "gcc");
        strcat(config->output_name, ".exe");
    #else
        strcpy(config->compiler, "gcc");
    #endif
    
    strcpy(config->cflags, "-Wall -Wextra -std=c11");
    config->optimization = 1;
    config->debug_info = 1;
    config->warnings = 2;
}

static void init_state(IDEState* state) {
    memset(state, 0, sizeof(IDEState));
    
    /* Window settings */
    state->show_editor = 1;
    state->show_explorer = 1;
    state->show_build_settings = 0;
    state->show_output = 0;
    
    /* Initialize components */
    init_editor(&state->editor);
    init_build_config(&state->build);
    
    /* File manager */
    state->file_count = 0;
    state->selected_file = -1;
    
    #ifdef _WIN32
        _getcwd(state->current_dir, sizeof(state->current_dir));
    #else
        getcwd(state->current_dir, sizeof(state->current_dir));
    #endif
    
    /* Build */
    state->building = 0;
    state->build_progress = 0;
    state->build_log[0] = '\0';
}

/* ==================== EDITOR FUNCTIONS ==================== */

static int is_c_file(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (!dot) return 0;
    return strcmp(dot, ".c") == 0 || strcmp(dot, ".h") == 0;
}

static int load_file(TextEditor* editor, const char* filepath) {
    if (!is_c_file(filepath)) {
        return 0; /* Only load C files */
    }
    
    FILE* file = fopen(filepath, "rb");
    if (!file) return 0;
    
    /* Get file size */
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(file);
        return 0;
    }
    
    /* Check if file fits */
    if ((size_t)size >= editor->buffer_size - 1) {
        editor->buffer_size = (size_t)size + 1024;
        char* new_buffer = (char*)realloc(editor->buffer, editor->buffer_size);
        if (!new_buffer) {
            fclose(file);
            return 0;
        }
        editor->buffer = new_buffer;
    }
    
    /* Read file */
    size_t read = fread(editor->buffer, 1, (size_t)size, file);
    editor->buffer[read] = '\0';
    editor->buffer_length = read;
    
    fclose(file);
    
    /* Update information */
    const char* slash = strrchr(filepath, PATH_SEPARATOR);
    if (slash) {
        strncpy(editor->filename, slash + 1, sizeof(editor->filename) - 1);
        editor->filename[sizeof(editor->filename) - 1] = '\0';
    } else {
        strncpy(editor->filename, filepath, sizeof(editor->filename) - 1);
        editor->filename[sizeof(editor->filename) - 1] = '\0';
    }
    strncpy(editor->filepath, filepath, sizeof(editor->filepath) - 1);
    editor->filepath[sizeof(editor->filepath) - 1] = '\0';
    editor->modified = 0;
    
    /* Count lines */
    editor->line_count = 1;
    for (size_t i = 0; i < editor->buffer_length; i++) {
        if (editor->buffer[i] == '\n') {
            editor->line_count++;
        }
    }
    
    return 1;
}

static int save_file(TextEditor* editor) {
    if (editor->filepath[0] == '\0') {
        /* Need to ask for filename */
        return 0;
    }
    
    FILE* file = fopen(editor->filepath, "wb");
    if (!file) return 0;
    
    size_t written = fwrite(editor->buffer, 1, editor->buffer_length, file);
    fclose(file);
    
    if (written != editor->buffer_length) {
        return 0;
    }
    
    editor->modified = 0;
    return 1;
}

/* ==================== FILE MANAGER ==================== */

static void scan_directory(IDEState* state) {
    state->file_count = 0;
    
    #ifdef _WIN32
        char search_path[1024];
        snprintf(search_path, sizeof(search_path), "%s\\*", state->current_dir);
        
        WIN32_FIND_DATA find_data;
        HANDLE handle = FindFirstFile(search_path, &find_data);
        
        if (handle != INVALID_HANDLE_VALUE) {
            do {
                /* Skip . and .. */
                if (strcmp(find_data.cFileName, ".") == 0 || 
                    strcmp(find_data.cFileName, "..") == 0) {
                    continue;
                }
                
                if (state->file_count >= 256) break;
                
                /* Check if it's a directory */
                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    char display[260];
                    snprintf(display, sizeof(display), "[DIR] %s", find_data.cFileName);
                    strncpy(state->file_list[state->file_count], display, 255);
                    state->file_count++;
                } else if (is_c_file(find_data.cFileName)) {
                    /* Only show C files */
                    strncpy(state->file_list[state->file_count], find_data.cFileName, 255);
                    state->file_count++;
                }
                
            } while (FindNextFile(handle, &find_data));
            
            FindClose(handle);
        }
    #else
        DIR* dir = opendir(state->current_dir);
        if (!dir) return;
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            /* Skip hidden files */
            if (entry->d_name[0] == '.') continue;
            
            if (state->file_count >= 256) break;
            
            char fullpath[512];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", 
                     state->current_dir, entry->d_name);
            
            struct stat st;
            if (stat(fullpath, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    char display[260];
                    snprintf(display, sizeof(display), "[DIR] %s", entry->d_name);
                    strncpy(state->file_list[state->file_count], display, 255);
                    state->file_count++;
                } else if (is_c_file(entry->d_name)) {
                    /* Only show C files */
                    strncpy(state->file_list[state->file_count], entry->d_name, 255);
                    state->file_count++;
                }
            }
        }
        
        closedir(dir);
    #endif
}

/* ==================== BUILD ==================== */

static void build_project(IDEState* state) {
    state->building = 1;
    state->build_progress = 0;
    state->build_log[0] = '\0';
    
    char command[2048];
    
    /* Form compile command */
    if (state->editor.filepath[0] != '\0') {
        snprintf(command, sizeof(command),
            "%s %s %s -o %s",
            state->build.compiler,
            state->build.cflags,
            state->editor.filepath,
            state->build.output_name);
    } else {
        /* Create a temporary file */
        FILE* temp = fopen("temp.c", "w");
        if (temp) {
            fwrite(state->editor.buffer, 1, state->editor.buffer_length, temp);
            fclose(temp);
            snprintf(command, sizeof(command),
                "%s %s temp.c -o %s",
                state->build.compiler,
                state->build.cflags,
                state->build.output_name);
        } else {
            strcpy(state->build_log, "Error: Cannot create temporary file\n");
            state->building = 0;
            return;
        }
    }
    
    /* Add optimization */
    if (state->build.optimization == 1) strcat(command, " -O1");
    else if (state->build.optimization == 2) strcat(command, " -O2");
    else if (state->build.optimization == 3) strcat(command, " -O3");
    
    /* Add debug info */
    if (state->build.debug_info) strcat(command, " -g");
    
    /* Log command */
    strcat(state->build_log, "Compile command:\n");
    strcat(state->build_log, command);
    strcat(state->build_log, "\n\n");
    
    state->build_progress = 30;
    
    /* Execute compilation */
    FILE* pipe = popen(command, "r");
    if (pipe) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            if (strlen(state->build_log) + strlen(buffer) < sizeof(state->build_log)) {
                strcat(state->build_log, buffer);
            }
        }
        
        int result = pclose(pipe);
        
        if (result == 0) {
            if (strlen(state->build_log) + 50 < sizeof(state->build_log)) {
                strcat(state->build_log, "\nBuild successful!\n");
            }
        } else {
            if (strlen(state->build_log) + 50 < sizeof(state->build_log)) {
                strcat(state->build_log, "\nBuild failed!\n");
            }
        }
        
        state->build_progress = 100;
    } else {
        if (strlen(state->build_log) + 50 < sizeof(state->build_log)) {
            strcat(state->build_log, "Error: Cannot execute compiler\n");
        }
    }
    
    /* Clean up temp file if it exists */
    remove("temp.c");
    
    state->building = 0;
    state->show_output = 1;
}

/* ==================== UI RENDERING ==================== */

static void apply_dark_theme(struct nk_context* ctx) {
    struct nk_color table[NK_COLOR_COUNT];
    
    table[NK_COLOR_TEXT] = nk_rgb(210, 210, 210);
    table[NK_COLOR_WINDOW] = nk_rgb(37, 37, 38);
    table[NK_COLOR_HEADER] = nk_rgb(45, 45, 48);
    table[NK_COLOR_BORDER] = nk_rgb(30, 30, 30);
    table[NK_COLOR_BUTTON] = nk_rgb(62, 62, 66);
    table[NK_COLOR_BUTTON_HOVER] = nk_rgb(72, 72, 76);
    table[NK_COLOR_BUTTON_ACTIVE] = nk_rgb(82, 82, 86);
    table[NK_COLOR_TOGGLE] = nk_rgb(62, 62, 66);
    table[NK_COLOR_TOGGLE_HOVER] = nk_rgb(72, 72, 76);
    table[NK_COLOR_TOGGLE_CURSOR] = nk_rgb(0, 122, 204);
    table[NK_COLOR_SELECT] = nk_rgb(0, 122, 204);
    table[NK_COLOR_SELECT_ACTIVE] = nk_rgb(0, 122, 204);
    table[NK_COLOR_SLIDER] = nk_rgb(62, 62, 66);
    table[NK_COLOR_SLIDER_CURSOR] = nk_rgb(0, 122, 204);
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgb(30, 144, 255);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgb(0, 90, 180);
    table[NK_COLOR_PROPERTY] = nk_rgb(45, 45, 48);
    table[NK_COLOR_EDIT] = nk_rgb(30, 30, 30);
    table[NK_COLOR_EDIT_CURSOR] = nk_rgb(200, 200, 200);
    table[NK_COLOR_COMBO] = nk_rgb(45, 45, 48);
    table[NK_COLOR_CHART] = nk_rgb(62, 62, 66);
    table[NK_COLOR_CHART_COLOR] = nk_rgb(0, 122, 204);
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgb(255, 0, 0);
    table[NK_COLOR_SCROLLBAR] = nk_rgb(30, 30, 30);
    table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgb(62, 62, 66);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgb(72, 72, 76);
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgb(82, 82, 86);
    table[NK_COLOR_TAB_HEADER] = nk_rgb(45, 45, 48);
    
    nk_style_from_table(ctx, table);
}

static void render_menu_bar(IDEState* state) {
    if (nk_begin(state->nk_ctx, "MenuBar", nk_rect(0, 0, 1600, 25),
        NK_WINDOW_NO_SCROLLBAR)) {
        
        nk_layout_row_begin(state->nk_ctx, NK_STATIC, 25, 5);
        
        nk_layout_row_push(state->nk_ctx, 60);
        if (nk_menu_begin_label(state->nk_ctx, "File", NK_TEXT_LEFT, nk_vec2(200, 200))) {
            nk_layout_row_dynamic(state->nk_ctx, 25, 1);
            
            if (nk_menu_item_label(state->nk_ctx, "New", NK_TEXT_LEFT)) {
                init_editor(&state->editor);
            }
            
            if (nk_menu_item_label(state->nk_ctx, "Open...", NK_TEXT_LEFT)) {
                state->show_explorer = 1;
            }
            
            if (nk_menu_item_label(state->nk_ctx, "Save", NK_TEXT_LEFT)) {
                if (save_file(&state->editor)) {
                    /* File saved */
                }
            }
            
            if (nk_menu_item_label(state->nk_ctx, "Save As...", NK_TEXT_LEFT)) {
                /* TODO: Save dialog */
            }
            
            nk_menu_end(state->nk_ctx);
        }
        
        nk_layout_row_push(state->nk_ctx, 80);
        if (nk_menu_begin_label(state->nk_ctx, "Edit", NK_TEXT_LEFT, nk_vec2(200, 200))) {
            nk_layout_row_dynamic(state->nk_ctx, 25, 1);
            if (nk_menu_item_label(state->nk_ctx, "Undo", NK_TEXT_LEFT)) {}
            if (nk_menu_item_label(state->nk_ctx, "Redo", NK_TEXT_LEFT)) {}
            nk_spacing(state->nk_ctx, 1);
            if (nk_menu_item_label(state->nk_ctx, "Cut", NK_TEXT_LEFT)) {}
            if (nk_menu_item_label(state->nk_ctx, "Copy", NK_TEXT_LEFT)) {}
            if (nk_menu_item_label(state->nk_ctx, "Paste", NK_TEXT_LEFT)) {}
            nk_menu_end(state->nk_ctx);
        }
        
        nk_layout_row_push(state->nk_ctx, 100);
        if (nk_menu_begin_label(state->nk_ctx, "View", NK_TEXT_LEFT, nk_vec2(200, 150))) {
            nk_layout_row_dynamic(state->nk_ctx, 25, 1);
            nk_checkbox_label(state->nk_ctx, "Editor", &state->show_editor);
            nk_checkbox_label(state->nk_ctx, "Explorer", &state->show_explorer);
            nk_menu_end(state->nk_ctx);
        }
        
        nk_layout_row_push(state->nk_ctx, 80);
        if (nk_menu_begin_label(state->nk_ctx, "Build", NK_TEXT_LEFT, nk_vec2(200, 150))) {
            nk_layout_row_dynamic(state->nk_ctx, 25, 1);
            if (nk_menu_item_label(state->nk_ctx, "Build", NK_TEXT_LEFT)) {
                build_project(state);
            }
            if (nk_menu_item_label(state->nk_ctx, "Run", NK_TEXT_LEFT)) {
                #ifdef _WIN32
                    char command[512];
                    snprintf(command, sizeof(command), "start %s", state->build.output_name);
                    system(command);
                #endif
            }
            if (nk_menu_item_label(state->nk_ctx, "Settings", NK_TEXT_LEFT)) {
                state->show_build_settings = 1;
            }
            nk_menu_end(state->nk_ctx);
        }
        
        nk_layout_row_push(state->nk_ctx, 60);
        if (nk_menu_begin_label(state->nk_ctx, "Help", NK_TEXT_LEFT, nk_vec2(200, 100))) {
            nk_layout_row_dynamic(state->nk_ctx, 25, 1);
            if (nk_menu_item_label(state->nk_ctx, "About", NK_TEXT_LEFT)) {}
            nk_menu_end(state->nk_ctx);
        }
        
        nk_layout_row_end(state->nk_ctx);
    }
    nk_end(state->nk_ctx);
}

static void render_toolbar(IDEState* state) {
    if (nk_begin(state->nk_ctx, "Toolbar", nk_rect(0, 25, 1600, 35),
        NK_WINDOW_NO_SCROLLBAR)) {
        
        nk_layout_row_begin(state->nk_ctx, NK_STATIC, 30, 6);
        
        nk_layout_row_push(state->nk_ctx, 40);
        if (nk_button_symbol(state->nk_ctx, NK_SYMBOL_TRIANGLE_RIGHT)) {
            build_project(state);
        }
        
        nk_layout_row_push(state->nk_ctx, 80);
        if (nk_button_label(state->nk_ctx, "Build")) {
            build_project(state);
        }
        
        nk_layout_row_push(state->nk_ctx, 60);
        if (nk_button_label(state->nk_ctx, "Run")) {
            #ifdef _WIN32
                char command[512];
                snprintf(command, sizeof(command), "start %s", state->build.output_name);
                system(command);
            #endif
        }
        
        nk_layout_row_push(state->nk_ctx, 20); /* Spacer */
        
        nk_layout_row_push(state->nk_ctx, 60);
        if (nk_button_label(state->nk_ctx, "Save")) {
            if (save_file(&state->editor)) {
                /* File saved */
            }
        }
        
        nk_layout_row_push(state->nk_ctx, 100);
        char title[256];
        snprintf(title, sizeof(title), "%s%s", 
                state->editor.filename,
                state->editor.modified ? " *" : "");
        nk_label(state->nk_ctx, title, NK_TEXT_LEFT);
        
        nk_layout_row_end(state->nk_ctx);
    }
    nk_end(state->nk_ctx);
}

static void render_editor(IDEState* state) {
    if (!state->show_editor) return;
    
    int width, height;
    glfwGetWindowSize(state->window, &width, &height);
    
    if (nk_begin(state->nk_ctx, "Editor", 
        nk_rect(250, 65, width - 260, height - 100),
        NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE | 
        NK_WINDOW_SCALABLE)) {
        
        /* Line numbers and editor */
        nk_layout_row_begin(state->nk_ctx, NK_STATIC, height - 150, 2);
        
        /* Line numbers (50px width) */
        nk_layout_row_push(state->nk_ctx, 50);
        if (nk_group_begin(state->nk_ctx, "LineNumbers", NK_WINDOW_NO_SCROLLBAR)) {
            nk_layout_row_dynamic(state->nk_ctx, 20, 1);
            
            for (int i = 1; i <= state->editor.line_count; i++) {
                char num[16];
                snprintf(num, sizeof(num), "%4d", i);
                
                struct nk_color color = (i == state->editor.cursor_line) ? 
                    nk_rgb(206, 145, 120) : nk_rgb(133, 133, 133);
                
                nk_label_colored(state->nk_ctx, num, NK_TEXT_RIGHT, color);
            }
            
            nk_group_end(state->nk_ctx);
        }
        
        /* Text editor (remaining space) */
        nk_layout_row_push(state->nk_ctx, width - 320);
        
        if (nk_group_begin(state->nk_ctx, "CodeEditor", NK_WINDOW_BORDER)) {
            /* Editor title */
            char title[256];
            snprintf(title, sizeof(title), "%s%s", 
                    state->editor.filename,
                    state->editor.modified ? " *" : "");
            
            nk_layout_row_dynamic(state->nk_ctx, 25, 1);
            nk_label(state->nk_ctx, title, NK_TEXT_LEFT);
            
            /* Text editor - use zero-terminated string function */
            nk_layout_row_dynamic(state->nk_ctx, height - 180, 1);
            
            /* Save old length */
            size_t old_len = state->editor.buffer_length;
            
            /* Edit string - use zero-terminated version */
            state->editor.modified = nk_edit_string_zero_terminated(
                state->nk_ctx,
                NK_EDIT_BOX | NK_EDIT_MULTILINE | NK_EDIT_SELECTABLE | NK_EDIT_CLIPBOARD,
                state->editor.buffer,
                (int)state->editor.buffer_size,
                nk_filter_default);
            
            /* Update buffer length */
            state->editor.buffer_length = strlen(state->editor.buffer);
            
            /* Count lines if buffer changed */
            if (old_len != state->editor.buffer_length) {
                state->editor.line_count = 1;
                for (size_t i = 0; i < state->editor.buffer_length; i++) {
                    if (state->editor.buffer[i] == '\n') {
                        state->editor.line_count++;
                    }
                }
            }
            
            nk_group_end(state->nk_ctx);
        }
        
        nk_layout_row_end(state->nk_ctx);
        
    } else {
        state->show_editor = 0;
    }
    nk_end(state->nk_ctx);
}

static void render_explorer(IDEState* state) {
    if (!state->show_explorer) return;
    
    int height;
    glfwGetWindowSize(state->window, &height, &height);
    
    if (nk_begin(state->nk_ctx, "Explorer", nk_rect(5, 65, 240, height - 100),
        NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE)) {
        
        nk_layout_row_dynamic(state->nk_ctx, 25, 1);
        nk_label(state->nk_ctx, state->current_dir, NK_TEXT_LEFT);
        
        /* Navigation buttons */
        nk_layout_row_dynamic(state->nk_ctx, 30, 2);
        if (nk_button_label(state->nk_ctx, "Up")) {
            #ifdef _WIN32
                _chdir("..");
                _getcwd(state->current_dir, sizeof(state->current_dir));
            #else
                chdir("..");
                getcwd(state->current_dir, sizeof(state->current_dir));
            #endif
            scan_directory(state);
        }
        
        if (nk_button_label(state->nk_ctx, "Refresh")) {
            scan_directory(state);
        }
        
        /* File list */
        nk_layout_row_dynamic(state->nk_ctx, height - 200, 1);
        if (nk_group_begin(state->nk_ctx, "FileList", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(state->nk_ctx, 25, 1);
            
            for (int i = 0; i < state->file_count; i++) {
                int selected = (state->selected_file == i);
                
                if (nk_selectable_label(state->nk_ctx, state->file_list[i], NK_TEXT_LEFT, &selected)) {
                    state->selected_file = i;
                    
                    if (strncmp(state->file_list[i], "[DIR]", 5) == 0) {
                        /* This is a directory - enter it */
                        char new_dir[512];
                        snprintf(new_dir, sizeof(new_dir), "%s/%s", 
                                 state->current_dir, 
                                 state->file_list[i] + 6);
                        
                        #ifdef _WIN32
                            if (_chdir(new_dir) == 0) {
                                char cwd[512];
                                _getcwd(cwd, sizeof(cwd));
                                strcpy(state->current_dir, cwd);
                                scan_directory(state);
                            }
                        #else
                            if (chdir(new_dir) == 0) {
                                char cwd[512];
                                getcwd(cwd, sizeof(cwd));
                                strcpy(state->current_dir, cwd);
                                scan_directory(state);
                            }
                        #endif
                        state->selected_file = -1;
                    } else {
                        /* This is a C file - load it in editor */
                        char fullpath[512];
                        snprintf(fullpath, sizeof(fullpath), "%s/%s", 
                                 state->current_dir, 
                                 state->file_list[i]);
                        
                        if (load_file(&state->editor, fullpath)) {
                            /* File loaded successfully */
                        }
                    }
                }
            }
            
            nk_group_end(state->nk_ctx);
        }
        
    } else {
        state->show_explorer = 0;
    }
    nk_end(state->nk_ctx);
}

static void render_build_settings(IDEState* state) {
    if (!state->show_build_settings) return;
    
    int width, height;
    glfwGetWindowSize(state->window, &width, &height);
    
    if (nk_begin(state->nk_ctx, "Build Settings", 
        nk_rect(width/4, height/4, width/2, height/2),
        NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE)) {
        
        nk_layout_row_dynamic(state->nk_ctx, 25, 1);
        nk_label(state->nk_ctx, "Build Configuration", NK_TEXT_CENTERED);
        
        /* Compiler settings */
        nk_layout_row_dynamic(state->nk_ctx, 30, 1);
        nk_label(state->nk_ctx, "Compiler:", NK_TEXT_LEFT);
        nk_edit_string_zero_terminated(state->nk_ctx, NK_EDIT_FIELD,
            state->build.compiler, sizeof(state->build.compiler),
            nk_filter_default);
        
        nk_layout_row_dynamic(state->nk_ctx, 30, 1);
        nk_label(state->nk_ctx, "Output file:", NK_TEXT_LEFT);
        nk_edit_string_zero_terminated(state->nk_ctx, NK_EDIT_FIELD,
            state->build.output_name, sizeof(state->build.output_name),
            nk_filter_default);
        
        nk_layout_row_dynamic(state->nk_ctx, 30, 1);
        nk_label(state->nk_ctx, "Compiler flags:", NK_TEXT_LEFT);
        nk_edit_string_zero_terminated(state->nk_ctx, NK_EDIT_FIELD,
            state->build.cflags, sizeof(state->build.cflags),
            nk_filter_default);
        
        /* Optimization */
        nk_layout_row_dynamic(state->nk_ctx, 30, 2);
        nk_label(state->nk_ctx, "Optimization:", NK_TEXT_LEFT);
        nk_property_int(state->nk_ctx, "Level", 0, 
            &state->build.optimization, 3, 1, 1);
        
        /* Debug info */
        nk_layout_row_dynamic(state->nk_ctx, 30, 2);
        nk_label(state->nk_ctx, "Debug info:", NK_TEXT_LEFT);
        nk_checkbox_label(state->nk_ctx, "", &state->build.debug_info);
        
        /* Buttons */
        nk_layout_row_dynamic(state->nk_ctx, 40, 3);
        if (nk_button_label(state->nk_ctx, "Save")) {
            state->show_build_settings = 0;
        }
        
        if (nk_button_label(state->nk_ctx, "Build Now")) {
            state->show_build_settings = 0;
            build_project(state);
        }
        
        if (nk_button_label(state->nk_ctx, "Cancel")) {
            state->show_build_settings = 0;
        }
        
    } else {
        state->show_build_settings = 0;
    }
    nk_end(state->nk_ctx);
}

static void render_build_output(IDEState* state) {
    if (!state->show_output) return;
    
    int width, height;
    glfwGetWindowSize(state->window, &width, &height);
    
    if (nk_begin(state->nk_ctx, "Build Output", 
        nk_rect(width/4, height/4, width/2, height/2),
        NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE)) {
        
        if (state->building) {
            nk_layout_row_dynamic(state->nk_ctx, 30, 1);
            nk_label(state->nk_ctx, "Building...", NK_TEXT_CENTERED);
            
            nk_layout_row_dynamic(state->nk_ctx, 30, 1);
            nk_progress(state->nk_ctx, &state->build_progress, 100, NK_FIXED);
        } else {
            nk_layout_row_dynamic(state->nk_ctx, height/2 - 100, 1);
            
            if (nk_group_begin(state->nk_ctx, "BuildOutput", NK_WINDOW_BORDER)) {
                nk_layout_row_dynamic(state->nk_ctx, height/2 - 120, 1);
                nk_edit_string_zero_terminated(state->nk_ctx, 
                    NK_EDIT_BOX | NK_EDIT_READ_ONLY | NK_EDIT_MULTILINE,
                    state->build_log, sizeof(state->build_log),
                    nk_filter_default);
                nk_group_end(state->nk_ctx);
            }
            
            nk_layout_row_dynamic(state->nk_ctx, 40, 1);
            if (nk_button_label(state->nk_ctx, "Close")) {
                state->show_output = 0;
            }
        }
        
    } else {
        state->show_output = 0;
    }
    nk_end(state->nk_ctx);
}

static void render_ui(IDEState* state) {
    render_menu_bar(state);
    render_toolbar(state);
    render_explorer(state);
    render_editor(state);
    render_build_settings(state);
    render_build_output(state);
}

/* ==================== MAIN FUNCTION ==================== */

int main(void) {
    /* Initialize GLFW */
    glfwSetErrorCallback(glfw_error_callback);
    
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }
    
    /* Window settings */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    
    /* Create window */
    GLFWwindow* window = glfwCreateWindow(1600, 900, "C Code Editor", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    
    /* Initialize GLAD */
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        glfwTerminate();
        return -1;
    }
    
    /* Initialize IDE */
    init_state(&g_state);
    g_state.window = window;
    
    /* Initialize Nuklear */
    g_state.nk_ctx = nk_glfw3_init(&g_state.nk_glfw, window, NK_GLFW3_INSTALL_CALLBACKS);
    if (!g_state.nk_ctx) {
        fprintf(stderr, "Failed to initialize Nuklear\n");
        glfwTerminate();
        return -1;
    }
    
    /* Fonts */
    struct nk_font_atlas* atlas;
    nk_glfw3_font_stash_begin(&g_state.nk_glfw, &atlas);
    struct nk_font* font = nk_font_atlas_add_default(atlas, 16, NULL);
    nk_rune range[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0400, 0x044F, // Cyrillic
        0, };
    font->config->range = range;
    nk_glfw3_font_stash_end(&g_state.nk_glfw);
    
    if (font) {
        nk_style_set_font(g_state.nk_ctx, &font->handle);
    }
    
    /* Apply theme */
    apply_dark_theme(g_state.nk_ctx);
    
    /* Scan current directory */
    scan_directory(&g_state);
    
    /* Main loop */
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        nk_glfw3_new_frame(&g_state.nk_glfw);
        
        /* Clear screen */
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        /* Render UI */
        render_ui(&g_state);
        
        /* Render Nuklear */
        nk_glfw3_render(&g_state.nk_glfw, NK_ANTI_ALIASING_ON, 1024*1024, 256*1024);
        
        /* Swap buffers */
        glfwSwapBuffers(window);
    }
    
    /* Cleanup */
    nk_glfw3_shutdown(&g_state.nk_glfw);
    glfwDestroyWindow(window);
    glfwTerminate();
    
    /* Free memory */
    if (g_state.editor.buffer) free(g_state.editor.buffer);
    
    return 0;
}
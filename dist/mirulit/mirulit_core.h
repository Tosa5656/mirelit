#ifndef MIRULIT_CORE_H
#define MIRULIT_CORE_H

// Частица
typedef struct {
    MIR_Vec2 position;
    MIR_Vec2 velocity;
    MIR_Vec2 acceleration;
    MIR_Color color;
    float size;
    float life;
    float max_life;
    bool active;
} MIR_Particle;

// Камера
typedef struct {
    MIR_Vec2 position;
    float zoom;
    float rotation;
    MIR_Vec2 target;
    float smooth_speed;
    MIR_Rect bounds;
} MIR_Camera;

// Основной движок
struct MIR_Engine {
    // SDL
    SDL_Window* window;
    SDL_Renderer* renderer;
    
    // Состояние
    int width;
    int height;
    char title[128];
    bool running;
    bool paused;
    
    // Сущности
    MIR_Entity* entities[MIRULIT_MAX_ENTITIES];
    int entity_count;
    int next_id;
    
    // Частицы
    MIR_Particle particles[MIRULIT_MAX_PARTICLES];
    
    // Камера
    MIR_Camera camera;
    
    // Ввод
    bool keys[MIRULIT_MAX_KEYS];
    bool keys_down[MIRULIT_MAX_KEYS];
    bool keys_up[MIRULIT_MAX_KEYS];
    
    bool mouse_buttons[MIRULIT_MAX_BUTTONS];
    bool mouse_down[MIRULIT_MAX_BUTTONS];
    bool mouse_up[MIRULIT_MAX_BUTTONS];
    MIR_Vec2 mouse_position;
    MIR_Vec2 mouse_world_position;
    float mouse_wheel;
    
    // Время
    float delta_time;
    float time_scale;
    uint64_t last_time;
    uint64_t start_time;
    int fps;
    int target_fps;
    
    // Статистика
    int draw_calls;
    int update_calls;
    int particle_count;
    
    // Ресурсы
    SDL_Texture* textures[100];
    int texture_count;
    
    // Состояние игры
    int score;
    int lives;
    int level;
    bool game_over;
};

// Глобальные переменные
static MIR_Engine* _mir = NULL;
static bool _mir_initialized = false;

// ==================== ЯДРО ДВИЖКА ====================

static bool MIR_Init(const char* title, int width, int height) {
    if (_mir_initialized) return true;
    
    // Инициализация SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("[MIRULIT] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    
    // Создание движка
    _mir = (MIR_Engine*)calloc(1, sizeof(MIR_Engine));
    if (!_mir) {
        printf("[MIRULIT] Memory allocation failed\n");
        SDL_Quit();
        return false;
    }
    
    // Создание окна
    _mir->window = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);
    if (!_mir->window) {
        printf("[MIRULIT] Window creation failed: %s\n", SDL_GetError());
        free(_mir);
        SDL_Quit();
        return false;
    }
    
    // Создание рендерера
    _mir->renderer = SDL_CreateRenderer(_mir->window, NULL);
    if (!_mir->renderer) {
        printf("[MIRULIT] Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(_mir->window);
        free(_mir);
        SDL_Quit();
        return false;
    }
    
    // Инициализация параметров
    _mir->width = width;
    _mir->height = height;
    _mir->running = true;
    _mir->paused = false;
    _mir->delta_time = 0.016f;
    _mir->time_scale = 1.0f;
    _mir->target_fps = MIRULIT_DEFAULT_FPS;
    _mir->next_id = 1;
    
    strncpy(_mir->title, title, sizeof(_mir->title) - 1);
    
    // Инициализация камеры
    _mir->camera.position = (MIR_Vec2){0, 0};
    _mir->camera.zoom = 1.0f;
    _mir->camera.rotation = 0.0f;
    _mir->camera.smooth_speed = 5.0f;
    _mir->camera.bounds = (MIR_Rect){-1000, -1000, 2000, 2000};
    
    // Инициализация времени
    _mir->start_time = SDL_GetTicks();
    _mir->last_time = _mir->start_time;
    
    // Инициализация рандома
    srand((unsigned int)time(NULL));
    
    printf("[MIRULIT] Engine v%s initialized: %dx%d\n", 
           MIRULIT_VERSION, width, height);
    printf("[MIRULIT] SDL3 version: %d.%d.%d\n",
           SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
    
    _mir_initialized = true;
    return true;
}

static void MIR_Shutdown(void) {
    if (!_mir_initialized || !_mir) return;
    
    printf("[MIRULIT] Shutting down...\n");
    
    // Уничтожение всех сущностей
    for (int i = 0; i < _mir->entity_count; i++) {
        if (_mir->entities[i]) {
            if (_mir->entities[i]->on_destroy) {
                _mir->entities[i]->on_destroy(_mir->entities[i]);
            }
            
            // Освобождение компонентов
            for (int j = 0; j < _mir->entities[i]->component_count; j++) {
                free(_mir->entities[i]->components[j]);
            }
            
            // Освобождение текстуры
            if (_mir->entities[i]->sprite.texture) {
                SDL_DestroyTexture(_mir->entities[i]->sprite.texture);
            }
            
            free(_mir->entities[i]);
        }
    }
    
    // Освобождение загруженных текстур
    for (int i = 0; i < _mir->texture_count; i++) {
        if (_mir->textures[i]) {
            SDL_DestroyTexture(_mir->textures[i]);
        }
    }
    
    // Освобождение SDL
    SDL_DestroyRenderer(_mir->renderer);
    SDL_DestroyWindow(_mir->window);
    SDL_Quit();
    
    // Освобождение движка
    free(_mir);
    _mir = NULL;
    _mir_initialized = false;
    
    printf("[MIRULIT] Engine shutdown complete\n");
}

// ==================== ЦИКЛ ОБНОВЛЕНИЯ ====================

static void MIR_ProcessEvents(void) {
    if (!_mir_initialized || !_mir) return;
    
    // Сброс состояний клавиш и мыши
    for (int i = 0; i < MIRULIT_MAX_KEYS; i++) {
        _mir->keys_down[i] = false;
        _mir->keys_up[i] = false;
    }
    
    for (int i = 0; i < MIRULIT_MAX_BUTTONS; i++) {
        _mir->mouse_down[i] = false;
        _mir->mouse_up[i] = false;
    }
    _mir->mouse_wheel = 0;
    
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                _mir->running = false;
                break;
                
            case SDL_EVENT_KEY_DOWN: {
                int key = event.key.key;
                if (key >= 0 && key < MIRULIT_MAX_KEYS) {
                    if (!_mir->keys[key]) {
                        _mir->keys_down[key] = true;
                    }
                    _mir->keys[key] = true;
                }
                break;
            }
                
            case SDL_EVENT_KEY_UP: {
                int key = event.key.key;
                if (key >= 0 && key < MIRULIT_MAX_KEYS) {
                    _mir->keys_up[key] = true;
                    _mir->keys[key] = false;
                }
                break;
            }
                
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                int button = event.button.button;
                if (button >= 0 && button < MIRULIT_MAX_BUTTONS) {
                    if (!_mir->mouse_buttons[button]) {
                        _mir->mouse_down[button] = true;
                    }
                    _mir->mouse_buttons[button] = true;
                }
                break;
            }
                
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                int button = event.button.button;
                if (button >= 0 && button < MIRULIT_MAX_BUTTONS) {
                    _mir->mouse_up[button] = true;
                    _mir->mouse_buttons[button] = false;
                }
                break;
            }
                
            case SDL_EVENT_MOUSE_MOTION:
                _mir->mouse_position.x = event.motion.x;
                _mir->mouse_position.y = event.motion.y;
                
                // Конвертация в мировые координаты
                _mir->mouse_world_position.x = 
                    (_mir->mouse_position.x - _mir->width / 2.0f) / _mir->camera.zoom + 
                    _mir->camera.position.x;
                _mir->mouse_world_position.y = 
                    (_mir->mouse_position.y - _mir->height / 2.0f) / _mir->camera.zoom + 
                    _mir->camera.position.y;
                break;
                
            case SDL_EVENT_MOUSE_WHEEL:
                _mir->mouse_wheel = event.wheel.y;
                break;
                
            case SDL_EVENT_WINDOW_RESIZED:
                _mir->width = event.window.data1;
                _mir->height = event.window.data2;
                break;
        }
    }
}

static void MIR_BeginFrame(void) {
    if (!_mir_initialized || !_mir || !_mir->running) return;
    
    // Расчет дельта-времени
    uint64_t current_time = SDL_GetTicks();
    _mir->delta_time = (current_time - _mir->last_time) / 1000.0f;
    _mir->last_time = current_time;
    
    // Ограничение дельта-времени (защита от рывков)
    if (_mir->delta_time > 0.1f) {
        _mir->delta_time = 0.1f;
    }
    
    // Применение time scale
    float scaled_dt = _mir->delta_time * _mir->time_scale;
    
    // Расчет FPS
    static int frame_counter = 0;
    static uint64_t fps_timer = 0;
    frame_counter++;
    
    if (current_time - fps_timer >= 1000) {
        _mir->fps = frame_counter;
        frame_counter = 0;
        fps_timer = current_time;
    }
    
    // Обновление камеры
    if (_mir->camera.smooth_speed > 0 && 
        (_mir->camera.target.x != 0 || _mir->camera.target.y != 0)) {
        float t = 1.0f - expf(-_mir->camera.smooth_speed * scaled_dt);
        _mir->camera.position.x = MIR_Math_Lerp(
            _mir->camera.position.x, _mir->camera.target.x, t);
        _mir->camera.position.y = MIR_Math_Lerp(
            _mir->camera.position.y, _mir->camera.target.y, t);
    }
    
    // Очистка экрана
    SDL_SetRenderDrawColor(_mir->renderer, 
                          MIR_COLOR_BACKGROUND.r,
                          MIR_COLOR_BACKGROUND.g,
                          MIR_COLOR_BACKGROUND.b,
                          MIR_COLOR_BACKGROUND.a);
    SDL_RenderClear(_mir->renderer);
    
    // Сброс статистики
    _mir->draw_calls = 0;
    _mir->update_calls = 0;
}

static void MIR_EndFrame(void) {
    if (!_mir_initialized || !_mir) return;
    
    // Отображение
    SDL_RenderPresent(_mir->renderer);
    
    // Ограничение FPS
    if (_mir->target_fps > 0) {
        uint64_t frame_time = SDL_GetTicks() - _mir->last_time;
        uint64_t target_frame_time = 1000 / _mir->target_fps;
        
        if (frame_time < target_frame_time) {
            SDL_Delay((uint32_t)(target_frame_time - frame_time));
        }
    }
}

// ==================== КАМЕРА ====================

static void MIR_SetCameraTarget(MIR_Vec2 target) {
    if (!_mir_initialized || !_mir) return;
    _mir->camera.target = target;
}

static void MIR_SetCameraZoom(float zoom) {
    if (!_mir_initialized || !_mir) return;
    _mir->camera.zoom = MIR_Math_Clamp(zoom, 0.1f, 5.0f);
}

static void MIR_CameraShake(float intensity, float duration) {
    if (!_mir_initialized || !_mir) return;
    
    static float shake_timer = 0;
    static float shake_intensity = 0;
    
    if (intensity > 0) {
        shake_timer = duration;
        shake_intensity = intensity;
    }
    
    if (shake_timer > 0) {
        shake_timer -= _mir->delta_time;
        
        float offset_x = MIR_Math_RandomRange(-shake_intensity, shake_intensity);
        float offset_y = MIR_Math_RandomRange(-shake_intensity, shake_intensity);
        
        _mir->camera.position.x += offset_x;
        _mir->camera.position.y += offset_y;
        
        // Затухание
        shake_intensity *= 0.9f;
    }
}

#endif // MIRULIT_CORE_H
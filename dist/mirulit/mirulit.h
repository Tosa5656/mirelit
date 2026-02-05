#ifndef MIRULIT_H
#define MIRULIT_H

// ==================== КОНФИГУРАЦИЯ ====================
// Раскомментируйте для дополнительных возможностей
// #define MIRULIT_ENABLE_SDL_IMAGE   // Требует SDL3_image
// #define MIRULIT_ENABLE_SDL_TTF     // Требует SDL3_ttf
// #define MIRULIT_ENABLE_PHYSICS     // Простая физика

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef MIRULIT_ENABLE_SDL_IMAGE
#include <SDL3_image/SDL_image.h>
#endif

#ifdef MIRULIT_ENABLE_SDL_TTF
#include <SDL3_ttf/SDL_ttf.h>
#endif

// ==================== ВЕРСИЯ И НАСТРОЙКИ ====================
#define MIRULIT_VERSION "1.0.0"
#define MIRULIT_MAX_ENTITIES 1024
#define MIRULIT_MAX_KEYS 512
#define MIRULIT_MAX_BUTTONS 8
#define MIRULIT_MAX_PARTICLES 1000
#define MIRULIT_DEFAULT_FPS 60

// ==================== ЦВЕТА (RGBA) ====================
#define MIR_COLOR_WHITE      (MIR_Color){255, 255, 255, 255}
#define MIR_COLOR_BLACK      (MIR_Color){0, 0, 0, 255}
#define MIR_COLOR_RED        (MIR_Color){255, 50, 50, 255}
#define MIR_COLOR_GREEN      (MIR_Color){50, 255, 50, 255}
#define MIR_COLOR_BLUE       (MIR_Color){50, 100, 255, 255}
#define MIR_COLOR_YELLOW     (MIR_Color){255, 255, 50, 255}
#define MIR_COLOR_CYAN       (MIR_Color){50, 255, 255, 255}
#define MIR_COLOR_MAGENTA    (MIR_Color){255, 50, 255, 255}
#define MIR_COLOR_ORANGE     (MIR_Color){255, 150, 50, 255}
#define MIR_COLOR_PURPLE     (MIR_Color){150, 50, 255, 255}
#define MIR_COLOR_GRAY       (MIR_Color){128, 128, 128, 255}
#define MIR_COLOR_DARKGRAY   (MIR_Color){64, 64, 64, 255}
#define MIR_COLOR_LIGHTGRAY  (MIR_Color){192, 192, 192, 255}
#define MIR_COLOR_BACKGROUND (MIR_Color){30, 30, 40, 255}

// Предварительные объявления для устранения циклических зависимостей
typedef struct MIR_Engine MIR_Engine;
typedef struct MIR_Entity MIR_Entity;

// Подключение модулей в правильном порядке
#include <mirulit_math.h>
#include <mirulit_entity.h>
#include <mirulit_core.h>
#include <mirulit_graphics.h>
#include <mirulit_input.h>
#include <mirulit_particles.h>
#include <mirulit_collision.h>

#endif // MIRULIT_H
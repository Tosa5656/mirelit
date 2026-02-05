#ifndef MIRULIT_UTILS_H
#define MIRULIT_UTILS_H

static inline float MIR_GetDeltaTime(void) {
    return _mir_initialized && _mir ? _mir->delta_time : 0.016f;
}

static inline float MIR_GetTime(void) {
    return _mir_initialized && _mir ? 
           (SDL_GetTicks() - _mir->start_time) / 1000.0f : 0.0f;
}

static inline int MIR_GetFPS(void) {
    return _mir_initialized && _mir ? _mir->fps : 0;
}

static inline int MIR_GetScreenWidth(void) {
    return _mir_initialized && _mir ? _mir->width : 0;
}

static inline int MIR_GetScreenHeight(void) {
    return _mir_initialized && _mir ? _mir->height : 0;
}

static inline void MIR_SetTimeScale(float scale) {
    if (_mir_initialized && _mir) {
        _mir->time_scale = MIR_Math_Clamp(scale, 0.0f, 5.0f);
    }
}

static inline void MIR_SetTargetFPS(int fps) {
    if (_mir_initialized && _mir) {
        _mir->target_fps = fps > 0 ? fps : 0;
    }
}

static inline void MIR_Pause(void) {
    if (_mir_initialized && _mir) {
        _mir->paused = true;
    }
}

static inline void MIR_Resume(void) {
    if (_mir_initialized && _mir) {
        _mir->paused = false;
    }
}

static inline bool MIR_IsPaused(void) {
    return _mir_initialized && _mir ? _mir->paused : false;
}

static inline bool MIR_IsRunning(void) {
    return _mir_initialized && _mir ? _mir->running : false;
}

static inline void MIR_Quit(void) {
    if (_mir_initialized && _mir) {
        _mir->running = false;
    }
}

// ==================== ДЕБАГ И СТАТИСТИКА ====================

static inline void MIR_DrawDebugInfo(void) {
    if (!_mir_initialized || !_mir) return;
    
    // Вывод в консоль
    static int last_fps = 0;
    if (_mir->fps != last_fps) {
        printf("\r[MIRULIT] FPS: %3d | Entities: %3d | Particles: %3d | Draws: %3d", 
               _mir->fps, _mir->entity_count, _mir->particle_count, _mir->draw_calls);
        fflush(stdout);
        last_fps = _mir->fps;
    }
}

#endif // MIRULIT_UTILS_H
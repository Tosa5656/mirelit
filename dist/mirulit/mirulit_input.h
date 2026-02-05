#ifndef MIRULIT_INPUT_H
#define MIRULIT_INPUT_H

static bool MIR_IsKeyDown(int key) {
    return _mir_initialized && _mir && key >= 0 && key < MIRULIT_MAX_KEYS ? 
           _mir->keys[key] : false;
}

static bool MIR_IsKeyPressed(int key) {
    return _mir_initialized && _mir && key >= 0 && key < MIRULIT_MAX_KEYS ? 
           _mir->keys_down[key] : false;
}

static bool MIR_IsKeyReleased(int key) {
    return _mir_initialized && _mir && key >= 0 && key < MIRULIT_MAX_KEYS ? 
           _mir->keys_up[key] : false;
}

static bool MIR_IsMouseButtonDown(int button) {
    return _mir_initialized && _mir && button >= 0 && button < MIRULIT_MAX_BUTTONS ? 
           _mir->mouse_buttons[button] : false;
}

static bool MIR_IsMouseButtonPressed(int button) {
    return _mir_initialized && _mir && button >= 0 && button < MIRULIT_MAX_BUTTONS ? 
           _mir->mouse_down[button] : false;
}

static bool MIR_IsMouseButtonReleased(int button) {
    return _mir_initialized && _mir && button >= 0 && button < MIRULIT_MAX_BUTTONS ? 
           _mir->mouse_up[button] : false;
}

static MIR_Vec2 MIR_GetMousePosition(void) {
    return _mir_initialized && _mir ? _mir->mouse_position : (MIR_Vec2){0, 0};
}

static MIR_Vec2 MIR_GetMouseWorldPosition(void) {
    return _mir_initialized && _mir ? _mir->mouse_world_position : (MIR_Vec2){0, 0};
}

static float MIR_GetMouseWheel(void) {
    return _mir_initialized && _mir ? _mir->mouse_wheel : 0.0f;
}

static float MIR_GetDeltaTime(void) {
    return _mir_initialized && _mir ? _mir->delta_time : 0.016f;
}

static float MIR_GetTime(void) {
    return _mir_initialized && _mir ? 
           (SDL_GetTicks() - _mir->start_time) / 1000.0f : 0.0f;
}

static int MIR_GetFPS(void) {
    return _mir_initialized && _mir ? _mir->fps : 0;
}

static int MIR_GetScreenWidth(void) {
    return _mir_initialized && _mir ? _mir->width : 0;
}

static int MIR_GetScreenHeight(void) {
    return _mir_initialized && _mir ? _mir->height : 0;
}

static void MIR_SetTimeScale(float scale) {
    if (_mir_initialized && _mir) {
        _mir->time_scale = MIR_Math_Clamp(scale, 0.0f, 5.0f);
    }
}

static void MIR_SetTargetFPS(int fps) {
    if (_mir_initialized && _mir) {
        _mir->target_fps = fps > 0 ? fps : 0;
    }
}

static void MIR_Pause(void) {
    if (_mir_initialized && _mir) {
        _mir->paused = true;
    }
}

static void MIR_Resume(void) {
    if (_mir_initialized && _mir) {
        _mir->paused = false;
    }
}

static bool MIR_IsPaused(void) {
    return _mir_initialized && _mir ? _mir->paused : false;
}

static bool MIR_IsRunning(void) {
    return _mir_initialized && _mir ? _mir->running : false;
}

static void MIR_Quit(void) {
    if (_mir_initialized && _mir) {
        _mir->running = false;
    }
}

#endif // MIRULIT_INPUT_H
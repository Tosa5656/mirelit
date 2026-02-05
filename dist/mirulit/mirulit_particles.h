#ifndef MIRULIT_PARTICLES_H
#define MIRULIT_PARTICLES_H

static void MIR_EmitParticleEx(MIR_Vec2 position, MIR_Vec2 velocity, MIR_Vec2 acceleration, 
                       MIR_Color color, float size, float life) {
    if (!_mir_initialized || !_mir) return;
    
    for (int i = 0; i < MIRULIT_MAX_PARTICLES; i++) {
        if (!_mir->particles[i].active) {
            _mir->particles[i].position = position;
            _mir->particles[i].velocity = velocity;
            _mir->particles[i].acceleration = acceleration;
            _mir->particles[i].color = color;
            _mir->particles[i].size = size;
            _mir->particles[i].life = life;
            _mir->particles[i].max_life = life;
            _mir->particles[i].active = true;
            _mir->particle_count++;
            break;
        }
    }
}

static void MIR_EmitParticle(MIR_Vec2 position, MIR_Vec2 velocity, 
                                   MIR_Color color, float size, float life) {
    MIR_EmitParticleEx(position, velocity, (MIR_Vec2){0, 50}, color, size, life);
}

static void MIR_UpdateParticles(void) {
    if (!_mir_initialized || !_mir) return;
    
    float scaled_dt = _mir->delta_time * _mir->time_scale;
    
    for (int i = 0; i < MIRULIT_MAX_PARTICLES; i++) {
        if (!_mir->particles[i].active) continue;
        
        // Обновление физики
        _mir->particles[i].velocity = MIR_Vec2_Add(
            _mir->particles[i].velocity,
            MIR_Vec2_Multiply(_mir->particles[i].acceleration, scaled_dt)
        );
        
        _mir->particles[i].position = MIR_Vec2_Add(
            _mir->particles[i].position,
            MIR_Vec2_Multiply(_mir->particles[i].velocity, scaled_dt)
        );
        
        // Обновление жизни
        _mir->particles[i].life -= scaled_dt;
        if (_mir->particles[i].life <= 0) {
            _mir->particles[i].active = false;
            _mir->particle_count--;
        }
    }
}

static void MIR_DrawParticles(void) {
    if (!_mir_initialized || !_mir) return;
    
    for (int i = 0; i < MIRULIT_MAX_PARTICLES; i++) {
        if (!_mir->particles[i].active) continue;
        
        // Интерполяция цвета по времени жизни
        float t = _mir->particles[i].life / _mir->particles[i].max_life;
        MIR_Color color = _mir->particles[i].color;
        color.a = (uint8_t)(color.a * t);
        
        // Конвертация мировых координат в экранные (с учетом камеры)
        float screen_x = (_mir->particles[i].position.x - _mir->camera.position.x) * 
                        _mir->camera.zoom + _mir->width / 2.0f;
        float screen_y = (_mir->particles[i].position.y - _mir->camera.position.y) * 
                        _mir->camera.zoom + _mir->height / 2.0f;
        
        // Масштабирование размера частицы с учетом зума камеры
        float screen_size = _mir->particles[i].size * t * _mir->camera.zoom;
        
        // Отрисовка частицы
        SDL_SetRenderDrawColor(_mir->renderer, 
                              color.r, color.g, color.b, color.a);
        
        SDL_FRect rect = {
            screen_x - screen_size / 2,
            screen_y - screen_size / 2,
            screen_size, screen_size
        };
        SDL_RenderFillRect(_mir->renderer, &rect);
        _mir->draw_calls++;
    }
}

#endif // MIRULIT_PARTICLES_H
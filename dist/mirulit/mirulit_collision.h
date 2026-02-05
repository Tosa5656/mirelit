#ifndef MIRULIT_COLLISION_H
#define MIRULIT_COLLISION_H

static bool MIR_CheckCollision(MIR_Entity* a, MIR_Entity* b) {
    if (!a || !b || !a->collider.enabled || !b->collider.enabled) return false;
    
    MIR_Rect rectA = a->collider.bounds;
    MIR_Rect rectB = b->collider.bounds;
    
    return (rectA.x < rectB.x + rectB.w &&
            rectA.x + rectA.w > rectB.x &&
            rectA.y < rectB.y + rectB.h &&
            rectA.y + rectA.h > rectB.y);
}

static MIR_Entity* MIR_PointCollision(MIR_Vec2 point) {
    if (!_mir_initialized || !_mir) return NULL;
    
    for (int i = 0; i < _mir->entity_count; i++) {
        MIR_Entity* entity = _mir->entities[i];
        if (!entity || !entity->collider.enabled) continue;
        
        MIR_Rect bounds = entity->collider.bounds;
        if (point.x >= bounds.x && point.x <= bounds.x + bounds.w &&
            point.y >= bounds.y && point.y <= bounds.y + bounds.h) {
            return entity;
        }
    }
    return NULL;
}

static void MIR_ResolveCollisions(void) {
    if (!_mir_initialized || !_mir) return;
    
    for (int i = 0; i < _mir->entity_count; i++) {
        for (int j = i + 1; j < _mir->entity_count; j++) {
            if (MIR_CheckCollision(_mir->entities[i], _mir->entities[j])) {
                if (_mir->entities[i]->collider.on_collision) {
                    _mir->entities[i]->collider.on_collision(
                        _mir->entities[i], _mir->entities[j]);
                }
                if (_mir->entities[j]->collider.on_collision) {
                    _mir->entities[j]->collider.on_collision(
                        _mir->entities[j], _mir->entities[i]);
                }
            }
        }
    }
}

static void MIR_DrawDebugInfo(void) {
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

#endif // MIRULIT_COLLISION_H
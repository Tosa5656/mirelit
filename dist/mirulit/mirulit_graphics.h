#ifndef MIRULIT_GRAPHICS_H
#define MIRULIT_GRAPHICS_H

static MIR_Entity* MIR_CreateEntity(const char* tag) {
    if (!_mir_initialized || !_mir || _mir->entity_count >= MIRULIT_MAX_ENTITIES) {
        return NULL;
    }
    
    MIR_Entity* entity = (MIR_Entity*)calloc(1, sizeof(MIR_Entity));
    if (!entity) return NULL;
    
    entity->id = _mir->next_id++;
    entity->active = true;
    entity->visible = true;
    entity->persistent = false;
    
    if (tag) {
        strncpy(entity->tag, tag, sizeof(entity->tag) - 1);
    }
    
    // Инициализация трансформа
    entity->transform.position = (MIR_Vec2){0, 0};
    entity->transform.scale = (MIR_Vec2){1, 1};
    entity->transform.rotation = 0.0f;
    entity->transform.velocity = (MIR_Vec2){0, 0};
    entity->transform.acceleration = (MIR_Vec2){0, 0};
    
    // Инициализация спрайта
    entity->sprite.texture = NULL;
    entity->sprite.source_rect = (MIR_Rect){0, 0, 0, 0};
    entity->sprite.color = MIR_COLOR_WHITE;
    entity->sprite.z_index = 0;
    entity->sprite.flip_x = false;
    entity->sprite.flip_y = false;
    
    // Инициализация коллайдера
    entity->collider.bounds = (MIR_Rect){0, 0, 1, 1};
    entity->collider.is_trigger = false;
    entity->collider.enabled = true;
    entity->collider.on_collision = NULL;
    
    // Добавление в движок
    _mir->entities[_mir->entity_count++] = entity;
    
    return entity;
}

static void MIR_DestroyEntity(MIR_Entity* entity) {
    if (!_mir_initialized || !_mir || !entity) return;
    
    // Вызов callback
    if (entity->on_destroy) {
        entity->on_destroy(entity);
    }
    
    // Удаление из списка детей родителя
    if (entity->parent) {
        for (int i = 0; i < entity->parent->child_count; i++) {
            if (entity->parent->children[i] == entity) {
                for (int j = i; j < entity->parent->child_count - 1; j++) {
                    entity->parent->children[j] = entity->parent->children[j + 1];
                }
                entity->parent->child_count--;
                break;
            }
        }
    }
    
    // Уничтожение детей
    for (int i = 0; i < entity->child_count; i++) {
        MIR_DestroyEntity(entity->children[i]);
    }
    
    // Освобождение компонентов
    for (int i = 0; i < entity->component_count; i++) {
        free(entity->components[i]);
    }
    
    // Освобождение текстуры
    if (entity->sprite.texture) {
        SDL_DestroyTexture(entity->sprite.texture);
    }
    
    // Удаление из массива движка
    for (int i = 0; i < _mir->entity_count; i++) {
        if (_mir->entities[i] == entity) {
            free(entity);
            for (int j = i; j < _mir->entity_count - 1; j++) {
                _mir->entities[j] = _mir->entities[j + 1];
            }
            _mir->entity_count--;
            break;
        }
    }
}

static MIR_Entity* MIR_FindEntityByTag(const char* tag) {
    if (!_mir_initialized || !_mir || !tag) return NULL;
    
    for (int i = 0; i < _mir->entity_count; i++) {
        if (_mir->entities[i] && strcmp(_mir->entities[i]->tag, tag) == 0) {
            return _mir->entities[i];
        }
    }
    return NULL;
}

static MIR_Entity* MIR_FindEntityByID(int id) {
    if (!_mir_initialized || !_mir) return NULL;
    
    for (int i = 0; i < _mir->entity_count; i++) {
        if (_mir->entities[i] && _mir->entities[i]->id == id) {
            return _mir->entities[i];
        }
    }
    return NULL;
}

static void MIR_UpdateEntities(void) {
    if (!_mir_initialized || !_mir || _mir->paused) return;
    
    float scaled_dt = _mir->delta_time * _mir->time_scale;
    
    for (int i = 0; i < _mir->entity_count; i++) {
        MIR_Entity* entity = _mir->entities[i];
        
        if (!entity || !entity->active) continue;
        
        _mir->update_calls++;
        
        // Обновление физики
        entity->transform.velocity = MIR_Vec2_Add(
            entity->transform.velocity,
            MIR_Vec2_Multiply(entity->transform.acceleration, scaled_dt)
        );
        
        entity->transform.position = MIR_Vec2_Add(
            entity->transform.position,
            MIR_Vec2_Multiply(entity->transform.velocity, scaled_dt)
        );
        
        // Вызов пользовательского обновления
        if (entity->update) {
            entity->update(entity, scaled_dt);
        }
        
        // Обновление коллайдера
        if (entity->collider.enabled) {
            entity->collider.bounds.x = entity->transform.position.x - 
                                       entity->collider.bounds.w / 2;
            entity->collider.bounds.y = entity->transform.position.y - 
                                       entity->collider.bounds.h / 2;
        }
    }
}

static void MIR_DrawEntity(MIR_Entity* entity) {
    if (!_mir_initialized || !_mir || !entity || !entity->visible) return;
    
    _mir->draw_calls++;
    
    // Мировые координаты с учётом камеры
    float world_x = (entity->transform.position.x - _mir->camera.position.x) * 
                    _mir->camera.zoom + _mir->width / 2.0f;
    float world_y = (entity->transform.position.y - _mir->camera.position.y) * 
                    _mir->camera.zoom + _mir->height / 2.0f;
    
    if (entity->sprite.texture) {
        // Отрисовка текстуры
        SDL_FRect dest_rect = {
            world_x - entity->transform.scale.x * _mir->camera.zoom / 2,
            world_y - entity->transform.scale.y * _mir->camera.zoom / 2,
            entity->transform.scale.x * _mir->camera.zoom,
            entity->transform.scale.y * _mir->camera.zoom
        };
        
        SDL_SetTextureColorMod(entity->sprite.texture,
                              entity->sprite.color.r,
                              entity->sprite.color.g,
                              entity->sprite.color.b);
        SDL_SetTextureAlphaMod(entity->sprite.texture, entity->sprite.color.a);
        
        SDL_RenderTexture(_mir->renderer, entity->sprite.texture, NULL, &dest_rect);
    } else {
        // Отрисовка цветного прямоугольника
        SDL_SetRenderDrawColor(_mir->renderer,
                              entity->sprite.color.r,
                              entity->sprite.color.g,
                              entity->sprite.color.b,
                              entity->sprite.color.a);
        
        SDL_FRect rect = {
            world_x - entity->transform.scale.x * _mir->camera.zoom / 2,
            world_y - entity->transform.scale.y * _mir->camera.zoom / 2,
            entity->transform.scale.x * _mir->camera.zoom,
            entity->transform.scale.y * _mir->camera.zoom
        };
        
        SDL_RenderFillRect(_mir->renderer, &rect);
        
        // Рамка
        SDL_SetRenderDrawColor(_mir->renderer,
                              entity->sprite.color.r / 2,
                              entity->sprite.color.g / 2,
                              entity->sprite.color.b / 2,
                              entity->sprite.color.a);
        SDL_RenderRect(_mir->renderer, &rect);
    }
    
    // Пользовательская отрисовка
    if (entity->draw) {
        entity->draw(entity);
    }
}

static void MIR_DrawEntities(void) {
    if (!_mir_initialized || !_mir) return;
    
    // Сортировка по z-index
    for (int i = 0; i < _mir->entity_count - 1; i++) {
        for (int j = 0; j < _mir->entity_count - i - 1; j++) {
            if (_mir->entities[j]->sprite.z_index > 
                _mir->entities[j + 1]->sprite.z_index) {
                MIR_Entity* temp = _mir->entities[j];
                _mir->entities[j] = _mir->entities[j + 1];
                _mir->entities[j + 1] = temp;
            }
        }
    }
    
    // Отрисовка
    for (int i = 0; i < _mir->entity_count; i++) {
        MIR_DrawEntity(_mir->entities[i]);
    }
}

static void MIR_DrawRect(MIR_Rect rect, MIR_Color color) {
    if (!_mir_initialized || !_mir) return;
    
    SDL_SetRenderDrawColor(_mir->renderer, color.r, color.g, color.b, color.a);
    SDL_FRect sdl_rect = {rect.x, rect.y, rect.w, rect.h};
    SDL_RenderFillRect(_mir->renderer, &sdl_rect);
    _mir->draw_calls++;
}

static void MIR_DrawCircle(MIR_Vec2 center, float radius, MIR_Color color, int segments) {
    if (!_mir_initialized || !_mir || radius <= 0) return;
    
    if (segments < 8) segments = 8;
    if (segments > 64) segments = 64;
    
    SDL_SetRenderDrawColor(_mir->renderer, color.r, color.g, color.b, color.a);
    
    for (int i = 0; i < segments; i++) {
        float angle1 = (float)i / segments * 2 * 3.14159265f;
        float angle2 = (float)(i + 1) / segments * 2 * 3.14159265f;
        
        SDL_RenderLine(_mir->renderer,
                      center.x + cosf(angle1) * radius,
                      center.y + sinf(angle1) * radius,
                      center.x + cosf(angle2) * radius,
                      center.y + sinf(angle2) * radius);
    }
    _mir->draw_calls++;
}

static SDL_Texture* MIR_LoadTexture(const char* filepath) {
    if (!_mir_initialized || !_mir) return NULL;
    
    // Создаем поверхность из BMP файла (SDL3 пока не поддерживает PNG без SDL_image)
    SDL_Surface* surface = SDL_LoadBMP(filepath);
    if (!surface) {
        printf("[MIRULIT] Failed to load texture: %s\n", SDL_GetError());
        return NULL;
    }
    
    // Создаем текстуру из поверхности
    SDL_Texture* texture = SDL_CreateTextureFromSurface(_mir->renderer, surface);
    SDL_DestroySurface(surface);
    
    if (!texture) {
        printf("[MIRULIT] Failed to create texture: %s\n", SDL_GetError());
        return NULL;
    }
    
    printf("[MIRULIT] Texture loaded: %s\n", filepath);
    return texture;
}

static void MIR_DrawLine(MIR_Vec2 start, MIR_Vec2 end, MIR_Color color, float thickness) {
    if (!_mir_initialized || !_mir) return;
    
    SDL_SetRenderDrawColor(_mir->renderer, color.r, color.g, color.b, color.a);
    
    if (thickness <= 1.0f) {
        SDL_RenderLine(_mir->renderer, start.x, start.y, end.x, end.y);
    } else {
        // Простая реализация толстой линии через прямоугольник
        MIR_Vec2 dir = MIR_Vec2_Subtract(end, start);
        float length = sqrtf(dir.x * dir.x + dir.y * dir.y);
        if (length > 0) {
            dir.x /= length;
            dir.y /= length;
            
            // Перпендикуляр
            MIR_Vec2 perp = { -dir.y, dir.x };
            perp = MIR_Vec2_Multiply(perp, thickness / 2);
            
            // Углы прямоугольника
            MIR_Vec2 p1 = MIR_Vec2_Add(start, perp);
            MIR_Vec2 p2 = MIR_Vec2_Subtract(start, perp);
            MIR_Vec2 p3 = MIR_Vec2_Subtract(end, perp);
            MIR_Vec2 p4 = MIR_Vec2_Add(end, perp);
            
            // Отрисовка как заполненного четырёхугольника
            SDL_Vertex vertices[4] = {
                { {p1.x, p1.y}, {color.r, color.g, color.b, color.a}, {0, 0} },
                { {p2.x, p2.y}, {color.r, color.g, color.b, color.a}, {0, 0} },
                { {p4.x, p4.y}, {color.r, color.g, color.b, color.a}, {0, 0} },
                { {p3.x, p3.y}, {color.r, color.g, color.b, color.a}, {0, 0} }
            };
            
            int indices[6] = {0, 1, 2, 1, 3, 2};
            SDL_RenderGeometry(_mir->renderer, NULL, vertices, 4, indices, 6);
        }
    }
    _mir->draw_calls++;
}

#endif // MIRULIT_GRAPHICS_H
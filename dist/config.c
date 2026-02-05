#include <mirulit.h>

MIR_Entity* player = NULL;
MIR_Entity* enemy = NULL;
float spawn_timer = 0;
int score = 0;
int enemies_destroyed = 0;
bool game_paused = false;
bool is_fullscreen = false;

// Текстура игрока
SDL_Texture* player_texture = NULL;

// Глобальная переменная для скорости игрока
static MIR_Vec2 player_velocity = {0, 0};

// Функция для создания частиц ходьбы
void CreateWalkParticles(MIR_Vec2 position, MIR_Vec2 move_direction, int count) {
    for (int i = 0; i < count; i++) {
        // Частицы создаются под ногами игрока
        MIR_Vec2 particle_pos = {
            position.x + MIR_Math_RandomRange(-15, 15),
            position.y + 20 + MIR_Math_RandomRange(0, 10) // Ниже игрока
        };
        
        // Направление частиц ходьбы - противоположно направлению движения
        MIR_Vec2 particle_dir = {
            -move_direction.x * 0.8f + MIR_Math_RandomRange(-0.2f, 0.2f),
            -move_direction.y * 0.5f + MIR_Math_RandomRange(-0.1f, 0.3f)
        };
        
        // Нормализуем направление
        float dir_len = sqrtf(particle_dir.x * particle_dir.x + particle_dir.y * particle_dir.y);
        if (dir_len > 0) {
            particle_dir.x /= dir_len;
            particle_dir.y /= dir_len;
        }
        
        // Скорость частиц ходьбы
        MIR_Vec2 particle_vel = MIR_Vec2_Multiply(particle_dir, 60.0f + MIR_Math_RandomRange(-20, 20));
        
        // Цвет частиц ходьбы (серый/пыль)
        float gray_value = MIR_Math_RandomRange(150, 200);
        MIR_Color walk_color = {
            (uint8_t)gray_value,
            (uint8_t)gray_value,
            (uint8_t)gray_value,
            (uint8_t)MIR_Math_RandomRange(180, 230)
        };
        
        // Создаем частицу ходьбы
        MIR_EmitParticleEx(
            particle_pos,
            particle_vel,
            (MIR_Vec2){0, 30}, // Гравитация вниз
            walk_color,
            MIR_Math_RandomRange(2.0f, 4.0f),
            MIR_Math_RandomRange(0.4f, 0.8f)
        );
    }
}

void PlayerUpdate(MIR_Entity* self, float dt) {
    if (game_paused) return;
    
    float speed = 300.0f * dt;
    MIR_Vec2 move_dir = {0, 0};
    
    // Сохраняем позицию до движения для расчета скорости
    MIR_Vec2 prev_pos = self->transform.position;
    
    if (MIR_IsKeyDown(SDLK_W)) { 
        self->transform.position.y -= speed; 
        move_dir.y -= 1;
    }
    if (MIR_IsKeyDown(SDLK_S)) { 
        self->transform.position.y += speed; 
        move_dir.y += 1;
    }
    if (MIR_IsKeyDown(SDLK_A)) { 
        self->transform.position.x -= speed; 
        move_dir.x -= 1;
    }
    if (MIR_IsKeyDown(SDLK_D)) { 
        self->transform.position.x += speed; 
        move_dir.x += 1;
    }
    
    // Расчет реальной скорости игрока
    if (dt > 0) {
        player_velocity = MIR_Vec2_Multiply(
            MIR_Vec2_Subtract(self->transform.position, prev_pos),
            1.0f / dt
        );
    }
    
    // Границы
    self->transform.position.x = MIR_Math_Clamp(self->transform.position.x, 40, 760);
    self->transform.position.y = MIR_Math_Clamp(self->transform.position.y, 40, 560);
    
    // Слежение камеры
    MIR_SetCameraTarget(self->transform.position);
    
    // Вращение в сторону мыши
    MIR_Vec2 mouse_world = MIR_GetMouseWorldPosition();
    MIR_Vec2 dir = MIR_Vec2_Subtract(mouse_world, self->transform.position);
    self->transform.rotation = atan2f(dir.y, dir.x) * 180.0f / 3.14159f + 90.0f;
    
    // Создаем частицы ходьбы при движении
    float move_magnitude = sqrtf(move_dir.x * move_dir.x + move_dir.y * move_dir.y);
    if (move_magnitude > 0) {
        // Нормализуем направление движения
        MIR_Vec2 normalized_move_dir = move_dir;
        float move_len = sqrtf(move_dir.x * move_dir.x + move_dir.y * move_dir.y);
        if (move_len > 0) {
            normalized_move_dir.x /= move_len;
            normalized_move_dir.y /= move_len;
        }
        
        // Создаем частицы ходьбы
        int particle_count = (int)(3 + move_magnitude * 2);
        CreateWalkParticles(self->transform.position, normalized_move_dir, particle_count);
    }
}

void EnemyUpdate(MIR_Entity* self, float dt) {
    if (game_paused) return;
    
    // Движение к игроку
    if (player) {
        MIR_Vec2 dir = MIR_Vec2_Subtract(
            player->transform.position,
            self->transform.position
        );
        float dist = MIR_Math_Distance(
            player->transform.position,
            self->transform.position
        );
        
        if (dist > 0 && dist < 500) { // Только если игрок в радиусе 500 пикселей
            dir = MIR_Vec2_Multiply(MIR_Math_Normalize(dir), 80.0f * dt);
            self->transform.position = MIR_Vec2_Add(self->transform.position, dir);
        }
    }
    
    // Пульсация
    static float pulse_time = 0;
    pulse_time += dt;
    self->sprite.color.r = (uint8_t)(128 + sinf(pulse_time * 3) * 127);
}

void CheckCollisions() {
    if (game_paused) return;
    
    // Проверяем коллизии игрока с врагами
    if (player && player->active) {
        for (int i = 0; i < _mir->entity_count; i++) {
            MIR_Entity* enemy_entity = _mir->entities[i];
            if (!enemy_entity || !enemy_entity->active || 
                strcmp(enemy_entity->tag, "Enemy") != 0) continue;
            
            if (MIR_CheckCollision(player, enemy_entity)) {
                // Уничтожаем врага
                enemy_entity->active = false;
                enemies_destroyed++;
                
                // Эффект столкновения
                for (int k = 0; k < 30; k++) {
                    MIR_Vec2 vel = {
                        MIR_Math_RandomRange(-400, 400),
                        MIR_Math_RandomRange(-400, 400)
                    };
                    MIR_Color collision_color = {
                        255,
                        100,
                        100,
                        255
                    };
                    MIR_EmitParticleEx(
                        enemy_entity->transform.position,
                        vel,
                        (MIR_Vec2){0, 30}, // Гравитация
                        collision_color,
                        MIR_Math_RandomRange(3.0f, 8.0f),
                        MIR_Math_RandomRange(0.3f, 0.8f)
                    );
                }
            }
        }
    }
    
    // Удаляем неактивных врагов
    for (int i = _mir->entity_count - 1; i >= 0; i--) {
        if (_mir->entities[i] && !_mir->entities[i]->active && 
            strcmp(_mir->entities[i]->tag, "Enemy") == 0) {
            MIR_DestroyEntity(_mir->entities[i]);
        }
    }
}

int main(void) {
    // Инициализация движка
    if (!MIR_Init("Mirulit Engine - Space Shooter", 800, 600)) {
        return 1;
    }
    
    // Загрузка текстуры игрока
    player_texture = MIR_LoadTexture("engine/icons/64px.png");
    if (!player_texture) {
        printf("Failed to load player texture! Using default color.\n");
        // Если текстура не загрузилась, можно использовать цветной прямоугольник
    }
    
    // Создание игрока
    player = MIR_CreateEntity("Player");
    player->transform.position = (MIR_Vec2){400, 300};
    player->transform.scale = (MIR_Vec2){64, 64};
    
    // Настройка спрайта
    if (player_texture) {
        player->sprite.texture = player_texture;
        player->sprite.color = MIR_COLOR_WHITE; // Белый цвет для сохранения исходных цветов текстуры
        player->sprite.source_rect = (MIR_Rect){0, 0, 64, 64}; // Предполагаем, что текстура 64x64
    } else {
        player->sprite.color = MIR_COLOR_CYAN; // Цвет по умолчанию
    }
    
    player->update = PlayerUpdate;
    player->collider.bounds = (MIR_Rect){0, 0, 40, 40};
    player->collider.enabled = true;
    player->active = true;
    
    // Главный игровой цикл
    while (MIR_IsRunning()) {
        // Обработка ввода
        MIR_ProcessEvents();
        
        // Пауза по Escape
        if (MIR_IsKeyPressed(SDLK_ESCAPE)) {
            game_paused = !game_paused;
            printf("Game %s\n", game_paused ? "PAUSED" : "RESUMED");
        }
        
        // Fullscreen по F11
        if (MIR_IsKeyPressed(SDLK_F11)) {
            is_fullscreen = !is_fullscreen;
            SDL_SetWindowFullscreen(_mir->window, is_fullscreen);
            printf("Fullscreen: %s\n", is_fullscreen ? "ON" : "OFF");
        }
        
        // Замедление времени по Tab (только когда игра не на паузе)
        if (!game_paused) {
            if (MIR_IsKeyPressed(SDLK_TAB)) {
                MIR_SetTimeScale(0.2f);
            }
            if (MIR_IsKeyReleased(SDLK_TAB)) {
                MIR_SetTimeScale(1.0f);
            }
        }
        
        // Начало кадра
        MIR_BeginFrame();
        
        // Спавн врагов по таймеру (только во время игры)
        if (!game_paused) {
            spawn_timer += MIR_GetDeltaTime();
            if (spawn_timer > 2.0f && _mir->entity_count < 20) { // Не больше 20 врагов
                MIR_Entity* new_enemy = MIR_CreateEntity("Enemy");
                new_enemy->transform.position = (MIR_Vec2){
                    MIR_Math_RandomRange(50, 750),
                    MIR_Math_RandomRange(50, 550)
                };
                new_enemy->transform.scale = (MIR_Vec2){
                    MIR_Math_RandomRange(35, 65),
                    MIR_Math_RandomRange(35, 65)
                };
                new_enemy->sprite.color = (MIR_Color){
                    (uint8_t)MIR_Math_RandomRange(150, 255),
                    (uint8_t)MIR_Math_RandomRange(50, 100),
                    (uint8_t)MIR_Math_RandomRange(50, 100),
                    255
                };
                new_enemy->update = EnemyUpdate;
                new_enemy->collider.bounds = (MIR_Rect){0, 0, 
                    new_enemy->transform.scale.x, new_enemy->transform.scale.y};
                new_enemy->collider.enabled = true;
                new_enemy->active = true;
                spawn_timer = 0;
            }
        }
        
        // Обновление (если не на паузе)
        if (!game_paused) {
            MIR_UpdateEntities();
            MIR_UpdateParticles();
            CheckCollisions();
        }
        
        // Отрисовка прицела (только когда игра не на паузе)
        if (!game_paused) {
            MIR_Vec2 mouse_pos = MIR_GetMousePosition();
            MIR_DrawLine(
                (MIR_Vec2){mouse_pos.x - 12, mouse_pos.y},
                (MIR_Vec2){mouse_pos.x + 12, mouse_pos.y},
                (MIR_Color){255, 100, 100, 220},
                2.0f
            );
            MIR_DrawLine(
                (MIR_Vec2){mouse_pos.x, mouse_pos.y - 12},
                (MIR_Vec2){mouse_pos.x, mouse_pos.y + 12},
                (MIR_Color){255, 100, 100, 220},
                2.0f
            );
        }
        
        // Отрисовка
        MIR_DrawEntities();
        MIR_DrawParticles();
        
        // Отрисовка надписи PAUSE когда игра на паузе
        if (game_paused) {
            // Полупрозрачный черный фон
            MIR_DrawRect((MIR_Rect){0, 0, 800, 600}, (MIR_Color){0, 0, 0, 128});
            
            // Белый прямоугольник с текстом PAUSE
            MIR_DrawRect((MIR_Rect){300, 250, 200, 100}, MIR_COLOR_WHITE);
            
            // Черная рамка
            MIR_DrawRect((MIR_Rect){302, 252, 196, 96}, MIR_COLOR_BLACK);
            
            // Текст PAUSE (примитивная отрисовка)
            MIR_DrawRect((MIR_Rect){340, 270, 40, 10}, MIR_COLOR_WHITE);
            MIR_DrawRect((MIR_Rect){340, 290, 40, 10}, MIR_COLOR_WHITE);
            MIR_DrawRect((MIR_Rect){420, 270, 40, 10}, MIR_COLOR_WHITE);
            MIR_DrawRect((MIR_Rect){420, 290, 40, 10}, MIR_COLOR_WHITE);
        }
        
        // Отрисовка статистики в углу
        MIR_DrawRect((MIR_Rect){10, 10, 200, 70}, (MIR_Color){0, 0, 0, 180});
        
        // Вывод информации в консоль
        if (!game_paused) {
            printf("\rSCORE: %6d | ENEMIES: %3d | FPS: %3d", 
                   score, enemies_destroyed, MIR_GetFPS());
            fflush(stdout);
        } else {
            printf("\rPAUSED | FPS: %3d | Press ESC to resume", MIR_GetFPS());
            fflush(stdout);
        }
        
        // Конец кадра
        MIR_EndFrame();
    }
    
    // Очистка текстуры (если она не была привязана к сущности)
    if (player_texture) {
        SDL_DestroyTexture(player_texture);
        player_texture = NULL;
    }
    
    // Завершение
    MIR_Shutdown();
    printf("\nGame Over! Final Score: %d | Enemies Destroyed: %d\n", score, enemies_destroyed);
    return 0;
}
#ifndef MIRULIT_ENTITY_H
#define MIRULIT_ENTITY_H

// Компоненты сущности
typedef struct MIR_Transform {
    MIR_Vec2 position;
    MIR_Vec2 scale;
    float rotation;
    MIR_Vec2 velocity;
    MIR_Vec2 acceleration;
} MIR_Transform;

typedef struct MIR_Sprite {
    SDL_Texture* texture;
    MIR_Rect source_rect;
    MIR_Color color;
    int z_index;
    bool flip_x;
    bool flip_y;
} MIR_Sprite;

typedef struct MIR_Collider {
    MIR_Rect bounds;
    bool is_trigger;
    bool enabled;
    void (*on_collision)(struct MIR_Entity*, struct MIR_Entity*);
} MIR_Collider;

// Сущность (Entity)
struct MIR_Entity {
    int id;
    char tag[32];
    bool active;
    bool visible;
    bool persistent;
    
    MIR_Transform transform;
    MIR_Sprite sprite;
    MIR_Collider collider;
    
    void* components[10];
    int component_count;
    
    void (*update)(struct MIR_Entity*, float);
    void (*draw)(struct MIR_Entity*);
    void (*on_click)(struct MIR_Entity*);
    void (*on_destroy)(struct MIR_Entity*);
    
    struct MIR_Entity* parent;
    struct MIR_Entity* children[20];
    int child_count;
    
    void* user_data;
};

// Анимация
typedef struct {
    MIR_Rect frames[32];
    int frame_count;
    int current_frame;
    float frame_time;
    float timer;
    bool loop;
    bool playing;
} MIR_Animation;

#endif // MIRULIT_ENTITY_H
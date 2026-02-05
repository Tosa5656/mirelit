#ifndef MIRULIT_MATH_H
#define MIRULIT_MATH_H

// ==================== СТРУКТУРЫ ДАННЫХ ====================

// Базовые типы
typedef struct { float x, y; } MIR_Vec2;
typedef struct { float x, y, z; } MIR_Vec3;
typedef struct { uint8_t r, g, b, a; } MIR_Color;
typedef struct { float x, y, w, h; } MIR_Rect;
typedef struct { float r, g, b, a; } MIR_Colorf;

// ==================== МАТЕМАТИЧЕСКИЕ ФУНКЦИИ ====================

static inline float MIR_Math_Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static inline float MIR_Math_Clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static inline float MIR_Math_Distance(MIR_Vec2 a, MIR_Vec2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

static inline MIR_Vec2 MIR_Math_Normalize(MIR_Vec2 v) {
    float length = sqrtf(v.x * v.x + v.y * v.y);
    if (length > 0) {
        v.x /= length;
        v.y /= length;
    }
    return v;
}

static inline float MIR_Math_RandomRange(float min, float max) {
    return min + ((float)rand() / RAND_MAX) * (max - min);
}

static inline MIR_Vec2 MIR_Vec2_Add(MIR_Vec2 a, MIR_Vec2 b) {
    return (MIR_Vec2){a.x + b.x, a.y + b.y};
}

static inline MIR_Vec2 MIR_Vec2_Subtract(MIR_Vec2 a, MIR_Vec2 b) {
    return (MIR_Vec2){a.x - b.x, a.y - b.y};
}

static inline MIR_Vec2 MIR_Vec2_Multiply(MIR_Vec2 v, float s) {
    return (MIR_Vec2){v.x * s, v.y * s};
}

static inline float MIR_Vec2_Dot(MIR_Vec2 a, MIR_Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

#endif // MIRULIT_MATH_H
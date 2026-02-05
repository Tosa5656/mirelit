#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 objectColor;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main() {
    // Простое освещение
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 norm = normalize(Normal);
    
    // Диффузное освещение
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);
    
    // Фоновое освещение
    vec3 ambient = vec3(0.1, 0.1, 0.1);
    
    // Комбинируем
    vec3 result = (ambient + diffuse) * objectColor;
    
    FragColor = vec4(result, 1.0);
}
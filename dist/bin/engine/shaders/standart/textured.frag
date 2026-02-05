#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

struct Material {
    sampler2D diffuse;
    vec3 specular;
    float shininess;
};

struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform Material material;
uniform Light light;
uniform vec3 viewPos;
uniform bool useTexture;
uniform vec3 objectColor;

void main()
{
    // Ambient
    vec3 ambient;
    if (useTexture) {
        ambient = light.ambient * texture(material.diffuse, TexCoords).rgb;
    } else {
        ambient = light.ambient * objectColor;
    }
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    
    vec3 diffuse;
    if (useTexture) {
        diffuse = light.diffuse * diff * texture(material.diffuse, TexCoords).rgb;
    } else {
        diffuse = light.diffuse * diff * objectColor;
    }
    
    // Specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * material.specular;
    
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
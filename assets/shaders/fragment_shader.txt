#version 330 core

out vec4 FragColor;

// Wejścia z vertex shadera
in vec3 FragPos_World;
in vec3 Normal_World;
in vec2 TexCoords;
in vec4 VertexColor_FS; // Kolor wierzchołka z VS

// --- Stałe ---
// MUSZĄ być zsynchronizowane z C++ (Lighting.h)
const int MAX_POINT_LIGHTS = 4;
const int MAX_SPOT_LIGHTS = 4;

// --- Struktury ---
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

struct DirectionalLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    bool enabled;
};

struct PointLight {
    vec3 position;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    bool enabled;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    bool enabled;
};

// --- Uniformy ---
uniform Material material;
uniform vec3 viewPos_World;
uniform bool u_UseFlatShading;

uniform DirectionalLight dirLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];

uniform int numPointLights; // Aktualna liczba aktywnych świateł punktowych
uniform int numSpotLights;  // Aktualna liczba aktywnych reflektorów

// --- Funkcje pomocnicze do obliczania światła ---

// Oblicza wkład światła kierunkowego
vec3 CalculateDirLight(DirectionalLight light, vec3 normal, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);

    vec3 ambient = light.ambient * material.ambient;
    
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);
    
    return (ambient + diffuse + specular);
}

// Oblicza wkład światła punktowego
vec3 CalculatePointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);

    vec3 lightDir = normalize(light.position - fragPos);
    
    // Ambient
    vec3 ambient = light.ambient * material.ambient;
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);
    
    // Tłumienie (Attenuation)
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    
    return (ambient + diffuse + specular);
}

// Oblicza wkład reflektora
vec3 CalculateSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);

    vec3 lightDir = normalize(light.position - fragPos);
    
    // Sprawdzenie, czy fragment jest w stożku światła
    float theta = dot(lightDir, normalize(-light.direction));
    float intensity = 0.0;

    if (theta > light.outerCutOff) { // Używamy outerCutOff do szybkiego odrzucenia
        intensity = smoothstep(light.outerCutOff, light.cutOff, theta);
    }
    // Starsza metoda:
    // if (theta > light.cutOff) { 
    //     intensity = 1.0;
    // }
    // // Płynne przejście na krawędziach (penumbra)
    // float epsilon = light.cutOff - light.outerCutOff;
    // intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);


    if (intensity <= 0.0) return vec3(0.0); // Poza stożkiem

    // Ambient
    vec3 ambient = light.ambient * material.ambient;
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);
    
    // Tłumienie
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
        
    return (ambient + diffuse + specular);
}


void main() {
    if (u_UseFlatShading) {
        // Tryb "Flat Shading" (Unlit): Użyj bezpośrednio VertexColor_FS
        // Jeśli VertexColor_FS nie został ustawiony (np. alfa = 0), można użyć domyślnego koloru
        if (VertexColor_FS.a < 0.01) { // Sprawdzenie alfy jako wskaźnika "nieustawionego" koloru
             FragColor = vec4(0.8, 0.8, 0.8, 1.0); // Domyślny szary
        } else {
             FragColor = VertexColor_FS;
        }
    } else {
        // Tryb Smooth Shading (Oświetlenie Phonga z wieloma światłami)
        vec3 norm = normalize(Normal_World); // Normalna zinterpolowana z VS
        vec3 viewDir = normalize(viewPos_World - FragPos_World);
        
        vec3 resultColor = vec3(0.0); // Inicjalizacja koloru wynikowego

        // 1. Światło kierunkowe
        resultColor += CalculateDirLight(dirLight, norm, viewDir);
        
        // 2. Światła punktowe
        for (int i = 0; i < numPointLights; i++) {
            if(i < MAX_POINT_LIGHTS) { // Dodatkowe zabezpieczenie, choć pętla powinna być poprawna
                 resultColor += CalculatePointLight(pointLights[i], norm, FragPos_World, viewDir);
            }
        }
            
        // 3. Reflektory
        for (int i = 0; i < numSpotLights; i++) {
             if(i < MAX_SPOT_LIGHTS) { // Dodatkowe zabezpieczenie
                resultColor += CalculateSpotLight(spotLights[i], norm, FragPos_World, viewDir);
            }
        }
        
        FragColor = vec4(resultColor, 1.0);
        // Można dodać obsługę tekstur, np. mnożąc resultColor przez texture(textureSampler, TexCoords).rgb
    }
}

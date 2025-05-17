#version 330 core

out vec4 FragColor;

// Wejścia z vertex shadera
in vec3 FragPos_World;
in vec3 Normal_World;
in vec2 TexCoords;
in vec4 VertexColor_FS;
in vec4 FragPosLightSpace;

// --- Stałe ---
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

uniform int numPointLights;
uniform int numSpotLights;

uniform sampler2D shadowMap;
uniform float shadowMapTexelSize;
uniform int u_pcfRadius; // NOWY UNIFORM: Promień PCF (0=1x1, 1=3x3, 2=5x5 itd.)

// --- Funkcja obliczająca współczynnik cienia z konfigurowalnym PCF ---
float CalculateShadowFactorPCF(vec4 fragPosLightSpace, vec3 normalWorld, vec3 lightDirWorld) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normalWorld, lightDirWorld)), 0.0005);

    float shadow = 0.0;
    float totalSamples = 0.0;

    // Pętla PCF używająca u_pcfRadius
    // Kernel będzie miał rozmiar (2 * u_pcfRadius + 1) x (2 * u_pcfRadius + 1)
    // Np. u_pcfRadius = 0 -> pętla od 0 do 0 -> 1 próbka
    // Np. u_pcfRadius = 1 -> pętla od -1 do 1 -> 9 próbek (3x3)
    // Np. u_pcfRadius = 2 -> pętla od -2 do 2 -> 25 próbek (5x5)
    for (int x = -u_pcfRadius; x <= u_pcfRadius; ++x) {
        for (int y = -u_pcfRadius; y <= u_pcfRadius; ++y) {
            // Przesunięcie próbki o wielokrotność rozmiaru teksla
            vec2 offset = vec2(x, y) * shadowMapTexelSize;
            float pcfDepth = texture(shadowMap, projCoords.xy + offset).r;
            if (currentDepth - bias > pcfDepth) {
                shadow += 1.0;
            }
            totalSamples += 1.0;
        }
    }

    if (totalSamples > 0.0) {
        shadow /= totalSamples;
    }
    
    if(projCoords.z > 1.0){
        shadow = 0.0;
    }

    return shadow;
}

// --- Funkcje pomocnicze do obliczania światła (bez zmian w logice, tylko wywołanie CalculateShadowFactorPCF) ---
vec3 CalculateDirLight(DirectionalLight light, vec3 normal, vec3 viewDir, float shadowFactor) {
    if (!light.enabled) return vec3(0.0);
    vec3 ambient = light.ambient * material.ambient;
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);
    return (ambient + (1.0 - shadowFactor) * (diffuse + specular));
}

vec3 CalculatePointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);
    vec3 lightDir = normalize(light.position - fragPos);
    vec3 ambient = light.ambient * material.ambient;
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

vec3 CalculateSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);
    vec3 lightDir = normalize(light.position - fragPos);
    float theta = dot(lightDir, normalize(-light.direction));
    float intensity = 0.0;
    if (theta > light.outerCutOff) {
        intensity = smoothstep(light.outerCutOff, light.cutOff, theta);
    }
    if (intensity <= 0.0) return vec3(0.0);
    vec3 ambient = light.ambient * material.ambient;
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    return (ambient + diffuse + specular);
}

void main() {
    if (u_UseFlatShading) {
        if (VertexColor_FS.a < 0.01) {
             FragColor = vec4(0.8, 0.8, 0.8, 1.0);
        } else {
             FragColor = VertexColor_FS;
        }
    } else {
        vec3 norm_world_normalized = normalize(Normal_World);
        vec3 viewDir_world_normalized = normalize(viewPos_World - FragPos_World);
        
        float shadowFactor = 0.0;
        if (dirLight.enabled) {
            vec3 lightDirToSource = normalize(-dirLight.direction); 
            shadowFactor = CalculateShadowFactorPCF(FragPosLightSpace, norm_world_normalized, lightDirToSource);
        }

        vec3 resultColor = vec3(0.0);
        resultColor += CalculateDirLight(dirLight, norm_world_normalized, viewDir_world_normalized, shadowFactor);
        
        for (int i = 0; i < numPointLights; i++) {
            if(i < MAX_POINT_LIGHTS) {
                 resultColor += CalculatePointLight(pointLights[i], norm_world_normalized, FragPos_World, viewDir_world_normalized);
            }
        }
            
        for (int i = 0; i < numSpotLights; i++) {
             if(i < MAX_SPOT_LIGHTS) {
                resultColor += CalculateSpotLight(spotLights[i], norm_world_normalized, FragPos_World, viewDir_world_normalized);
            }
        }
        
        FragColor = vec4(resultColor, 1.0);
    }
}

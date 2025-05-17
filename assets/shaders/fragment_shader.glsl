#version 330 core

out vec4 FragColor;

in vec3 FragPos_World;
in vec3 Normal_World;
in vec2 TexCoords;
in vec4 VertexColor_FS;
in vec4 FragPosDirLightSpace; 

// Tablica pozycji fragmentu w przestrzeniach świateł reflektorów
// Rozmiar musi odpowiadać MAX_SHADOW_CASTING_SPOT_LIGHTS_FS
in vec4 FragPosSpotLightSpace[2]; // Załóżmy MAX_SHADOW_CASTING_SPOT_LIGHTS_FS = 2


const int MAX_POINT_LIGHTS = 4; 
const int MAX_SPOT_LIGHTS_TOTAL = 4;  
const int MAX_SHADOW_CASTING_SPOT_LIGHTS_FS = 2; // Musi być zgodne z C++ (Lighting.h) i VS

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

// ZMODYFIKOWANA STRUKTURA: Dodano texelSize
struct SpotLightShadowData {
    sampler2D shadowMap;
    bool enabled; 
    float texelSize; // Rozmiar teksla dla tej konkretnej mapy cieni reflektora
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
    bool castsShadow;     
    int shadowDataIndex; 
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

uniform Material material;
uniform vec3 viewPos_World;
uniform bool u_UseFlatShading;

uniform DirectionalLight dirLight;
uniform sampler2D dirShadowMap; // Mapa cieni dla światła kierunkowego

uniform SpotLight spotLights[MAX_SPOT_LIGHTS_TOTAL]; 
uniform int numSpotLights; 

uniform SpotLightShadowData spotLightShadowData[MAX_SHADOW_CASTING_SPOT_LIGHTS_FS];
uniform int numActiveSpotShadowCastersFS; 

uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int numPointLights;

uniform float shadowMapTexelSize; // Globalny texel size (używany dla dirLight)
uniform int u_pcfRadius; 

// ZMODYFIKOWANA FUNKCJA: Przyjmuje specificTexelSize
float CalculateShadowFactorPCF(vec4 fragPosLight, sampler2D specificShadowMap, float specificTexelSize, vec3 normalWorld, vec3 lightDirWorld) {
    vec3 projCoords = fragPosLight.xyz / fragPosLight.w;
    projCoords = projCoords * 0.5 + 0.5;
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normalWorld, lightDirWorld)), 0.0005);
    float shadow = 0.0;
    float totalSamples = 0.0;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0; // Poza frustum światła, brak cienia
    }

    for (int x = -u_pcfRadius; x <= u_pcfRadius; ++x) {
        for (int y = -u_pcfRadius; y <= u_pcfRadius; ++y) {
            vec2 offset = vec2(x, y) * specificTexelSize; // Użyj specificTexelSize
            float pcfDepth = texture(specificShadowMap, projCoords.xy + offset).r;
            if (currentDepth - bias > pcfDepth) {
                shadow += 1.0;
            }
            totalSamples += 1.0;
        }
    }
    if (totalSamples > 0.0) shadow /= totalSamples;
    return shadow;
}

vec3 CalculateDirLightContribution(DirectionalLight light, vec3 normal, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);
    float shadowFactor = 0.0;
    vec3 lightDirToSource = normalize(-light.direction);
    // Użyj globalnego shadowMapTexelSize dla światła kierunkowego
    shadowFactor = CalculateShadowFactorPCF(FragPosDirLightSpace, dirShadowMap, shadowMapTexelSize, normal, lightDirToSource);

    vec3 ambient = light.ambient * material.ambient;
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);
    return (ambient + (1.0 - shadowFactor) * (diffuse + specular));
}

vec3 CalculateSpotLightContribution(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);

    vec3 lightToFragDir = normalize(light.position - fragPos); 
    float theta = dot(lightToFragDir, normalize(-light.direction)); 
    float intensity = 0.0;

    if (theta > light.outerCutOff) { 
        intensity = smoothstep(light.outerCutOff, light.cutOff, theta);
    }
    if (intensity <= 0.0) return vec3(0.0);

    float shadowFactor = 0.0;
    if (light.castsShadow && light.shadowDataIndex >= 0 && light.shadowDataIndex < numActiveSpotShadowCastersFS) {
        // Sprawdź, czy dane cienia dla tego slotu są aktywne
        if (spotLightShadowData[light.shadowDataIndex].enabled) {
             shadowFactor = CalculateShadowFactorPCF(
                FragPosSpotLightSpace[light.shadowDataIndex], 
                spotLightShadowData[light.shadowDataIndex].shadowMap, 
                spotLightShadowData[light.shadowDataIndex].texelSize, // Użyj dedykowanego texelSize
                normal,
                lightToFragDir 
            );
        }
    }

    vec3 ambient = light.ambient * material.ambient;
    float diff = max(dot(normal, lightToFragDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    vec3 reflectDir = reflect(-lightToFragDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);
    
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    
    ambient *= attenuation * intensity; 
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
        
    return (ambient + (1.0 - shadowFactor) * (diffuse + specular));
}


vec3 CalculatePointLightContribution(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
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

void main() {
    if (u_UseFlatShading) {
        FragColor = (VertexColor_FS.a < 0.01) ? vec4(0.8, 0.8, 0.8, 1.0) : VertexColor_FS;
    } else {
        vec3 norm_world = normalize(Normal_World);
        vec3 viewDir_world = normalize(viewPos_World - FragPos_World);
        vec3 resultColor = vec3(0.0);

        resultColor += CalculateDirLightContribution(dirLight, norm_world, viewDir_world);
        
        for (int i = 0; i < numSpotLights; i++) { 
            if (i < MAX_SPOT_LIGHTS_TOTAL) { 
                resultColor += CalculateSpotLightContribution(spotLights[i], norm_world, FragPos_World, viewDir_world);
            }
        }
            
        for (int i = 0; i < numPointLights; i++) {
            if (i < MAX_POINT_LIGHTS) {
                 resultColor += CalculatePointLightContribution(pointLights[i], norm_world, FragPos_World, viewDir_world);
            }
        }
        FragColor = vec4(resultColor, 1.0);
    }
}

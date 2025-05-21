#version 330 core

out vec4 FragColor;

// Dane wejściowe z Vertex Shadera
in vec3 FragPos_World;
in vec3 Normal_World;
in vec2 TexCoords;         // Współrzędne tekstury
in vec4 VertexColor_FS;    // Kolor wierzchołka (może być używany lub nie)
in vec4 FragPosDirLightSpace; 

// Dla cieni reflektorów
in vec4 FragPosSpotLightSpace[2]; // MAX_SHADOW_CASTING_SPOT_LIGHTS_FS = 2

// Stałe (powinny być zsynchronizowane z C++)
const int MAX_POINT_LIGHTS_FS = 4; 
const int MAX_SPOT_LIGHTS_TOTAL_FS = 4;
const int MAX_SHADOW_CASTING_SPOT_LIGHTS_FS = 2;
const int MAX_SHADOW_CASTING_POINT_LIGHTS_FS = 1; 

// Struktura Materiału - przywrócone nazwy dla kolorów bazowych
struct Material {
    vec3 ambient;           // Kolor otoczenia materiału (fallback)
    vec3 diffuse;           // Kolor rozproszenia materiału (fallback)
    vec3 specular;          // Kolor odbicia lustrzanego materiału (fallback)
    float shininess;

    sampler2D diffuseTexture;   // Tekstura diffuse (kolor bazowy)
    sampler2D specularTexture;  // Tekstura specular (mapa intensywności odbić)

    bool useDiffuseTexture;
    bool useSpecularTexture;
};

// Struktury świateł (pozostają takie same jak w dostarczonym pliku)
struct DirectionalLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    bool enabled;
};

struct SpotLightShadowData { 
    sampler2D shadowMap;
    bool enabled;
    float texelSize;
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

struct PointLightShadowData {
    samplerCube shadowCubeMap; 
    float farPlane;            
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
    bool castsShadow;       
    int shadowDataIndex;    
};

// Uniformy
uniform Material material; // Teraz zawiera samplery i flagi użycia tekstur
uniform vec3 viewPos_World;
uniform bool u_UseFlatShading;

// Światło kierunkowe i jego cień
uniform DirectionalLight dirLight;
uniform sampler2D dirShadowMap;
uniform float shadowMapTexelSize; 
uniform int u_pcfRadius;          

// Reflektory i ich cienie
uniform SpotLight spotLights[MAX_SPOT_LIGHTS_TOTAL_FS];
uniform int numSpotLights; 
uniform SpotLightShadowData spotLightShadowData[MAX_SHADOW_CASTING_SPOT_LIGHTS_FS];

// Światła punktowe i ich cienie
uniform PointLight pointLights[MAX_POINT_LIGHTS_FS];
uniform int numPointLights; 
uniform PointLightShadowData pointLightShadowData[MAX_SHADOW_CASTING_POINT_LIGHTS_FS];


// Funkcja PCF dla map 2D (bez zmian z dostarczonego pliku)
float CalculateShadowFactorPCF_2D(vec4 fragPosLightSpace, sampler2D specificShadowMap, float specificTexelSize, vec3 normalWorld, vec3 lightDirWorld) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5; 
    float currentDepth = projCoords.z;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0; 
    }

    float shadow = 0.0;
    float bias = max(0.005 * (1.0 - dot(normalWorld, lightDirWorld)), 0.0005); 
    
    float totalSamples = 0.0;
    for (int x = -u_pcfRadius; x <= u_pcfRadius; ++x) {
        for (int y = -u_pcfRadius; y <= u_pcfRadius; ++y) {
            vec2 offset = vec2(x, y) * specificTexelSize;
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

// Funkcja cienia dla światła punktowego (bez zmian z dostarczonego pliku)
float CalculatePointShadowFactor(vec3 fragPos_World, PointLight currentPointLight, PointLightShadowData shadowData) {
    vec3 fragToLightVector = fragPos_World - currentPointLight.position;
    float currentDepthLinear = length(fragToLightVector);
    float closestDepthNormalized = texture(shadowData.shadowCubeMap, fragToLightVector).r;
    float closestDepthLinear = closestDepthNormalized * shadowData.farPlane; 
    float shadow = 0.0;
    float bias = 0.05; 
    if (currentDepthLinear - bias > closestDepthLinear) {
        shadow = 1.0; 
    }
    return shadow; 
}


// ZMODYFIKOWANE FUNKCJE OBLICZAJĄCE WKŁAD ŚWIATŁA
// Teraz przyjmują kolory materiału jako parametry

vec3 CalculateDirLightContribution(DirectionalLight light, vec3 normal, vec3 viewDir, 
                                   vec3 currentMaterialAmbient, vec3 currentMaterialDiffuse, vec3 currentMaterialSpecular, float currentMaterialShininess) {
    if (!light.enabled) return vec3(0.0);
    
    vec3 lightDirToSource = normalize(-light.direction);
    float shadowFactor = CalculateShadowFactorPCF_2D(FragPosDirLightSpace, dirShadowMap, shadowMapTexelSize, normal, lightDirToSource);

    vec3 ambient = light.ambient * currentMaterialAmbient;
    float diff = max(dot(normal, lightDirToSource), 0.0);
    vec3 diffuse = light.diffuse * (diff * currentMaterialDiffuse);
    
    vec3 reflectDir = reflect(light.direction, normal); 
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), currentMaterialShininess);
    vec3 specular = light.specular * (spec * currentMaterialSpecular);
    
    return (ambient + (1.0 - shadowFactor) * (diffuse + specular));
}

vec3 CalculateSpotLightContribution(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir,
                                    vec3 currentMaterialAmbient, vec3 currentMaterialDiffuse, vec3 currentMaterialSpecular, float currentMaterialShininess) {
    if (!light.enabled) return vec3(0.0);

    vec3 lightToFragDir = normalize(light.position - fragPos); 
    float theta = dot(lightToFragDir, normalize(-light.direction)); 
    float intensity = 0.0;

    if (theta > light.outerCutOff) {
        intensity = smoothstep(light.outerCutOff, light.cutOff, theta);
    }
    if (intensity <= 0.0) return vec3(0.0);

    float shadowFactor = 0.0;
    if (light.castsShadow && light.shadowDataIndex >= 0 && light.shadowDataIndex < MAX_SHADOW_CASTING_SPOT_LIGHTS_FS) {
        if (spotLightShadowData[light.shadowDataIndex].enabled) {
             shadowFactor = CalculateShadowFactorPCF_2D(
                FragPosSpotLightSpace[light.shadowDataIndex],
                spotLightShadowData[light.shadowDataIndex].shadowMap,
                spotLightShadowData[light.shadowDataIndex].texelSize,
                normal,
                lightToFragDir 
            );
        }
    }

    vec3 ambient = light.ambient * currentMaterialAmbient;
    float diff = max(dot(normal, lightToFragDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * currentMaterialDiffuse);
    
    vec3 reflectDir = reflect(-lightToFragDir, normal); 
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), currentMaterialShininess);
    vec3 specular = light.specular * (spec * currentMaterialSpecular);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;

    return (ambient + (1.0 - shadowFactor) * (diffuse + specular));
}

vec3 CalculatePointLightContribution(PointLight light, int lightIndex, vec3 normal, vec3 fragPos, vec3 viewDir,
                                     vec3 currentMaterialAmbient, vec3 currentMaterialDiffuse, vec3 currentMaterialSpecular, float currentMaterialShininess) {
    if (!light.enabled) return vec3(0.0);

    vec3 lightToFragDir = normalize(light.position - fragPos); 

    float shadowFactor = 0.0;
    if (light.castsShadow && light.shadowDataIndex >= 0 && light.shadowDataIndex < MAX_SHADOW_CASTING_POINT_LIGHTS_FS) {
        if (pointLightShadowData[light.shadowDataIndex].enabled) {
            shadowFactor = CalculatePointShadowFactor(fragPos, light, pointLightShadowData[light.shadowDataIndex]);
        }
    }

    vec3 ambient = light.ambient * currentMaterialAmbient;
    float diff = max(dot(normal, lightToFragDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * currentMaterialDiffuse);

    vec3 reflectDir = reflect(-lightToFragDir, normal); 
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), currentMaterialShininess);
    vec3 specular = light.specular * (spec * currentMaterialSpecular);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return (ambient + (1.0 - shadowFactor) * (diffuse + specular));
}

void main() {
    // Określenie efektywnych właściwości materiału
    vec3 effectiveAmbient;
    vec3 effectiveDiffuse;
    vec3 effectiveSpecular;
    float effectiveShininess = material.shininess; // Shininess zazwyczaj nie pochodzi bezpośrednio z tekstury

    // Ambient: często modulowany przez teksturę diffuse lub bazowy kolor ambient
    if (material.useDiffuseTexture) {
        // Moduluj bazowy kolor ambient materiału przez kolor z tekstury diffuse
        effectiveAmbient = material.ambient * texture(material.diffuseTexture, TexCoords).rgb; 
    } else {
        effectiveAmbient = material.ambient; // Użyj bazowego koloru ambient z uniformu Material
    }

    // Diffuse: z tekstury lub fallback
    if (material.useDiffuseTexture) {
        effectiveDiffuse = texture(material.diffuseTexture, TexCoords).rgb;
    } else {
        effectiveDiffuse = material.diffuse; // Użyj bazowego koloru diffuse z uniformu Material
    }

    // Specular: z tekstury lub fallback
    if (material.useSpecularTexture) {
        // Zakładamy, że tekstura specular dostarcza pełny kolor RGB
        effectiveSpecular = texture(material.specularTexture, TexCoords).rgb;
        // Jeśli tekstura specular to mapa intensywności (skala szarości), można zrobić:
        // effectiveSpecular = material.specular * texture(material.specularTexture, TexCoords).r;
    } else {
        effectiveSpecular = material.specular; // Użyj bazowego koloru specular z uniformu Material
    }
    

    if (u_UseFlatShading) {
        // Dla trybu "unlit", użyj efektywnego koloru diffuse
        FragColor = vec4(effectiveDiffuse, 1.0);
    } else {
        vec3 norm_world = normalize(Normal_World);
        vec3 viewDir_world = normalize(viewPos_World - FragPos_World); 
        vec3 resultColor = vec3(0.0);

        // Obliczanie wkładu od każdego typu światła z użyciem efektywnych właściwości materiału
        resultColor += CalculateDirLightContribution(dirLight, norm_world, viewDir_world, 
                                                    effectiveAmbient, effectiveDiffuse, effectiveSpecular, effectiveShininess);

        for (int i = 0; i < numSpotLights; i++) {
            if (i < MAX_SPOT_LIGHTS_TOTAL_FS) { 
                resultColor += CalculateSpotLightContribution(spotLights[i], norm_world, FragPos_World, viewDir_world,
                                                            effectiveAmbient, effectiveDiffuse, effectiveSpecular, effectiveShininess);
            }
        }

        for (int i = 0; i < numPointLights; i++) {
            if (i < MAX_POINT_LIGHTS_FS) { 
                 resultColor += CalculatePointLightContribution(pointLights[i], i, norm_world, FragPos_World, viewDir_world,
                                                              effectiveAmbient, effectiveDiffuse, effectiveSpecular, effectiveShininess);
            }
        }
        FragColor = vec4(resultColor, 1.0);
    }
}

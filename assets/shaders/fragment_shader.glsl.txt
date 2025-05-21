#version 330 core

out vec4 FragColor;

in vec3 FragPos_World;
in vec3 Normal_World;
in vec2 TexCoords;
in vec4 VertexColor_FS; // Kolor wierzchołka z VS
in vec4 FragPosDirLightSpace; // Pozycja fragmentu w przestrzeni światła kierunkowego

// Dla cieni reflektorów (bez zmian)
in vec4 FragPosSpotLightSpace[2]; // MAX_SHADOW_CASTING_SPOT_LIGHTS_FS = 2

// Stałe (powinny być zsynchronizowane z C++)
const int MAX_POINT_LIGHTS_FS = 4; // Całkowita liczba świateł punktowych
const int MAX_SPOT_LIGHTS_TOTAL_FS = 4;
const int MAX_SHADOW_CASTING_SPOT_LIGHTS_FS = 2;

// NOWA STAŁA: Maksymalna liczba świateł punktowych rzucających cień
const int MAX_SHADOW_CASTING_POINT_LIGHTS_FS = 1; // Musi być zgodne z Engine.h i Lighting.h

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
    // sampler2D diffuseTexture; // Jeśli używasz tekstur materiału
    // sampler2D specularTexture;
    // bool useDiffuseTexture;
    // bool useSpecularTexture;
};

struct DirectionalLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    bool enabled;
};

struct SpotLightShadowData { // Bez zmian
    sampler2D shadowMap;
    bool enabled;
    float texelSize;
};

struct SpotLight { // Bez zmian
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
    int shadowDataIndex; // Indeks do tablicy spotLightShadowData
};

// NOWA STRUKTURA: Dla danych cienia światła punktowego
struct PointLightShadowData {
    samplerCube shadowCubeMap; // Mapa sześcienna cieni
    float farPlane;            // Płaszczyzna daleka użyta do generowania mapy
    bool enabled;              // Czy ten slot danych cienia jest aktywny
    // Nie potrzebujemy texelSize dla standardowego próbkowania mapy sześciennej,
    // ale może być potrzebne dla zaawansowanych technik PCF.
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
    // NOWE POLA: Dla obsługi cieni
    bool castsShadow;       // Czy to światło rzuca cień
    int shadowDataIndex;    // Indeks do tablicy pointLightShadowData
                            // shadowFarPlane jest teraz w PointLightShadowData
};

uniform Material material;
uniform vec3 viewPos_World;
uniform bool u_UseFlatShading; // Dla trybu "unlit"

// Światło kierunkowe i jego cień (bez zmian)
uniform DirectionalLight dirLight;
uniform sampler2D dirShadowMap;
uniform float shadowMapTexelSize; // Dla PCF światła kierunkowego
uniform int u_pcfRadius;          // Dla PCF

// Reflektory i ich cienie (bez zmian)
uniform SpotLight spotLights[MAX_SPOT_LIGHTS_TOTAL_FS];
uniform int numSpotLights; // Całkowita liczba aktywnych reflektorów
uniform SpotLightShadowData spotLightShadowData[MAX_SHADOW_CASTING_SPOT_LIGHTS_FS];
// numActiveSpotShadowCastersFS jest używany w CalculateSpotLightContribution

// Światła punktowe
uniform PointLight pointLights[MAX_POINT_LIGHTS_FS];
uniform int numPointLights; // Całkowita liczba aktywnych świateł punktowych

// NOWE UNIFORMY: Dla cieni świateł punktowych
uniform PointLightShadowData pointLightShadowData[MAX_SHADOW_CASTING_POINT_LIGHTS_FS];
// numActivePointShadowCastersFS (przekazywany z Engine) informuje, ile z tych slotów jest używanych


// Funkcja PCF dla map 2D (bez zmian)
float CalculateShadowFactorPCF_2D(vec4 fragPosLightSpace, sampler2D specificShadowMap, float specificTexelSize, vec3 normalWorld, vec3 lightDirWorld) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5; // Transformacja do [0,1]
    float currentDepth = projCoords.z;

    // Sprawdź, czy fragment jest poza frustum światła (dla map 2D)
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0; // Poza frustum, brak cienia (w pełni oświetlony z tego światła)
    }

    float shadow = 0.0;
    float bias = max(0.005 * (1.0 - dot(normalWorld, lightDirWorld)), 0.0005); // Shadow acne bias
    
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
    return shadow; // 0.0 = oświetlony, 1.0 = w cieniu
}

// NOWA FUNKCJA: Obliczanie współczynnika cienia dla światła punktowego (mapa sześcienna)
float CalculatePointShadowFactor(vec3 fragPos_World, PointLight currentPointLight, PointLightShadowData shadowData) {
    vec3 fragToLightVector = fragPos_World - currentPointLight.position;
    // Głębokość to odległość od światła do fragmentu
    float currentDepthLinear = length(fragToLightVector);

    // Pobierz głębokość z mapy sześciennej.
    // Tekstura mapy sześciennej przechowuje znormalizowaną głębokość (odległość / farPlane).
    float closestDepthNormalized = texture(shadowData.shadowCubeMap, fragToLightVector).r;
    float closestDepthLinear = closestDepthNormalized * shadowData.farPlane; // Przekształć na odległość w przestrzeni świata

    float shadow = 0.0;
    // Bias dla map sześciennych może wymagać dostosowania.
    // Prosty bias stały lub zależny od odległości.
    float bias = 0.05; // Dostosuj ten bias! Może być za mały lub za duży.
                       // Dla map sześciennych, bias może być bardziej skomplikowany.
                       // np. bias = 0.05 + 0.05 * (currentDepthLinear / shadowData.farPlane);

    if (currentDepthLinear - bias > closestDepthLinear) {
        shadow = 1.0; // Fragment jest w cieniu
    }

    // Prosty PCF dla map sześciennych (bardzo podstawowy, można rozbudować)
    // Wymaga próbkowania sąsiadów w 3D, co jest bardziej kosztowne.
    // Na razie zostawmy twarde cienie dla uproszczenia.
    // Jeśli chcesz PCF, musiałbyś dodać pętlę i próbkować wokół fragToLightVector.
    // vec3 sampleOffsetDirections[NUM_PCF_SAMPLES_CUBE] = { ... };
    // float pcfShadow = 0.0;
    // int samplesTaken = 0;
    // if (u_pcfRadius > 0) { // Jeśli PCF jest włączone
    //     float diskRadius = 0.05; // Rozmiar obszaru próbkowania wokół kierunku
    //     for (int i = 0; i < 4; ++i) { // Przykładowe 4 próbki
    //         // Wygeneruj offsety w przestrzeni stycznej do sfery
    //         // To jest uproszczenie, prawdziwy PCF dla cubemap jest bardziej złożony
    //         vec3 neighborFragToLight = fragToLightVector + sampleOffsetDirections[i] * diskRadius;
    //         closestDepthNormalized = texture(shadowData.shadowCubeMap, neighborFragToLight).r;
    //         closestDepthLinear = closestDepthNormalized * shadowData.farPlane;
    //         if(currentDepthLinear - bias > closestDepthLinear) {
    //             pcfShadow += 1.0;
    //         }
    //         samplesTaken++;
    //     }
    //     if(samplesTaken > 0) shadow = pcfShadow / samplesTaken;
    // }


    return shadow; // 0.0 = brak cienia, 1.0 = w cieniu
}


// Obliczanie wkładu światła kierunkowego (bez zmian, używa CalculateShadowFactorPCF_2D)
vec3 CalculateDirLightContribution(DirectionalLight light, vec3 normal, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);
    float shadowFactor = 0.0;
    vec3 lightDirToSource = normalize(-light.direction); // Kierunek OD fragmentu DO źródła światła
    shadowFactor = CalculateShadowFactorPCF_2D(FragPosDirLightSpace, dirShadowMap, shadowMapTexelSize, normal, lightDirToSource);

    vec3 ambient = light.ambient * material.ambient;
    // vec3 lightDir = normalize(-light.direction); // To jest kierunek, w którym światło świeci
    float diff = max(dot(normal, lightDirToSource), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    vec3 reflectDir = reflect(light.direction, normal); // reflect oczekuje kierunku OD źródła DO fragmentu
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);
    return (ambient + (1.0 - shadowFactor) * (diffuse + specular));
}

// Obliczanie wkładu reflektora (bez zmian, używa CalculateShadowFactorPCF_2D)
vec3 CalculateSpotLightContribution(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);

    vec3 lightToFragDir = normalize(light.position - fragPos); // Kierunek OD światła DO fragmentu
    float theta = dot(lightToFragDir, normalize(-light.direction)); // light.direction to kierunek, w którym świeci reflektor
    float intensity = 0.0;

    if (theta > light.outerCutOff) {
        intensity = smoothstep(light.outerCutOff, light.cutOff, theta);
    }
    if (intensity <= 0.0) return vec3(0.0);

    float shadowFactor = 0.0;
    if (light.castsShadow && light.shadowDataIndex >= 0 && light.shadowDataIndex < MAX_SHADOW_CASTING_SPOT_LIGHTS_FS) {
        if (spotLightShadowData[light.shadowDataIndex].enabled) {
            // Dla reflektora, kierunek światła to lightToFragDir
             shadowFactor = CalculateShadowFactorPCF_2D(
                FragPosSpotLightSpace[light.shadowDataIndex],
                spotLightShadowData[light.shadowDataIndex].shadowMap,
                spotLightShadowData[light.shadowDataIndex].texelSize,
                normal,
                lightToFragDir // Kierunek OD fragmentu DO źródła światła
            );
        }
    }

    vec3 ambient = light.ambient * material.ambient;
    float diff = max(dot(normal, lightToFragDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    vec3 reflectDir = reflect(-lightToFragDir, normal); // reflect oczekuje kierunku OD źródła DO fragmentu
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;

    return (ambient + (1.0 - shadowFactor) * (diffuse + specular));
}


// ZMODYFIKOWANA FUNKCJA: Obliczanie wkładu światła punktowego z uwzględnieniem cieni
vec3 CalculatePointLightContribution(PointLight light, int lightIndex, vec3 normal, vec3 fragPos, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);

    vec3 lightToFragDir = normalize(light.position - fragPos); // Kierunek OD światła DO fragmentu

    float shadowFactor = 0.0;
    if (light.castsShadow && light.shadowDataIndex >= 0 && light.shadowDataIndex < MAX_SHADOW_CASTING_POINT_LIGHTS_FS) {
        // Sprawdź, czy dane cienia dla tego slotu są aktywne
        if (pointLightShadowData[light.shadowDataIndex].enabled) {
            shadowFactor = CalculatePointShadowFactor(fragPos, light, pointLightShadowData[light.shadowDataIndex]);
        }
    }

    vec3 ambient = light.ambient * material.ambient;
    float diff = max(dot(normal, lightToFragDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    vec3 reflectDir = reflect(-lightToFragDir, normal); // reflect oczekuje kierunku OD źródła DO fragmentu
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return (ambient + (1.0 - shadowFactor) * (diffuse + specular));
}

void main() {
    // Teksturowanie (jeśli używasz)
    // vec3 albedo = material.useDiffuseTexture ? texture(material.diffuseTexture, TexCoords).rgb : material.diffuse;
    // float specStrength = material.useSpecularTexture ? texture(material.specularTexture, TexCoords).r : 1.0;
    // vec3 matAmbient = material.ambient * albedo; // Moduluj ambient przez teksturę diffuse
    // vec3 matDiffuse = albedo;
    // vec3 matSpecular = material.specular * specStrength;


    if (u_UseFlatShading) {
        // Dla trybu "unlit", użyj koloru wierzchołka lub podstawowego koloru materiału diffuse
        // FragColor = VertexColor_FS; // Jeśli VertexColor_FS przechowuje żądany kolor "unlit"
        // Lub po prostu kolor materiału diffuse bez oświetlenia:
        FragColor = vec4(material.diffuse, 1.0);
         // Jeśli VertexColor_FS.a jest używane do rozróżnienia, można przywrócić:
        // FragColor = (VertexColor_FS.a < 0.01) ? vec4(material.diffuse, 1.0) : VertexColor_FS;

    } else {
        vec3 norm_world = normalize(Normal_World);
        vec3 viewDir_world = normalize(viewPos_World - FragPos_World); // Kierunek OD fragmentu DO kamery
        vec3 resultColor = vec3(0.0);

        // Obliczanie wkładu od każdego typu światła
        resultColor += CalculateDirLightContribution(dirLight, norm_world, viewDir_world);

        for (int i = 0; i < numSpotLights; i++) {
            if (i < MAX_SPOT_LIGHTS_TOTAL_FS) { // Bezpieczeństwo
                resultColor += CalculateSpotLightContribution(spotLights[i], norm_world, FragPos_World, viewDir_world);
            }
        }

        for (int i = 0; i < numPointLights; i++) {
            if (i < MAX_POINT_LIGHTS_FS) { // Bezpieczeństwo
                 resultColor += CalculatePointLightContribution(pointLights[i], i, norm_world, FragPos_World, viewDir_world);
            }
        }
        FragColor = vec4(resultColor, 1.0);
        // Można dodać globalne światło otoczenia, jeśli nie jest częścią dirLight.ambient
        // FragColor.rgb += globalAmbient * material.ambient;
    }
}


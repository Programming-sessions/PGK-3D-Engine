#version 330 core

out vec4 FragColor;

// Wejścia z vertex shadera
in vec3 FragPos_World;
in vec3 Normal_World;
in vec2 TexCoords;
in vec4 VertexColor_FS;   // Kolor wierzchołka z VS
in vec4 FragPosLightSpace; // Pozycja fragmentu w przestrzeni światła

// --- Stałe ---
const int MAX_POINT_LIGHTS = 4;
const int MAX_SPOT_LIGHTS = 4;

// --- Struktury (takie same jak wcześniej) ---
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

struct DirectionalLight {
    vec3 direction; // Znormalizowany kierunek OD źródła światła
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
    vec3 direction; // Znormalizowany kierunek OD źródła światła
    float cutOff;       // cosinus kąta wewnętrznego
    float outerCutOff;  // cosinus kąta zewnętrznego
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
uniform vec3 viewPos_World;      // Pozycja kamery w przestrzeni świata
uniform bool u_UseFlatShading;

uniform DirectionalLight dirLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];

uniform int numPointLights;
uniform int numSpotLights;

// Uniformy do obsługi cieni
uniform sampler2D shadowMap;         // Tekstura mapy głębi
// lightSpaceMatrix jest już używany w VS, ale może być potrzebny tutaj, jeśli nie jest przekazywany FragPosLightSpace
// uniform mat4 lightSpaceMatrix; // Jeśli nie używasz FragPosLightSpace bezpośrednio

// --- Funkcja obliczająca współczynnik cienia ---
// normalWorld: normalna powierzchni w przestrzeni świata
// lightDirWorld: znormalizowany kierunek DO źródła światła w przestrzeni świata
float CalculateShadowFactor(vec4 fragPosLightSpace, vec3 normalWorld, vec3 lightDirWorld) {
    // Wykonaj dzielenie perspektywiczne
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Przekształć współrzędne do zakresu [0,1] (dla próbkowania tekstury)
    projCoords = projCoords * 0.5 + 0.5;

    // Jeśli współrzędne są poza zakresem [0,1], fragment nie jest w zasięgu mapy cieni
    // (np. jest poza frustum światła). W takim przypadku nie powinien być w cieniu.
    // Jednakże, CLAMP_TO_BORDER na teksturze mapy cieni powinien obsłużyć to,
    // zwracając głębokość 1.0, co sprawi, że fragment będzie traktowany jako nieocieniony,
    // jeśli currentDepth < 1.0.
    // Można dodać jawne sprawdzenie, jeśli CLAMP_TO_BORDER nie działa zgodnie z oczekiwaniami.
    // if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
    //     return 0.0; // Nie w cieniu

    // Pobierz najbliższą głębokość z mapy cieni (z perspektywy światła)
    float closestDepthFromShadowMap = texture(shadowMap, projCoords.xy).r;

    // Pobierz aktualną głębokość fragmentu (z perspektywy światła)
    float currentDepth = projCoords.z;
    
    // Bias, aby uniknąć artefaktów "shadow acne"
    // Dynamiczny bias zależny od kąta między normalną a kierunkiem światła
    // Im bardziej powierzchnia jest równoległa do światła, tym większy bias.
    float bias = max(0.005 * (1.0 - dot(normalWorld, lightDirWorld)), 0.0005);
    // Alternatywnie, prosty stały bias:
    // float bias = 0.002;

    // Sprawdź, czy fragment jest w cieniu
    // Jeśli aktualna głębokość jest większa niż najbliższa zapisana głębokość (plus bias),
    // to fragment jest ocieniony.
    float shadow = 0.0; // 0.0 = brak cienia, 1.0 = w pełnym cieniu
    if (currentDepth - bias > closestDepthFromShadowMap) {
        shadow = 1.0;
    }
    
    // Zabezpieczenie przed rzucaniem cienia przez fragmenty poza daleką płaszczyzną frustum światła
    // Jeśli currentDepth > 1.0, oznacza to, że fragment jest dalej niż far plane światła.
    // W takim przypadku nie powinien być uznawany za ocieniony przez mapę (bo jest poza jej zasięgiem).
    if(projCoords.z > 1.0){
        shadow = 0.0;
    }

    return shadow;
}


// --- Funkcje pomocnicze do obliczania światła (takie same jak wcześniej) ---
vec3 CalculateDirLight(DirectionalLight light, vec3 normal, vec3 viewDir, float shadowFactor) {
    if (!light.enabled) return vec3(0.0);

    vec3 ambient = light.ambient * material.ambient;
    
    vec3 lightDir = normalize(-light.direction); // Kierunek OD fragmentu DO światła
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);
    
    // Zastosuj współczynnik cienia tylko do komponentów diffuse i specular
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
    return (ambient + diffuse + specular); // Cienie od świateł punktowych nie są tutaj zaimplementowane
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
    return (ambient + diffuse + specular); // Cienie od reflektorów nie są tutaj zaimplementowane
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
        if (dirLight.enabled) { // Obliczaj cień tylko jeśli światło kierunkowe jest włączone
            // Kierunek DO światła dla funkcji CalculateShadowFactor
            vec3 lightDirToSource = normalize(-dirLight.direction); 
            shadowFactor = CalculateShadowFactor(FragPosLightSpace, norm_world_normalized, lightDirToSource);
        }

        vec3 resultColor = vec3(0.0);

        // 1. Światło kierunkowe (z uwzględnieniem cienia)
        resultColor += CalculateDirLight(dirLight, norm_world_normalized, viewDir_world_normalized, shadowFactor);
        
        // 2. Światła punktowe (bez cieni w tej implementacji)
        for (int i = 0; i < numPointLights; i++) {
            if(i < MAX_POINT_LIGHTS) {
                 resultColor += CalculatePointLight(pointLights[i], norm_world_normalized, FragPos_World, viewDir_world_normalized);
            }
        }
            
        // 3. Reflektory (bez cieni w tej implementacji)
        for (int i = 0; i < numSpotLights; i++) {
             if(i < MAX_SPOT_LIGHTS) {
                resultColor += CalculateSpotLight(spotLights[i], norm_world_normalized, FragPos_World, viewDir_world_normalized);
            }
        }
        
        FragColor = vec4(resultColor, 1.0);
        // TODO: Można dodać obsługę tekstur, np. mnożąc FragColor.rgb przez texture(textureSampler, TexCoords).rgb
        // Jeśli masz sampler2D dla tekstury obiektu, np. uniform sampler2D diffuseTexture;
        // FragColor.rgb *= texture(diffuseTexture, TexCoords).rgb;
    }
}


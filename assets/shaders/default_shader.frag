#version 330 core

out vec4 FragColor; // Wyjściowy kolor fragmentu

// --- Dane wejściowe z Vertex Shadera (interpolowane) ---
in vec3 FragPos_World;        // Pozycja fragmentu w przestrzeni świata
in vec3 Normal_World;         // Wektor normalny fragmentu w przestrzeni świata (znormalizowany przez VS)
in vec2 TexCoords;            // Współrzędne tekstury
in vec4 VertexColor_FS;       // Kolor wierzchołka (może być używany lub nie)
in vec4 FragPosDirLightSpace; // Pozycja fragmentu w przestrzeni światła kierunkowego

// Pozycja fragmentu w przestrzeniach świateł reflektorów rzucających cień
// Rozmiar tablicy musi być zsynchronizowany z VS i C++
const int MAX_SHADOW_CASTING_SPOT_LIGHTS_FS_CONST = 2; // Używamy stałej dla rozmiaru tablicy
in vec4 FragPosSpotLightSpace[MAX_SHADOW_CASTING_SPOT_LIGHTS_FS_CONST];


// --- Stałe definiujące maksymalne liczby świateł (muszą być zsynchronizowane z C++) ---
const int MAX_POINT_LIGHTS_FS = 4;              // Maksymalna liczba świateł punktowych
const int MAX_SPOT_LIGHTS_TOTAL_FS = 4;         // Maksymalna łączna liczba reflektorów (aktywnych i nieaktywnych w tablicy)
const int MAX_SHADOW_CASTING_SPOT_LIGHTS_FS = 2;// Maksymalna liczba reflektorów rzucających cień
const int MAX_SHADOW_CASTING_POINT_LIGHTS_FS = 1;// Maksymalna liczba świateł punktowych rzucających cień

// --- Struktura Materiału ---
struct Material {
    vec3 ambient;             // Kolor światła otoczenia odbijanego przez materiał (jeśli brak tekstury)
    vec3 diffuse;             // Kolor światła rozproszonego odbijanego przez materiał (jeśli brak tekstury)
    vec3 specular;            // Kolor światła lustrzanego odbijanego przez materiał (jeśli brak tekstury)
    float shininess;          // Współczynnik połysku materiału (wpływa na rozmiar i intensywność odbicia lustrzanego)

    sampler2D diffuseTexture;   // Tekstura diffuse (kolor bazowy obiektu)
    sampler2D specularTexture;  // Tekstura specular (mapa intensywności lub koloru odbić lustrzanych)

    bool useDiffuseTexture;     // Flaga: czy używać tekstury diffuse
    bool useSpecularTexture;    // Flaga: czy używać tekstury specular
};

// --- Struktury Świateł ---
struct DirectionalLight {
    vec3 direction;   // Kierunek padania światła (od źródła)
    vec3 ambient;     // Składowa ambient światła
    vec3 diffuse;     // Składowa diffuse światła
    vec3 specular;    // Składowa specular światła
    bool enabled;     // Czy światło jest włączone
};

// Dane potrzebne do obliczania cieni dla reflektora
struct SpotLightShadowData {
    sampler2D shadowMap; // Tekstura mapy cieni (2D)
    bool enabled;        // Czy cienie dla tego slotu są włączone
    float texelSize;     // Rozmiar teksela mapy cieni (1.0 / szerokosc_mapy_cieni)
};

// Reflektor (Spot Light)
struct SpotLight {
    vec3 position;    // Pozycja źródła światła
    vec3 direction;   // Kierunek świecenia
    float cutOff;     // Cosinus wewnętrznego kąta stożka (pełna intensywność)
    float outerCutOff;// Cosinus zewnętrznego kąta stożka (stopniowe zanikanie)

    // Współczynniki tłumienia (attenuation)
    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    bool enabled;

    // Dane związane z cieniami
    bool castsShadow;        // Czy ten reflektor rzuca cień
    int shadowDataIndex;     // Indeks do tablicy spotLightShadowData (jeśli castsShadow = true)
                             // Wartość -1 może oznaczać brak przypisanej mapy cieni.
};

// Dane potrzebne do obliczania cieni dla światła punktowego
struct PointLightShadowData {
    samplerCube shadowCubeMap; // Tekstura mapy cieni (Cube Map)
    float farPlane;            // Daleka płaszczyzna przycinania użyta do generowania mapy cieni
    bool enabled;              // Czy cienie dla tego slotu są włączone
};

// Światło punktowe (Point Light)
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
    int shadowDataIndex; // Indeks do tablicy pointLightShadowData
};

// --- Uniformy ---
uniform Material material;         // Materiał aktualnie renderowanego obiektu
uniform vec3 viewPos_World;        // Pozycja kamery (obserwatora) w przestrzeni świata
uniform bool u_UseFlatShading;     // Flaga: czy używać płaskiego cieniowania (tylko kolor diffuse, bez oświetlenia)

// Światło kierunkowe i jego cień
uniform DirectionalLight dirLight;
uniform sampler2D dirShadowMap;        // Mapa cieni dla światła kierunkowego
uniform float shadowMapTexelSize;  // Rozmiar teksela dla dirShadowMap (1.0 / szerokosc_mapy)
uniform int u_pcfRadius;           // Promień dla PCF (Percentage Closer Filtering)

// Reflektory i ich dane cieni
uniform SpotLight spotLights[MAX_SPOT_LIGHTS_TOTAL_FS]; // Tablica wszystkich reflektorów
uniform int numSpotLights;                              // Liczba aktywnych reflektorów w tablicy spotLights
uniform SpotLightShadowData spotLightShadowData[MAX_SHADOW_CASTING_SPOT_LIGHTS_FS]; // Dane map cieni dla reflektorów

// Światła punktowe i ich dane cieni
uniform PointLight pointLights[MAX_POINT_LIGHTS_FS];    // Tablica wszystkich świateł punktowych
uniform int numPointLights;                             // Liczba aktywnych świateł punktowych
uniform PointLightShadowData pointLightShadowData[MAX_SHADOW_CASTING_POINT_LIGHTS_FS]; // Dane map cieni dla świateł punktowych

// --- Funkcje Pomocnicze ---

/**
 * Oblicza współczynnik cienia dla map 2D (światło kierunkowe, reflektory)
 * używając Percentage Closer Filtering (PCF) dla zmiękczenia krawędzi cieni.
 * @param fragPosLightSpace Pozycja fragmentu w przestrzeni światła.
 * @param specificShadowMap Tekstura mapy cieni do próbkowania.
 * @param specificTexelSize Rozmiar teksela danej mapy cieni.
 * @param normalWorld Normalna fragmentu w przestrzeni świata.
 * @param lightDirWorld Kierunek od fragmentu do źródła światła w przestrzeni świata.
 * @return Współczynnik cienia (0.0 - w cieniu, 1.0 - w pełni oświetlony).
 * W shaderze jest odwrotnie: 0.0 - brak cienia, 1.0 - pełny cień. Zostawiam jak w oryginale.
 */
float CalculateShadowFactorPCF_2D(vec4 fragPosLightSpace, sampler2D specificShadowMap, float specificTexelSize, vec3 normalWorld, vec3 lightDirWorld) {
    // Transformacja współrzędnych do przestrzeni tekstury mapy cieni [0,1] i normalizacja przez w
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5; // Przekształcenie z [-1,1] do [0,1]

    float currentDepth = projCoords.z; // Głębokość bieżącego fragmentu z perspektywy światła

    // Sprawdzenie, czy fragment znajduje się poza obszarem mapy cieni lub za daleką płaszczyzną
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0; // Poza mapą cieni, brak cienia (lub pełne światło, zależy od interpretacji)
    }

    float shadow = 0.0; // Akumulator współczynnika cienia
    // Bias zapobiegający "shadow acne" (artefakty samocieniowania)
    // Dynamiczny bias zależny od kąta padania światła
    float bias = max(0.005 * (1.0 - dot(normalWorld, lightDirWorld)), 0.0005);
    float totalSamples = 0.0;

    // Pętla PCF - próbkowanie wokół centralnego punktu w mapie cieni
    for (int x = -u_pcfRadius; x <= u_pcfRadius; ++x) {
        for (int y = -u_pcfRadius; y <= u_pcfRadius; ++y) {
            vec2 offset = vec2(x, y) * specificTexelSize; // Przesunięcie próbki
            // Głębokość zapisana w mapie cieni dla przesuniętej próbki
            float pcfDepth = texture(specificShadowMap, projCoords.xy + offset).r;
            // Porównanie głębokości bieżącego fragmentu z głębokością z mapy cieni
            if (currentDepth - bias > pcfDepth) {
                shadow += 1.0; // Fragment jest w cieniu tej próbki
            }
            totalSamples += 1.0;
        }
    }
    if (totalSamples > 0.0) shadow /= totalSamples; // Uśrednienie wyniku
    return shadow; // 0.0 = brak cienia, 1.0 = pełny cień (w tej implementacji)
}

/**
 * Oblicza współczynnik cienia dla światła punktowego (używając CubeMap).
 * @param fragPos_World Pozycja fragmentu w przestrzeni świata.
 * @param currentPointLight Aktualnie przetwarzane światło punktowe.
 * @param shadowData Dane mapy cieni dla tego światła.
 * @return Współczynnik cienia (0.0 - brak cienia, 1.0 - pełny cień).
 */
float CalculatePointShadowFactor(vec3 fragPos_World, PointLight currentPointLight, PointLightShadowData shadowData) {
    // Wektor od źródła światła do fragmentu (używany do próbkowania CubeMap)
    vec3 fragToLightVector = fragPos_World - currentPointLight.position;
    // Aktualna głębokość fragmentu (odległość liniowa od źródła światła)
    float currentDepthLinear = length(fragToLightVector);

    // Odczyt najbliższej zapisanej głębokości z CubeMap
    // CubeMap przechowuje wartości znormalizowane [0,1], więc trzeba je przeskalować
    float closestDepthNormalized = texture(shadowData.shadowCubeMap, fragToLightVector).r;
    float closestDepthLinear = closestDepthNormalized * shadowData.farPlane; // Przeskalowanie do odległości liniowej

    float shadow = 0.0;
    float bias = 0.05; // Bias dla cieni punktowych (może wymagać dostosowania)
    // Porównanie głębokości
    if (currentDepthLinear - bias > closestDepthLinear) {
        shadow = 1.0; // Fragment jest w cieniu
    }
    return shadow;
}


// --- Funkcje obliczające wkład poszczególnych typów świateł ---
// Przyjmują teraz efektywne właściwości materiału jako argumenty.

vec3 CalculateDirLightContribution(DirectionalLight light, vec3 normal, vec3 viewDir,
                                   vec3 currentMaterialAmbient, vec3 currentMaterialDiffuse, vec3 currentMaterialSpecular, float currentMaterialShininess) {
    if (!light.enabled) return vec3(0.0); // Jeśli światło wyłączone, brak wkładu

    vec3 lightDirToSource = normalize(-light.direction); // Kierunek DO źródła światła

    // Obliczenie współczynnika cienia
    float shadowFactor = CalculateShadowFactorPCF_2D(FragPosDirLightSpace, dirShadowMap, shadowMapTexelSize, normal, lightDirToSource);

    // Składowa Ambient
    vec3 ambient = light.ambient * currentMaterialAmbient;

    // Składowa Diffuse (model Lamberta)
    float diffIntensity = max(dot(normal, lightDirToSource), 0.0); // Intensywność światła rozproszonego
    vec3 diffuse = light.diffuse * (diffIntensity * currentMaterialDiffuse);

    // Składowa Specular (model Phonga-Blinna lub Phonga)
    vec3 reflectDir = reflect(light.direction, normal); // Wektor odbity (light.direction to wektor OD źródła)
    float specIntensity = pow(max(dot(viewDir, reflectDir), 0.0), currentMaterialShininess); // Intensywność odbicia lustrzanego
    vec3 specular = light.specular * (specIntensity * currentMaterialSpecular);

    // Łączny wkład światła (uwzględniając cień dla diffuse i specular)
    return (ambient + (1.0 - shadowFactor) * (diffuse + specular));
}

vec3 CalculateSpotLightContribution(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir,
                                    vec3 currentMaterialAmbient, vec3 currentMaterialDiffuse, vec3 currentMaterialSpecular, float currentMaterialShininess) {
    if (!light.enabled) return vec3(0.0);

    vec3 lightToFragDir = normalize(light.position - fragPos); // Kierunek OD źródła światła DO fragmentu

    // Sprawdzenie, czy fragment jest w stożku światła reflektora
    float theta = dot(lightToFragDir, normalize(-light.direction)); // Kąt między kierunkiem światła a wektorem do fragmentu
    float intensityFactor = 0.0;
    if (theta > light.outerCutOff) { // Fragment jest potencjalnie w stożku
        // Płynne przejście między wewnętrznym a zewnętrznym stożkiem
        intensityFactor = smoothstep(light.outerCutOff, light.cutOff, theta);
    }
    if (intensityFactor <= 0.0) return vec3(0.0); // Poza stożkiem, brak wkładu

    // Obliczenie współczynnika cienia, jeśli reflektor rzuca cień
    float shadowFactor = 0.0;
    if (light.castsShadow && light.shadowDataIndex >= 0 && light.shadowDataIndex < MAX_SHADOW_CASTING_SPOT_LIGHTS_FS) {
        if (spotLightShadowData[light.shadowDataIndex].enabled) {
             shadowFactor = CalculateShadowFactorPCF_2D(
                FragPosSpotLightSpace[light.shadowDataIndex], // Użyj odpowiedniego FragPos dla tego reflektora
                spotLightShadowData[light.shadowDataIndex].shadowMap,
                spotLightShadowData[light.shadowDataIndex].texelSize,
                normal,
                lightToFragDir // Kierunek DO źródła światła
            );
        }
    }

    // Składowa Ambient
    vec3 ambient = light.ambient * currentMaterialAmbient;

    // Składowa Diffuse
    float diffIntensity = max(dot(normal, lightToFragDir), 0.0);
    vec3 diffuse = light.diffuse * (diffIntensity * currentMaterialDiffuse);

    // Składowa Specular
    vec3 reflectDir = reflect(-lightToFragDir, normal); // Odbicie wektora OD fragmentu DO źródła
    float specIntensity = pow(max(dot(viewDir, reflectDir), 0.0), currentMaterialShininess);
    vec3 specular = light.specular * (specIntensity * currentMaterialSpecular);

    // Tłumienie (attenuation) w zależności od odległości
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    // Zastosowanie tłumienia i intensywności stożka
    ambient  *= attenuation * intensityFactor;
    diffuse  *= attenuation * intensityFactor;
    specular *= attenuation * intensityFactor;

    return (ambient + (1.0 - shadowFactor) * (diffuse + specular));
}

vec3 CalculatePointLightContribution(PointLight light, int lightIndex, vec3 normal, vec3 fragPos, vec3 viewDir,
                                     vec3 currentMaterialAmbient, vec3 currentMaterialDiffuse, vec3 currentMaterialSpecular, float currentMaterialShininess) {
    if (!light.enabled) return vec3(0.0);

    vec3 lightToFragDir = normalize(light.position - fragPos); // Kierunek OD źródła światła DO fragmentu

    // Obliczenie współczynnika cienia, jeśli światło punktowe rzuca cień
    float shadowFactor = 0.0;
    if (light.castsShadow && light.shadowDataIndex >= 0 && light.shadowDataIndex < MAX_SHADOW_CASTING_POINT_LIGHTS_FS) {
        if (pointLightShadowData[light.shadowDataIndex].enabled) {
            shadowFactor = CalculatePointShadowFactor(fragPos, light, pointLightShadowData[light.shadowDataIndex]);
        }
    }

    // Składowa Ambient
    vec3 ambient = light.ambient * currentMaterialAmbient;

    // Składowa Diffuse
    float diffIntensity = max(dot(normal, lightToFragDir), 0.0);
    vec3 diffuse = light.diffuse * (diffIntensity * currentMaterialDiffuse);

    // Składowa Specular
    vec3 reflectDir = reflect(-lightToFragDir, normal);
    float specIntensity = pow(max(dot(viewDir, reflectDir), 0.0), currentMaterialShininess);
    vec3 specular = light.specular * (specIntensity * currentMaterialSpecular);

    // Tłumienie (attenuation)
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;

    return (ambient + (1.0 - shadowFactor) * (diffuse + specular));
}

// --- Główna funkcja Fragment Shadera ---
void main() {
    // --- Określenie efektywnych właściwości materiału na podstawie uniformu 'material' i tekstur ---
    vec3 effectiveAmbient;
    vec3 effectiveDiffuse;
    vec3 effectiveSpecular;
    float effectiveShininess = material.shininess; // Połysk zazwyczaj nie pochodzi bezpośrednio z tekstury

    // Kolor Ambient: modulowany przez teksturę diffuse lub bazowy kolor ambient materiału
    if (material.useDiffuseTexture) {
        // Moduluj bazowy kolor ambient materiału (z uniformu) przez kolor z tekstury diffuse
        effectiveAmbient = material.ambient * texture(material.diffuseTexture, TexCoords).rgb;
    } else {
        effectiveAmbient = material.ambient; // Użyj bazowego koloru ambient z uniformu Material
    }

    // Kolor Diffuse: z tekstury diffuse lub bazowy kolor diffuse materiału
    if (material.useDiffuseTexture) {
        effectiveDiffuse = texture(material.diffuseTexture, TexCoords).rgb;
    } else {
        effectiveDiffuse = material.diffuse; // Użyj bazowego koloru diffuse z uniformu Material
    }

    // Kolor Specular: z tekstury specular lub bazowy kolor specular materiału
    if (material.useSpecularTexture) {
        // Zakładamy, że tekstura specular dostarcza pełny kolor RGB dla odbicia
        effectiveSpecular = texture(material.specularTexture, TexCoords).rgb;
        // Alternatywnie, jeśli tekstura specular to mapa intensywności (skala szarości):
        // effectiveSpecular = material.specular * texture(material.specularTexture, TexCoords).r;
    } else {
        effectiveSpecular = material.specular; // Użyj bazowego koloru specular z uniformu Material
    }


    // --- Wybór modelu cieniowania ---
    if (u_UseFlatShading) {
        // Tryb płaskiego cieniowania (unlit): użyj tylko efektywnego koloru diffuse (np. koloru z tekstury)
        // Można też użyć VertexColor_FS, jeśli jest przekazywany i pożądany.
        FragColor = vec4(effectiveDiffuse, 1.0);
    } else {
        // Tryb pełnego oświetlenia (model Phonga-Blinna lub podobny)

        // Przygotowanie wektorów potrzebnych do obliczeń oświetlenia
        vec3 norm_world = normalize(Normal_World); // Znormalizowany wektor normalny fragmentu
        vec3 viewDir_world = normalize(viewPos_World - FragPos_World); // Znormalizowany kierunek od fragmentu do kamery

        vec3 resultColor = vec3(0.0); // Inicjalizacja wynikowego koloru

        // Obliczanie wkładu od światła kierunkowego
        resultColor += CalculateDirLightContribution(dirLight, norm_world, viewDir_world,
                                                    effectiveAmbient, effectiveDiffuse, effectiveSpecular, effectiveShininess);

        // Obliczanie wkładu od aktywnych reflektorów
        for (int i = 0; i < numSpotLights; i++) {
            // Upewniamy się, że nie wyjdziemy poza zadeklarowany rozmiar tablicy uniformów
            if (i < MAX_SPOT_LIGHTS_TOTAL_FS) {
                resultColor += CalculateSpotLightContribution(spotLights[i], norm_world, FragPos_World, viewDir_world,
                                                              effectiveAmbient, effectiveDiffuse, effectiveSpecular, effectiveShininess);
            }
        }

        // Obliczanie wkładu od aktywnych świateł punktowych
        for (int i = 0; i < numPointLights; i++) {
            // Upewniamy się, że nie wyjdziemy poza zadeklarowany rozmiar tablicy uniformów
            if (i < MAX_POINT_LIGHTS_FS) {
                 resultColor += CalculatePointLightContribution(pointLights[i], i, norm_world, FragPos_World, viewDir_world,
                                                                effectiveAmbient, effectiveDiffuse, effectiveSpecular, effectiveShininess);
            }
        }
        FragColor = vec4(resultColor, 1.0); // Ustawienie finalnego koloru fragmentu (z domyślną alfa = 1.0)
    }
}


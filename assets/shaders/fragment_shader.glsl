#version 330 core

out vec4 FragColor;

// Wejścia z vertex shadera
in vec3 FragPos_World;   // Pozycja fragmentu w przestrzeni świata
in vec3 Normal_World;    // Interpolowana normalna wierzchołka (używana tylko dla smooth shading)
in vec2 TexCoords;       // Współrzędne tekstury
in vec4 VertexColor;     // Kolor zdefiniowany dla wierzchołka (np. coneColor)

// Struktury materiału i światła (używane tylko dla smooth shading)
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
};

// Uniformy
uniform Material material;          // Używane tylko dla smooth shading
uniform DirectionalLight dirLight;  // Używane tylko dla smooth shading
uniform vec3 viewPos_World;       // Używane tylko dla smooth shading
uniform bool u_UseFlatShading;    // Flaga do przełączania trybu

void main() {
    if (u_UseFlatShading) {
        // Tryb "Flat Shading" (Unlit): Użyj bezpośrednio VertexColor
        FragColor = VertexColor;
    } else {
        // Tryb Smooth Shading (Oświetlenie Phonga)
        vec3 N_for_lighting = normalize(Normal_World);

        // --- Światło otoczenia (Ambient) ---
        vec3 ambientColor = dirLight.ambient * material.ambient;

        // --- Światło rozproszone (Diffuse) ---
        vec3 lightDir = normalize(-dirLight.direction);
        float diffFactor = max(dot(N_for_lighting, lightDir), 0.0);
        vec3 diffuseColor = dirLight.diffuse * diffFactor * material.diffuse;

        // --- Światło zwierciadlane (Specular) ---
        vec3 viewDir = normalize(viewPos_World - FragPos_World);
        vec3 reflectDir = reflect(-lightDir, N_for_lighting);
        float specFactor = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
        vec3 specularColor = dirLight.specular * specFactor * material.specular;
        
        vec3 finalColor = ambientColor + diffuseColor + specularColor;
        FragColor = vec4(finalColor, 1.0); // Alpha można wziąć z VertexColor.a lub material.alpha, jeśli dodasz
    }
}

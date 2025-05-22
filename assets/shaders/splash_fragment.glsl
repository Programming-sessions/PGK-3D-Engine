#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D splashTextureSampler;
uniform float alphaValue; // Dla efektu zanikania

void main() {
    vec4 texColor = texture(splashTextureSampler, TexCoords);
    // Można dodać obsługę przezroczystości tekstury, jeśli ją ma
    // if(texColor.a < 0.1) discard; 
    FragColor = vec4(texColor.rgb, texColor.a * alphaValue);
}
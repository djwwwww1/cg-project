#version 330 core
in vec3 vPos;
in vec3 vNrm;

uniform vec3 uColor;

uniform vec3 uViewPos;
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uLightDir;
uniform float uInnerCut;
uniform float uOuterCut;

uniform float uEnvAmbient;

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNrm);
    vec3 L = normalize(uLightPos - vPos);

    float diff = max(dot(N, L), 0.0);

    float theta = dot(normalize(-uLightDir), L);
    float eps = max(uInnerCut - uOuterCut, 1e-4);
    float spot = clamp((theta - uOuterCut) / eps, 0.0, 1.0);

    float dist = length(uLightPos - vPos);
    float atten = 1.0 / (1.0 + 0.10 * dist + 0.018 * dist * dist);

    vec3 ambient = uEnvAmbient * uColor;
    vec3 lit = (ambient + spot * atten * diff * uLightColor) * uColor;

    FragColor = vec4(lit, 1.0);
}
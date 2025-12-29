#version 330 core
out vec4 FragColor;

in vec3 vWorldPos;

uniform int uMode = 1; // 0 ball, 1 base, 5 mask, 6 composite

uniform vec3 uColor  = vec3(0.65, 0.50, 0.32);   // wall/floor base
uniform vec4 uColor4 = vec4(0.10, 0.07, 0.05, 1); // ball/shadow color

uniform int  uUseLighting = 1;
uniform vec2  uLightCenter = vec2(0.0);
uniform float uLightRadius = 10.0;
uniform float uSoftness    = 2.0;
uniform float uAmbient     = 0.45;

uniform vec2 uResolution = vec2(1280, 720);
uniform sampler2D uShadowMask;

float safeLightFactor(vec2 wallXY) {
    if (uLightRadius <= 1e-4 || uSoftness <= 1e-4) return 0.0;
    float d = distance(wallXY, uLightCenter);
    float edge0 = uLightRadius;
    float edge1 = max(uLightRadius - uSoftness, 0.0);
    return smoothstep(edge0, edge1, d); // inside=1 outside=0
}

void main() {
    vec2 wallXY = vWorldPos.xy;
    float lf = safeLightFactor(wallXY);

    if (uMode == 0) { // ball
        FragColor = uColor4;
        return;
    }

    if (uMode == 5) { // mask gen (R8)
        float mask = 0.90 * lf;
        FragColor = vec4(mask, 0.0, 0.0, 1.0);
        return;
    }

    if (uMode == 6) { // composite from mask
        vec2 uv = gl_FragCoord.xy / uResolution;
        float m = texture(uShadowMask, uv).r;
        FragColor = vec4(uColor4.rgb, uColor4.a * m);
        return;
    }

    // uMode == 1 base
    vec3 base = uColor;
    if (uUseLighting == 1) {
        float k = mix(uAmbient, 1.0, lf);
        base *= k;
    }
    FragColor = vec4(base, 1.0);
}

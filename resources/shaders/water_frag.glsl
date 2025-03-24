varying vec4 v_color;
varying vec2 v_texCoord;
varying vec3 v_worldPos;
uniform sampler2D tex0;
uniform vec2 u_playerPos;
uniform float u_foamRadiusWorld;
uniform float u_time;

uniform float u_timeScale;
uniform float u_initialAmplitude;
uniform float u_amplitudeGain;
uniform float u_rotationAngle;
uniform float u_colorMixFactor;

#define OCTAVES 5
#define NOISE_SCALE 3.0
#define X_MOVEMENT_SCALE1 0.1
#define X_MOVEMENT_SCALE2 0.13
#define LAYER12_MIX 0.5

#define WATER_COLOR_DARK    vec3(0.101961, 0.619608, 0.266667)
#define WATER_COLOR_MID     vec3(0.666667, 0.666667, 0.998039)
#define WATER_COLOR_SHALLOW vec3(0.0, 0.4, 0.964706)
#define WATER_COLOR_FOAM    vec3(0.666667, 1.0, 1.0)

float hashS(in vec2 pos)
{
    return fract(sin(dot(pos.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float valueNoise(in vec2 pos)
{
    vec2 grid = floor(pos);
    vec2 gPos = fract(pos);

    float a = hashS(grid);
    float b = hashS(grid + vec2(1.0, 0.0));
    float c = hashS(grid + vec2(0.0, 1.0));
    float d = hashS(grid + vec2(1.0, 1.0));

    // factorized cubic hermite spline
    vec2 intp = gPos * gPos * (3.0 - 2.0 * gPos);

    return mix(a, b, u.x) + (c - a) * intp.y * (1.0 - intp.x) + (d - b) * intp.x * intp.y;
}

float fbm(in vec2 pos)
{
    float value = 0.0;
    float initAmplitude = u_initialAmplitude;
    float amplitudeGain = u_amplitudeGain;
    vec2 octaveOffset = vec2(100.0);

    mat2 rotation = mat2(cos(u_rotationAngle), sin(u_rotationAngle), - sin(u_rotationAngle), cos(u_rotationAngle));

    for (int i = 0; i < OCTAVES; ++i)
    {
        value += initAmplitude * valueNoise(pos + octaveOffset);
        pos = rotation * pos * 2.0;
        octaveOffset += vec2(50.0, 50.0);
        initAmplitude *= amplitudeGain;
    }

    return value;
}

vec4 domainWarpedFBM(vec2 screenPos)
{
    screenPos *= NOISE_SCALE;

    vec2 layer1;
    layer1.x = fbm(screenPos + X_MOVEMENT_SCALE1 * u_time * u_timeScale);
    layer1.y = fbm(screenPos + vec2(2.0));

    vec2 layer2;
    layer2.x = fbm(screenPos + 1.0 * layer1 + vec2(1.7, 9.2) + 0.15 * u_time * u_timeScale);
    layer2.y = fbm(screenPos + 1.0 * layer1 + vec2(8.3, 2.8) + 0.126 * u_time * u_timeScale);

    float l1Height = fbm(screenPos + layer2);

    vec2 layer3;
    layer3.x = fbm(screenPos * 2.0 - X_MOVEMENT_SCALE2 * u_time * u_timeScale + vec2(10.0, 20.0));
    layer3.y = fbm(screenPos * 2.0 + vec2(3.0) + vec2(10.0, 20.0));

    vec2 layer4;
    layer4.x = fbm(screenPos * 2.3 + 1.0 * layer3 + vec2(1.7, 4.2) + 0.3 * u_time * u_timeScale);
    layer4.y = fbm(screenPos * 3.0 + 1.0 * layer3 + vec2(2.3, 2.8) + 0.1 * u_time * u_timeScale);

    float l2Height = fbm(screenPos * 2.0 + layer4);
    
    float f = mix(l1Height, l2Height, LAYER12_MIX);

    vec3 fmbColor = mix(WATER_COLOR_DARK,
        WATER_COLOR_MID, clamp((f * f) * u_colorMixFactor, 0.0, 1.0));

    fmbColor = mix(fmbColor,
        WATER_COLOR_SHALLOW, clamp(length(layer1), 0.0, 1.0));

    fmbColor = mix(fmbColor,
        WATER_COLOR_FOAM, clamp(length(layer2.x), 0.0, 1.0));

    return vec4((f * f * f + .6 * f * f + .5 * f) * fmbColor, 1.0);
}

void main()
{
    vec2 u_resolution = vec2(1280.0, 720.0);
    vec2 screenPos = gl_FragCoord.xy / u_resolution.xy;

    vec4 fin = domainWarpedFBM(screenPos);

    vec4 foamColor = vec4(0.7, 0.7, 1.0, 1.0);
    foamColor = vec4(0.1, 0.1, 0.1, 1.0);
    float aspectRatio = 1280.0 / 720.0;
    vec2 screenPos2 = vec2(screenPos.x, 1.0 - screenPos.y);
    screenPos2.x *= aspectRatio;

    vec2 playerPosAdjusted = u_playerPos;
    playerPosAdjusted.x *= aspectRatio;

    float distanceToPlayer = distance(screenPos2.xy, playerPosAdjusted);
    float foamFactor = 1.0 - smoothstep(0.0, 0.14, distanceToPlayer);

    vec4 finalColor = mix(fin, foamColor, foamFactor);
    gl_FragColor = finalColor;
}
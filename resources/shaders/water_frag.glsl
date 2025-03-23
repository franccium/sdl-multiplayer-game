varying vec4 v_color;
varying vec2 v_texCoord;
varying vec2 v_worldPos;
uniform sampler2D tex0;
uniform vec2 u_playerPos;
uniform float u_foamRadiusWorld;

void main()
{
    float distanceToPlayer = distance(v_worldPos, u_playerPos);
    float foamFactor = 1.0 - smoothstep(u_foamRadiusWorld - 0.1, u_foamRadiusWorld, distanceToPlayer);
    vec4 baseColor = texture2D(tex0, v_texCoord) * v_color;
    vec4 foamColor = vec4(1.0, 1.0, 1.0, 1.0); // White foam
    gl_FragColor = mix(baseColor, foamColor, foamFactor);
}

/*varying vec4 v_color;
varying vec2 v_texCoord;
uniform sampler2D tex0;

void main()
{
    //vec4 color = vec4(1.0, 1.0, 0.1, 1.0);
    vec4 color = texture2D(tex0, v_texCoord) * v_color;
    gl_FragColor = color;
}*/
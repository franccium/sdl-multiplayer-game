varying vec4 v_color;
varying vec2 v_texCoord;
varying vec2 v_worldPos;

uniform float u_time;
uniform vec2 u_playerPos;
uniform vec2 u_aspectRatio;
uniform float u_foamRadiusWorld;

void main()
{
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    v_color = gl_Color;

    v_color.b += sin(u_time * 0.01);
    vec2 scrollDirection = vec2(1.0, 0.5);
    float scrollSpeed = 0.02;
    v_texCoord = vec2(gl_MultiTexCoord0)
    
    /*
    //v_texCoord = vec2(gl_MultiTexCoord0) + scrollDirection * u_time * scrollSpeed;
    v_texCoord = vec2(gl_MultiTexCoord0);;

    //vec2 worldPos = vec2(v_texCoord.x * 1280.0, v_texCoord.y * 720.0);
    vec2 delta = (v_texCoord - u_playerPos) * u_aspectRatio;
    float playerDistance = length(delta);

    //float playerDistance = length(v_texCoord - u_playerPos);
    float foamIntensity = 1.0 - smoothstep(0.0, u_foamRadiusWorld, playerDistance);
    vec4 foamColor = vec4(1.0, 0.0, 0.0, 1.0);
    vec4 finalColor = mix(v_color, foamColor, foamIntensity);
    v_color = finalColor;*/

    v_worldPos = gl_Vertex.xy;
}

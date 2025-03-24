varying vec4 v_color;
varying vec2 v_texCoord;

uniform mat4 u_modelMatrix;
varying vec3 v_worldPos;

uniform float u_time;
uniform vec2 u_playerPos;
uniform vec2 u_aspectRatio;
uniform float u_foamRadiusWorld;
varying vec2 v_playerPos;

void main()
{
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    v_color = gl_Color;

    v_color.b += sin(u_time * 0.01);
    vec2 scrollDirection = vec2(1.0, 0.5);
    float scrollSpeed = 0.02;

    //v_texCoord = vec2(gl_MultiTexCoord0);
    v_texCoord = vec2(gl_MultiTexCoord0) + scrollDirection * u_time * scrollSpeed;

    v_playerPos = u_playerPos;
}

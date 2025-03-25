varying vec4 v_color;
varying vec2 v_texCoord;
uniform float u_time;

void main()
{
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    vec3 color = mix(gl_Color.rgb, vec3(0.4, 0.4, 1.0), sin(u_time * 1.5));

    v_color = vec4(color, 1.0);
    v_texCoord = vec2(gl_MultiTexCoord0);
}

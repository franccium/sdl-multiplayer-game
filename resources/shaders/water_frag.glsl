varying vec4 v_color;
varying vec2 v_texCoord;
uniform sampler2D tex0;

void main()
{
    vec4 color = vec4(1.0, 1.0, 0.1, 1.0);
    gl_FragColor = texture2D(tex0, v_texCoord) * v_color;
}

varying vec4 v_color;
varying vec2 v_texCoord;
uniform sampler2D tex0;

void main()
{
    //vec4 debugColor = vec4(0.2, 0.0, 0.1, 1.0);
    vec4 finalColor = texture2D(tex0, v_texCoord);
    gl_FragColor = finalColor;
}

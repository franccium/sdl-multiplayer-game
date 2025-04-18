varying vec4 v_color;
varying vec2 v_texCoord;
uniform sampler2D tex0;
uniform float u_hpPercentage;

void main()
{
    //vec4 debugColor = vec4(0.2, 0.0, 0.1, 1.0);
    float fade = 0.15;
    float hpMask = smoothstep(u_hpPercentage + fade, u_hpPercentage - fade, v_texCoord.x);

    vec4 hpBackColor = vec4(0.1, 0.1, 0.1, 0.6);
    vec4 greenColor = vec4(0.3, 0.9, 0.3, 0.8);
    vec4 redColor = vec4(1.0, 0.1, 0.1, 0.8);
    vec4 hpBaseColor = mix(redColor, greenColor, u_hpPercentage);
    vec4 textureColor = texture2D(tex0, v_texCoord);
    vec4 hpColor = textureColor * hpBaseColor;
    vec4 finalColor = mix(hpBackColor * textureColor, hpColor, hpMask);

    vec4 texcoords = vec4(v_texCoord, 0.0, 1.0);
    gl_FragColor = finalColor;
}

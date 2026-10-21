#version 330

// One direction of a separable gaussian blur, nine taps.
uniform sampler2D sourceTex;
uniform vec2 direction; // a texel along the blur

in vec2 texCoord;
out vec4 fragColor;

void main()
{
    const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 result = texture(sourceTex, texCoord).rgb * weights[0];
    for (int i = 1; i < 5; ++i)
    {
        result += texture(sourceTex, texCoord + direction * float(i)).rgb * weights[i];
        result += texture(sourceTex, texCoord - direction * float(i)).rgb * weights[i];
    }
    fragColor = vec4(result, 1.0);
}

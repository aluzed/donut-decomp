#version 330

uniform sampler2D sceneTex;
uniform vec2 screenSize;

uniform float bloomIntensity = 0.3;
uniform float vignetteIntensity = 0.5;
uniform float contrast = 1.2;
uniform float saturation = 1.1;

in vec2 texCoord;
out vec4 fragColor;

vec3 applyColorGrading(vec3 color)
{
    vec3 gray = vec3(dot(color, vec3(0.299, 0.587, 0.114)));
    color = mix(gray, color, saturation);
    color = (color - 0.5) * contrast + 0.5;
    return clamp(color, 0.0, 1.0);
}

vec3 blurSample(sampler2D tex, vec2 uv, vec2 dir)
{
    vec3 result = vec3(0.0);
    float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec2 step = (1.0 / screenSize) * dir;
    for (int i = -4; i <= 4; i++)
    {
        float w = 0.0;
        int idx = abs(i);
        if (idx < 5) w = weights[idx];
        result += texture(tex, uv + float(i) * step).rgb * w;
    }
    return result;
}

void main()
{
    vec3 color = texture(sceneTex, texCoord).rgb;

    vec3 bright = max(color - vec3(0.7), 0.0);
    vec3 bloomH = blurSample(sceneTex, texCoord, vec2(3.0, 0.0));
    vec3 bloomV = blurSample(sceneTex, texCoord, vec2(0.0, 3.0));
    vec3 bloom = mix(bloomH, bloomV, 0.5) * bloomIntensity * 0.5;
    bloom *= bright;

    float vignette = 1.0 - vignetteIntensity * smoothstep(0.4, 1.4,
        length((texCoord - 0.5) * vec2(1.0, screenSize.x / screenSize.y)));

    color += bloom;
    color = applyColorGrading(color);
    color *= vignette;

    fragColor = vec4(color, 1.0);
}

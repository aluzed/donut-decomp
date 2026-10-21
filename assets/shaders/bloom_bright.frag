#version 330

// The bloom's first pass: what of the scene is brighter than the threshold.
uniform sampler2D sceneTex;
uniform float threshold;

in vec2 texCoord;
out vec4 fragColor;

void main()
{
    vec3 color = texture(sceneTex, texCoord).rgb;
    fragColor = vec4(max(color - vec3(threshold), 0.0) / max(1.0 - threshold, 0.001), 1.0);
}

#version 330

uniform vec4 meshColor;
uniform vec3 lightDir = normalize(vec3(-0.4, 0.5, -0.6));

in vec3 fragNormal;
in vec3 fragPos;

out vec4 fragColor;

void main()
{
    vec3 n = normalize(fragNormal);
    float NdotL = clamp(dot(n, lightDir), 0.0, 1.0);
    vec3 lit = meshColor.rgb * (NdotL * 0.7 + 0.3);
    fragColor = vec4(lit, meshColor.a);
}

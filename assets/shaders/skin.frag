#version 330

uniform sampler2D diffuseTex;
uniform vec4 fogColor = vec4(0.62, 0.78, 1.0, 1.0);
uniform float fogDensity = 0.0004;
uniform vec3 cameraPos = vec3(0.0);

in VertexData
{
    vec2 uv;
    vec3 normal;
    vec3 worldPos;
} inData;

out vec4 fragColor;

void main()
{
    vec3 n = normalize(inData.normal);
    vec3 light0Position = normalize(vec3(-0.4, 0.5, -0.6));
    float NdotL0 = clamp(dot(n, light0Position), 0.0, 1.0);
    vec3 diffuse = vec3(NdotL0 + 0.5);
    diffuse.rgb = clamp(diffuse.rgb, 0.0, 1.0);
    vec3 diffuseColor = texture(diffuseTex, inData.uv).rgb;

    vec3 color = diffuseColor * diffuse;

    float dist = length(inData.worldPos - cameraPos);
    float fogFactor = 1.0 - exp(-fogDensity * dist);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    fragColor = vec4(mix(color, fogColor.rgb, fogFactor), 1.0);
}

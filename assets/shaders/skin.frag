#version 330

uniform sampler2D diffuseTex;
uniform vec4 fogColor = vec4(0.62, 0.78, 1.0, 1.0);
uniform float fogDensity = 0.0004;
uniform vec3 cameraPos = vec3(0.0);
uniform vec3 ambientLight = vec3(0.5);
uniform int lightCount = 1;
uniform vec3 lightDirections[4] = vec3[4](vec3(0.456, -0.570, 0.684), vec3(0.0), vec3(0.0), vec3(0.0));
uniform vec3 lightColors[4] = vec3[4](vec3(1.0), vec3(0.0), vec3(0.0), vec3(0.0));

in VertexData
{
    vec2 uv;
    vec3 normal;
    vec3 worldPos;
} inData;

out vec4 fragColor;

void main()
{
    // The level's lights (its "sun" group), as the cars are lit; without
    // them, one from above and in front, half ambient.
    vec3 n = normalize(inData.normal);
    vec3 diffuse = ambientLight;
    for (int i = 0; i < lightCount; ++i)
        diffuse += lightColors[i] * max(dot(n, -lightDirections[i]), 0.0);
    diffuse = clamp(diffuse, 0.0, 1.0);
    vec3 diffuseColor = texture(diffuseTex, inData.uv).rgb;

    vec3 color = diffuseColor * diffuse;

    float dist = length(inData.worldPos - cameraPos);
    float fogFactor = 1.0 - exp(-fogDensity * dist);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    fragColor = vec4(mix(color, fogColor.rgb, fogFactor), 1.0);
}

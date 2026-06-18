#version 330

uniform sampler2D diffuseTex;
uniform sampler2D lightmapTex;
uniform float alphaMask;
uniform vec4 fogColor = vec4(0.5, 0.5, 0.5, 1.0);
uniform float fogDensity = 0.0;
uniform vec3 cameraPos = vec3(0.0);
uniform float lightmapIntensity = 1.0;

in VertexData
{
    vec2 uv;
    vec2 lightmapUV;
    vec4 color;
    vec3 worldPos;
} inData;

out vec4 fragColor;

void main()
{
    vec4 diffuseColor = texture(diffuseTex, inData.uv);

    if (diffuseColor.a < alphaMask) discard;

    vec4 color = diffuseColor * inData.color;

    vec4 lightmap = texture(lightmapTex, inData.lightmapUV);
    color.rgb *= mix(vec3(1.0), lightmap.rgb, lightmapIntensity);

    float dist = length(inData.worldPos - cameraPos);
    float fogFactor = 1.0 - exp(-fogDensity * dist);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    fragColor = mix(color, fogColor, fogFactor);
}

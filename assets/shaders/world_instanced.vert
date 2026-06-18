#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 color;
layout(location = 3) in mat4 transform;

out VertexData
{
    vec2 uv;
    vec2 lightmapUV;
    vec4 color;
    vec3 worldPos;
} outData;

uniform mat4 viewProj;

void main()
{
    outData.uv = uv;
    outData.lightmapUV = uv;
    outData.color = color;

    vec4 worldPos4 = transform * vec4(position, 1.0);
    outData.worldPos = worldPos4.xyz;
    gl_Position = viewProj * worldPos4;
}

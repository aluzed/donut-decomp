#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 color;

out VertexData
{
    vec2 uv;
    vec4 color;
    vec3 worldPos;
} outData;

uniform mat4 viewProj;

void main()
{
    outData.uv = uv;
    outData.color = color;
    outData.worldPos = position;

	gl_Position = viewProj * vec4(position, 1.0);
}
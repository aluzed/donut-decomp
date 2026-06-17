#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

uniform mat4 viewProj;
uniform vec4 meshColor;

out vec3 fragNormal;
out vec3 fragPos;

void main()
{
    fragNormal = normal;
    fragPos = position;
    gl_Position = viewProj * vec4(position, 1.0);
}

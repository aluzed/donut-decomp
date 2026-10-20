#version 330

layout(location = 0) in vec2 vertex;
layout(location = 1) in vec3 offset;
layout(location = 2) in vec2 size;
layout(location = 3) in vec4 color;
layout(location = 4) in mat4x2 uvs;
// Turned about its middle, radians (a particle's spin); 0 for the billboards,
// which do not give it.
layout(location = 8) in float angle;

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
    outData.uv = uvs[gl_VertexID];
    outData.lightmapUV = vec2(0.0);
    outData.color = color;
    outData.worldPos = offset;

    // The corner, turned: its height in the world, its width on the screen.
    vec2 corner = vertex.xy * size;
    float c = cos(angle), s = sin(angle);
    corner = vec2(corner.x * c - corner.y * s, corner.x * s + corner.y * c);
    vec4 p = viewProj * vec4(vec3(0, corner.y, 0) + offset, 1.0);
    p.x += corner.x;
    gl_Position = p;
}

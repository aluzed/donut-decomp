#version 330

layout(location = 0) in vec2 vertex;
layout(location = 1) in vec3 offset;
layout(location = 2) in vec2 size;
layout(location = 3) in vec4 color;
layout(location = 4) in mat4x2 uvs;
// Turned about its middle, radians (a particle's spin); 0 for the billboards,
// which do not give it.
layout(location = 8) in float angle;
// A quad of no axis (NOAX: a sign on a wall) stands as it is in the world,
// turned by its rotation (x, y, z, w); the others face the camera.
layout(location = 9) in vec4 rotation;
layout(location = 10) in float fixedQuad;

out VertexData
{
    vec2 uv;
    vec2 lightmapUV;
    vec4 color;
    vec3 worldPos;
    vec3 normal;
} outData;

uniform mat4 viewProj;

void main()
{
    outData.uv = uvs[gl_VertexID];
    outData.lightmapUV = vec2(0.0);
    outData.color = color;
    outData.worldPos = offset;
    outData.normal = vec3(0.0, 1.0, 0.0);

    // The corner, turned: its height in the world, its width on the screen.
    vec2 corner = vertex.xy * size;
    float c = cos(angle), s = sin(angle);
    corner = vec2(corner.x * c - corner.y * s, corner.x * s + corner.y * c);
    if (fixedQuad > 0.5)
    {
        vec3 local = vec3(corner, 0.0);
        vec3 t = 2.0 * cross(rotation.xyz, local);
        gl_Position = viewProj * vec4(offset + local + rotation.w * t + cross(rotation.xyz, t), 1.0);
        return;
    }
    vec4 p = viewProj * vec4(vec3(0, corner.y, 0) + offset, 1.0);
    p.x += corner.x;
    gl_Position = p;
}

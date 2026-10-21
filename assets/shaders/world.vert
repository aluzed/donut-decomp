#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec2 lightmapUV;
layout(location = 3) in vec4 color;
// Lit meshes' (the cars'): their normals, turned as the mesh is by
// normalRotation.
layout(location = 4) in vec3 normal;

out VertexData
{
    vec2 uv;
    vec2 lightmapUV;
    vec4 color;
    vec3 worldPos;
    vec3 normal;
} outData;

uniform mat4 viewProj;
uniform mat3 normalRotation = mat3(1.0);

void main()
{
    outData.uv = uv;
    outData.lightmapUV = lightmapUV;
    outData.color = color;
    outData.worldPos = position;
    outData.normal = normalRotation * normal;

	gl_Position = viewProj * vec4(position, 1.0);
}

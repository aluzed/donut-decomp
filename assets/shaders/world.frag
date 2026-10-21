#version 330

uniform sampler2D diffuseTex;
uniform sampler2D lightmapTex;
uniform float alphaMask;
uniform vec4 fogColor = vec4(0.5, 0.5, 0.5, 1.0);
uniform float fogDensity = 0.0;
uniform vec3 cameraPos = vec3(0.0);
uniform float lightmapIntensity = 1.0;
// Lit by the level's lights (its "sun" light group: an ambient light and
// directional ones), for the meshes that are not lit already by their vertex
// colours: the cars. Off, the colour is as it is.
uniform int lighting = 0;
uniform vec3 ambientLight = vec3(1.0);
uniform int lightCount = 0;
uniform vec3 lightDirections[4];
uniform vec3 lightColors[4];
// The material's: lit at all (LIT), its ambient, diffuse and emissive colours.
uniform int materialLit = 1;
uniform vec3 materialAmbient = vec3(1.0);
uniform vec3 materialDiffuse = vec3(1.0);
uniform vec3 materialEmissive = vec3(0.0);
// The sun's shadows of the characters and the cars (FX-004): the depth from
// the sun, where it is, how dark a shadow is. Off at 0, and on the world
// only (unlit: the cars' own positions are not the world's).
// A spheremap shader's reflection (the cars' paint): the sphere map, looked up
// by the normal as the camera sees it, times how much is added; black, none.
uniform vec3 envColor = vec3(0.0);
uniform sampler2D envTex;
uniform mat3 viewRotation = mat3(1.0);
uniform float shadowStrength = 0.0;
uniform sampler2D shadowTex;
uniform mat4 lightViewProj;
uniform float shadowTexel = 1.0 / 2048.0;

in VertexData
{
    vec2 uv;
    vec2 lightmapUV;
    vec4 color;
    vec3 worldPos;
    vec3 normal;
} inData;

out vec4 fragColor;

void main()
{
    vec4 diffuseColor = texture(diffuseTex, inData.uv);

    if (diffuseColor.a < alphaMask) discard;

    vec4 color = diffuseColor * inData.color;
    if (lighting != 0 && materialLit != 0)
    {
        vec3 n = normalize(inData.normal);
        vec3 diffuse = vec3(0.0);
        for (int i = 0; i < lightCount; ++i)
            diffuse += lightColors[i] * max(dot(n, -lightDirections[i]), 0.0);
        vec3 light = materialEmissive + materialAmbient * ambientLight + materialDiffuse * diffuse;
        color.rgb *= min(light, vec3(1.0));
    }

    if (envColor != vec3(0.0))
    {
        vec3 seen = normalize(viewRotation * normalize(inData.normal));
        color.rgb += texture(envTex, vec2(seen.x * 0.5 + 0.5, 0.5 - seen.y * 0.5)).rgb * envColor;
    }

    if (shadowStrength > 0.0 && lighting == 0)
    {
        vec4 fromSun = lightViewProj * vec4(inData.worldPos, 1.0);
        vec3 at = fromSun.xyz * 0.5 + 0.5;
        if (at.x > 0.0 && at.x < 1.0 && at.y > 0.0 && at.y < 1.0 && at.z < 1.0)
        {
            // Nine taps, for a soft edge.
            float shade = 0.0;
            for (int x = -1; x <= 1; ++x)
                for (int y = -1; y <= 1; ++y)
                    shade += at.z - 0.002 > texture(shadowTex, at.xy + vec2(x, y) * shadowTexel).r ? 1.0 : 0.0;
            color.rgb *= 1.0 - shadowStrength * 0.6 * shade / 9.0;
        }
    }

    vec4 lightmap = texture(lightmapTex, inData.lightmapUV);
    color.rgb *= mix(vec3(1.0), lightmap.rgb, lightmapIntensity);

    float dist = length(inData.worldPos - cameraPos);
    float fogFactor = 1.0 - exp(-fogDensity * dist);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    // The fog tints the colour, not the coverage: mixing the alpha too gave
    // every transparent texel a little opacity, and overlapping smoke puffs
    // showed as stacks of squares.
    fragColor = vec4(mix(color.rgb, fogColor.rgb, fogFactor), color.a);
}

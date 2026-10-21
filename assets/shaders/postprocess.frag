#version 330

// The scene onto the screen through the effects. Each is off at 0, and the
// picture is then the scene as drawn.
uniform sampler2D sceneTex;
uniform sampler2D bloomTex;
uniform sampler2D depthTex;
uniform sampler2D lutTex;
uniform vec2 screenSize;

uniform float bloomIntensity;
uniform float vignetteIntensity;
uniform float motionBlur;
uniform float grading;
// Back from the screen to the world with this frame's camera, and into the
// last frame's: where the pixel was a frame ago.
uniform mat4 inverseViewProj;
uniform mat4 previousViewProj;

in vec2 texCoord;
out vec4 fragColor;

// The grading table: 16 by 16 by 16, its blue slices side by side (256 x 16).
vec3 graded(vec3 color)
{
    color = clamp(color, 0.0, 1.0);
    float blue = color.b * 15.0;
    float lower = floor(blue);
    float upper = min(lower + 1.0, 15.0);
    vec2 inSlice = vec2(color.r * 15.0 + 0.5, color.g * 15.0 + 0.5);
    vec3 a = texture(lutTex, vec2((lower * 16.0 + inSlice.x) / 256.0, inSlice.y / 16.0)).rgb;
    vec3 b = texture(lutTex, vec2((upper * 16.0 + inSlice.x) / 256.0, inSlice.y / 16.0)).rgb;
    return mix(a, b, blue - lower);
}

vec3 blurred(vec2 uv)
{
    float depth = texture(depthTex, uv).r;
    vec4 world = inverseViewProj * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    world /= world.w;
    vec4 before = previousViewProj * world;
    before /= before.w;
    vec2 velocity = (uv - (before.xy * 0.5 + 0.5)) * motionBlur;
    // Not smeared across the screen on a cut, or a turn too quick.
    float length = length(velocity);
    if (length > 0.05)
        velocity *= 0.05 / length;
    vec3 sum = vec3(0.0);
    for (int i = 0; i < 8; ++i)
        sum += texture(sceneTex, uv - velocity * (float(i) / 7.0)).rgb;
    return sum / 8.0;
}

void main()
{
    vec3 color = motionBlur > 0.0 ? blurred(texCoord) : texture(sceneTex, texCoord).rgb;
    if (bloomIntensity > 0.0)
        color += texture(bloomTex, texCoord).rgb * bloomIntensity;
    if (grading > 0.0)
        color = mix(color, graded(color), grading);
    float vignette = 1.0 - vignetteIntensity * smoothstep(0.4, 1.4,
        length((texCoord - 0.5) * vec2(1.0, screenSize.x / screenSize.y)));
    fragColor = vec4(color * vignette, 1.0);
}

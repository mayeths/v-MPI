#version 330 core
out vec4 fragColor;

in vec2 TexCoord;

uniform int isCrashed;
uniform sampler2D texture1;
uniform sampler2D texture2;

void main()
{
    float interp = 0.2;
    // fragColor = texture(texture1, TexCoord);
    // fragColor = mix(texture(texture1, TexCoord), texture(texture2, TexCoord), interp);
    vec4 color1 = mix(texture(texture1, TexCoord), texture(texture2, TexCoord), 0.2);
    if (isCrashed == 1) {
        vec4 redMask = vec4(color1.r, 0.0, 0.0, 0.5);
        fragColor = color1 * (1.0 - redMask.a) + redMask;
    } else {
        fragColor = color1;
    }
}


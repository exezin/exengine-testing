#START VS
#version 330 core

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec2 in_uv;

out vec2 uv;

uniform mat4 u_projection;

void main()
{
  gl_Position = u_projection * vec4(in_position.x, in_position.y, 0.0f, 1.0f);
  uv = in_uv;
}
#END VS


#START FS
#version 330 core

in vec2 uv;

out vec4 color;

uniform sampler2D u_texture;
uniform float     u_scale;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
  vec3 sample = texture(u_texture, uv).rgb;
  float dist = u_scale * (median(sample.r, sample.g, sample.b) - 0.5);
  float o = clamp(dist + 0.5, 0.0, 1.0);
  color = vec4(vec3(1.0), o);
}
#END FS
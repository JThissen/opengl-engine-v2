#version 460 core

layout (location = 0) in vec3 in_position;
out vec3 out_position;
uniform float time;

void main() {
  out_position = in_position;
  float pos_x = sin(in_position.x + time);
  float pos_y = sin(in_position.y + time);
  gl_Position = vec4(pos_x, pos_y, in_position.z, 1.0);
}

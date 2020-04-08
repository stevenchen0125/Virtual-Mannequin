R"zzz(#version 330 core
uniform mat4 bone_transform; // transform the cylinder to the correct configuration
const float kPi = 3.1415926535897932384626433832795;
const float mesh_len = 1.0f;
uniform mat4 projection;
uniform mat4 model;
uniform mat4 view;
in vec4 vertex_position;

// FIXME: Implement your vertex shader for cylinders
// Note: you need call sin/cos to transform the input mesh to a cylinder
void main() {
	vec4 cylinder_pos = vertex_position + vec4(0.5, 0, 0, 0);
	float t = cylinder_pos.x * 2 * kPi;
	cylinder_pos = vec4(cos(t), cylinder_pos.y, sin(t), 1);
	mat4 mvpt = projection * view * bone_transform * model;
	gl_Position = mvpt * cylinder_pos;
}
)zzz"

R"zzz(
#version 330 core
uniform vec4 light_position;
uniform vec3 camera_position;

uniform vec3 joint_trans[128];
uniform vec4 joint_rot[128];
uniform mat4 joint_rot_mat[128];

in int jid0;
in int jid1;
in float w0;
in vec3 vector_from_joint0;
in vec3 vector_from_joint1;
in vec4 normal;
in vec2 uv;
in vec4 vert;

out vec4 vs_light_direction;
out vec4 vs_normal;
out vec2 vs_uv;
out vec4 vs_camera_direction;

vec3 qtransform(vec4 q, vec3 v) {
	return v + 2.0 * cross(cross(v, q.xyz) - q.w*v, q.xyz);
}

void main() {
	// FIXME: Implement linear skinning here
	vec4 first_joint = w0 * vec4(qtransform(joint_rot[jid0], vector_from_joint0) + joint_trans[jid0], 1);
	//vec4 first_joint = w0 * (joint_rot_mat[jid0] * vec4(vector_from_joint0, 0) + vec4(joint_trans[jid0], 1)); 
	vec4 second_joint = vec4(0,0,0,0);
	if (jid1 >= 0) {
		second_joint = (1.0f-w0) * vec4(qtransform(joint_rot[jid1], vector_from_joint1) + joint_trans[jid1], 1);
		//second_joint = (1-w0) * (joint_rot_mat[jid1] * vec4(vector_from_joint1, 0) + vec4(joint_trans[jid1], 1));
	}
	gl_Position = first_joint + second_joint;
	vs_normal = normal;
	vs_light_direction = light_position - gl_Position;
	vs_camera_direction = vec4(camera_position, 1.0) - gl_Position;
	vs_uv = uv;
}
)zzz"

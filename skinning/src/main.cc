#include <GL/glew.h>

#include "bone_geometry.h"
#include "procedure_geometry.h"
#include "render_pass.h"
#include "config.h"
#include "gui.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/io.hpp>
#include <debuggl.h>

int window_width = 800, window_height = 600;
const std::string window_title = "Skinning";

const char* vertex_shader =
#include "shaders/default.vert"
;

const char* blending_shader =
#include "shaders/blending.vert"
;

const char* geometry_shader =
#include "shaders/default.geom"
;

const char* fragment_shader =
#include "shaders/default.frag"
;

const char* floor_fragment_shader =
#include "shaders/floor.frag"
;

const char* bone_vertex_shader =
#include "shaders/bone.vert"
;

const char* bone_fragment_shader =
#include "shaders/bone.frag"
;

// FIXME: Add more shaders here.
const char* cylinder_vertex_shader =
#include "shaders/cylinder.vert"
;

const char* cylinder_fragment_shader =
#include "shaders/cylinder.frag"
;

const char* axes_vertex_shader =
#include "shaders/axes.vert"
;

const char* axes_fragment_shader =
#include "shaders/axes.frag"
;

void ErrorCallback(int error, const char* description) {
	std::cerr << "GLFW Error: " << description << "\n";
}

void printVec3(glm::vec3 vec)
{
	std::cout << vec.x << ", " << vec.y << ", " << vec.z << ", " << std::endl;
}

void printVec4(glm::vec4 vec)
{
	std::cout << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << std::endl;
}

GLFWwindow* init_glefw()
{
	if (!glfwInit())
		exit(EXIT_FAILURE);
	glfwSetErrorCallback(ErrorCallback);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 4);
	auto ret = glfwCreateWindow(window_width, window_height, window_title.data(), nullptr, nullptr);
	CHECK_SUCCESS(ret != nullptr);
	glfwMakeContextCurrent(ret);
	glewExperimental = GL_TRUE;
	CHECK_SUCCESS(glewInit() == GLEW_OK);
	glGetError();  // clear GLEW's error for it
	glfwSwapInterval(1);
	const GLubyte* renderer = glGetString(GL_RENDERER);  // get renderer string
	const GLubyte* version = glGetString(GL_VERSION);    // version as a string
	std::cout << "Renderer: " << renderer << "\n";
	std::cout << "OpenGL version supported:" << version << "\n";

	return ret;
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		std::cerr << "Input model file is missing" << std::endl;
		std::cerr << "Usage: " << argv[0] << " <PMD file>" << std::endl;
		return -1;
	}
	GLFWwindow *window = init_glefw();
	GUI gui(window);

	std::vector<glm::vec4> floor_vertices;
	std::vector<glm::uvec3> floor_faces;
	create_floor(floor_vertices, floor_faces);

	LineMesh cylinder_mesh;
	LineMesh axes_mesh;

	// FIXME: we already created meshes for cylinders. Use them to render
	//        the cylinder and axes if required by the assignment.
	create_cylinder_mesh(cylinder_mesh);
	create_axes_mesh(axes_mesh);

	Mesh mesh;
	mesh.loadPmd(argv[1]);
	std::cout << "Loaded object  with  " << mesh.vertices.size()
		<< " vertices and " << mesh.faces.size() << " faces.\n";

	glm::vec4 mesh_center = glm::vec4(0.0f);
	for (size_t i = 0; i < mesh.vertices.size(); ++i) {
		mesh_center += mesh.vertices[i];
	}
	mesh_center /= mesh.vertices.size();

	/*
	 * GUI object needs the mesh object for bone manipulation.
	 */
	gui.assignMesh(&mesh);

	gui.pixel_buffer = malloc(window_height * window_width * 3);

	glm::vec4 light_position = glm::vec4(0.0f, 100.0f, 0.0f, 1.0f);
	MatrixPointers mats; // Define MatrixPointers here for lambda to capture

	/*
	 * In the following we are going to define several lambda functions as
	 * the data source of GLSL uniforms
	 *
	 * Introduction about lambda functions:
	 *      http://en.cppreference.com/w/cpp/language/lambda
	 *      http://www.stroustrup.com/C++11FAQ.html#lambda
	 *
	 * Note: lambda expressions cannot be converted to std::function directly
	 *       Hence we need to declare the data function explicitly.
	 *
	 * CAVEAT: DO NOT RETURN const T&, which compiles but causes
	 *         segfaults.
	 *
	 * Do not worry about the efficient issue, copy elision in C++ 17 will
	 * minimize the performance impact.
	 *
	 * More details about copy elision:
	 *      https://en.cppreference.com/w/cpp/language/copy_elision
	 */

	// FIXME: add more lambdas for data_source if you want to use RenderPass.
	//        Otherwise, do whatever you like here
	std::function<const glm::mat4*()> model_data = [&mats]() {
		return mats.model;
	};
	std::function<glm::mat4()> view_data = [&mats]() { return *mats.view; };
	std::function<glm::mat4()> proj_data = [&mats]() { return *mats.projection; };
	std::function<glm::mat4()> identity_mat = [](){ return glm::mat4(1.0f); };
	std::function<glm::vec3()> cam_data = [&gui](){ return gui.getCamera(); };
	std::function<glm::vec4()> lp_data = [&light_position]() { return light_position; };

	auto std_model = std::make_shared<ShaderUniform<const glm::mat4*>>("model", model_data);
	auto floor_model = make_uniform("model", identity_mat);
	auto std_view = make_uniform("view", view_data);
	auto std_camera = make_uniform("camera_position", cam_data);
	auto std_proj = make_uniform("projection", proj_data);
	auto std_light = make_uniform("light_position", lp_data);

	std::function<float()> alpha_data = [&gui]() {
		static const float transparet = 0.5; // Alpha constant goes here
		static const float non_transparet = 1.0;
		if (gui.isTransparent())
			return transparet;
		else
			return non_transparet;
	};
	auto object_alpha = make_uniform("alpha", alpha_data);

	std::function<std::vector<glm::vec3>()> trans_data = [&mesh](){ return mesh.getCurrentQ()->transData(); };
	std::function<std::vector<glm::fquat>()> rot_data = [&mesh](){ return mesh.getCurrentQ()->rotData(); };
	std::function<std::vector<glm::mat4>()> rotation_data = [&mesh]() {return mesh.getCurrentQ()->rotationData();  };
	auto joint_trans = make_uniform("joint_trans", trans_data);
	auto joint_rot = make_uniform("joint_rot", rot_data);
	auto joint_rotation = make_uniform("joint_rotation", rotation_data);

	// FIXME: define more ShaderUniforms for RenderPass if you want to use it.
	//        Otherwise, do whatever you like here
	glm::mat4 cylinder_rotation;
	glm::mat4 cylinder_scaling;
	glm::mat4 cylinder_translation;
	glm::mat4 bone_mat = glm::mat4(1.0f);
	std::function<glm::mat4()> cylinder_data = [&bone_mat]() {return bone_mat; };
	auto bone_transform = make_uniform("bone_transform", cylinder_data);

	// Floor render pass
	RenderDataInput floor_pass_input;
	floor_pass_input.assign(0, "vertex_position", floor_vertices.data(), floor_vertices.size(), 4, GL_FLOAT);
	floor_pass_input.assignIndex(floor_faces.data(), floor_faces.size(), 3);
	RenderPass floor_pass(-1,
			floor_pass_input,
			{ vertex_shader, geometry_shader, floor_fragment_shader},
			{ floor_model, std_view, std_proj, std_light },
			{ "fragment_color" }
			);

	// PMD Model render pass
	// FIXME: initialize the input data at Mesh::loadPmd
	std::vector<glm::vec2>& uv_coordinates = mesh.uv_coordinates;
	RenderDataInput object_pass_input;
	object_pass_input.assign(0, "jid0", mesh.joint0.data(), mesh.joint0.size(), 1, GL_INT);
	object_pass_input.assign(1, "jid1", mesh.joint1.data(), mesh.joint1.size(), 1, GL_INT);
	object_pass_input.assign(2, "w0", mesh.weight_for_joint0.data(), mesh.weight_for_joint0.size(), 1, GL_FLOAT);
	object_pass_input.assign(3, "vector_from_joint0", mesh.vector_from_joint0.data(), mesh.vector_from_joint0.size(), 3, GL_FLOAT);
	object_pass_input.assign(4, "vector_from_joint1", mesh.vector_from_joint1.data(), mesh.vector_from_joint1.size(), 3, GL_FLOAT);
	object_pass_input.assign(5, "normal", mesh.vertex_normals.data(), mesh.vertex_normals.size(), 4, GL_FLOAT);
	object_pass_input.assign(6, "uv", uv_coordinates.data(), uv_coordinates.size(), 2, GL_FLOAT);
	// TIPS: You won't need vertex position in your solution.
	//       This only serves the stub shader.
	object_pass_input.assign(7, "vert", mesh.vertices.data(), mesh.vertices.size(), 4, GL_FLOAT);
	object_pass_input.assignIndex(mesh.faces.data(), mesh.faces.size(), 3);
	object_pass_input.useMaterials(mesh.materials);
	RenderPass object_pass(-1,
			object_pass_input,
			{
			  blending_shader,
			  geometry_shader,
			  fragment_shader
			},
			{ std_model, std_view, std_proj,
			  std_light,
			  std_camera, object_alpha,
			  joint_trans, joint_rot, joint_rotation
			},
			{ "fragment_color" }
			);

	// Setup the render pass for drawing bones
	// FIXME: You won't see the bones until Skeleton::joints were properly
	//        initialized
	std::vector<int> bone_vertex_id;
	std::vector<glm::uvec2> bone_indices;
	for (int i = 0; i < (int)mesh.skeleton.joints.size(); i++) {
		bone_vertex_id.emplace_back(i);
	}
	for (const auto& joint: mesh.skeleton.joints) {
		if (joint.parent_index < 0)
			continue;
		bone_indices.emplace_back(joint.joint_index, joint.parent_index);
	}
	RenderDataInput bone_pass_input;
	bone_pass_input.assign(0, "jid", bone_vertex_id.data(), bone_vertex_id.size(), 1, GL_UNSIGNED_INT);
	bone_pass_input.assignIndex(bone_indices.data(), bone_indices.size(), 2);
	RenderPass bone_pass(-1, bone_pass_input,
			{ bone_vertex_shader, nullptr, bone_fragment_shader},
			{ std_model, std_view, std_proj, joint_trans },
			{ "fragment_color" }
			);

	// FIXME: Create the RenderPass objects for bones here.
	//        Otherwise do whatever you like.
	RenderDataInput cylinder_pass_input;
	cylinder_pass_input.assign(0, "vertex_position", cylinder_mesh.vertices.data(), cylinder_mesh.vertices.size(), 4, GL_FLOAT);
	cylinder_pass_input.assignIndex(cylinder_mesh.indices.data(), cylinder_mesh.indices.size(), 2);
	RenderPass cylinder_pass(-1, cylinder_pass_input,
		{ cylinder_vertex_shader, nullptr, cylinder_fragment_shader },
		{ bone_transform, std_model, std_view, std_proj },
		{ "fragment_color" }
		);

	RenderDataInput axes_pass_input;
	axes_pass_input.assign(0, "vertex_position", axes_mesh.vertices.data(), axes_mesh.vertices.size(), 4, GL_FLOAT);
	axes_pass_input.assignIndex(axes_mesh.indices.data(), axes_mesh.indices.size(), 2);
	RenderPass axes_pass(-1, axes_pass_input,
		{ axes_vertex_shader, nullptr, axes_fragment_shader },
		{ bone_transform, std_model, std_view, std_proj },
		{ "fragment_color" }
	);

	float aspect = 0.0f;
	std::cout << "center = " << mesh.getCenter() << "\n";

	bool draw_floor = true;
	bool draw_skeleton = true;
	bool draw_object = true;
	bool draw_cylinder = true;

	while (!glfwWindowShouldClose(window)) {
		// Setup some basic window stuff.
		glfwGetFramebufferSize(window, &window_width, &window_height);
		glViewport(0, 0, window_width, window_height);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_MULTISAMPLE);
		glEnable(GL_BLEND);
		glEnable(GL_CULL_FACE);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDepthFunc(GL_LESS);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glCullFace(GL_BACK);

		gui.updateMatrices();
		mats = gui.getMatrixPointers();
#if 0
		std::cerr << model_data() << '\n';
		std::cerr << "call from outside: " << std_model->data_source() << "\n";
		std_model->bind(0);
#endif

		if (gui.isPoseDirty()) {
			mesh.updateAllMatrices();
			mesh.updateAnimation();
			gui.clearPose();
		}

		int current_bone = gui.getCurrentBone();

		// Draw bones first.
		if (draw_skeleton && gui.isTransparent()) {
			bone_pass.setup();
			// Draw our lines.
			// FIXME: you need setup skeleton.joints properly in
			//        order to see the bones.
			CHECK_GL_ERROR(glDrawElements(GL_LINES,
			                              bone_indices.size() * 2,
			                              GL_UNSIGNED_INT, 0));

		}
		draw_cylinder = (current_bone != -1 && gui.isTransparent());
		if (draw_cylinder) {
			Joint curr_joint = mesh.skeleton.joints[current_bone];
			glm::vec3 beg_pos = curr_joint.wcoord;
			glm::vec3 end_pos = curr_joint.position;
			float height = glm::length(end_pos - beg_pos);
			
			glm::vec3 cylinder_axis = glm::normalize(end_pos - beg_pos);
			glm::vec3 y_axis = glm::normalize(glm::vec3(0, 1, 0));
			auto dot = glm::dot(cylinder_axis, y_axis);
			auto cross = glm::cross(cylinder_axis, y_axis);
			auto mag_cross = glm::length(cross);
			const auto epsilon = 1e-7;
			glm::mat3 change_of_coordinates;
			if (mag_cross < epsilon) {
				change_of_coordinates = glm::mat3(1.0f);
				if (glm::dot(cylinder_axis, y_axis) < 0) {
					change_of_coordinates = glm::mat3(-1.0f);
				}
			}
			else {
				auto u = cylinder_axis;
				auto v = glm::normalize(y_axis - dot * cylinder_axis);
				auto w = glm::normalize(-cross);

				glm::mat3 rotation = glm::mat3(0.0f);
				rotation[2][2] = 1;
				rotation[0][0] = dot;
				rotation[1][1] = dot;
				rotation[0][1] = mag_cross;
				rotation[1][0] = -mag_cross;

				glm::mat3 coordinate_change = glm::mat3(u, v, w);

				change_of_coordinates = coordinate_change * rotation * glm::inverse(coordinate_change);
			}
			glm::mat4 cylinder_rotation_helper = glm::mat4(1.0f);
			cylinder_rotation_helper[0][0] = -1;
			cylinder_rotation_helper[2][2] = -1;

			cylinder_rotation = cylinder_rotation_helper * glm::mat4(change_of_coordinates);

			cylinder_scaling = glm::mat4(1.0f);
			cylinder_scaling[0][0] = kCylinderRadius;
			cylinder_scaling[1][1] = height;
			cylinder_scaling[2][2] = kCylinderRadius;

			cylinder_translation = glm::mat4(1.0f);
			cylinder_translation[3][0] = beg_pos.x;
			cylinder_translation[3][1] = beg_pos.y;
			cylinder_translation[3][2] = beg_pos.z;

			bone_mat = cylinder_translation * cylinder_rotation * cylinder_scaling;

			cylinder_pass.setup();
			CHECK_GL_ERROR(glDrawElements(GL_LINES,
				cylinder_mesh.indices.size() * 2,
				GL_UNSIGNED_INT, 0));

			glm::mat4 axes_translation_helper = glm::mat4(1.0f);
			//axes_translation_helper[3][0] = height*cylinder_axis.x;
			//axes_translation_helper[3][1] = height*cylinder_axis.y;
			//axes_translation_helper[3][2] = height*cylinder_axis.z;
			bone_mat = axes_translation_helper * bone_mat;
			axes_pass.setup();
			CHECK_GL_ERROR(glDrawElements(GL_LINES,
				axes_mesh.indices.size() * 2,
				GL_UNSIGNED_INT, 0));
		}

		// Then draw floor.
		if (draw_floor) {
			floor_pass.setup();
			// Draw our triangles.
			CHECK_GL_ERROR(glDrawElements(GL_TRIANGLES,
			                              floor_faces.size() * 3,
			                              GL_UNSIGNED_INT, 0));
		}

		// Draw the model
		if (draw_object) {
			object_pass.setup();
			int mid = 0;
			while (object_pass.renderWithMaterial(mid))
				mid++;
#if 0
			// For debugging also
			if (mid == 0) // Fallback
				CHECK_GL_ERROR(glDrawElements(GL_TRIANGLES, mesh.faces.size() * 3, GL_UNSIGNED_INT, 0));
#endif
		}

		CHECK_GL_ERROR(glReadPixels(0, 0, window_width, window_height, GL_RGB, GL_BYTE, gui.pixel_buffer));

		// Poll and swap.
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}

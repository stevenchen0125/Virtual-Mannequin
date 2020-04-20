#include "gui.h"
#include "config.h"
#include <jpegio.h>
#include "bone_geometry.h"
#include <iostream>
#include <algorithm>
#include <debuggl.h>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace {
	// FIXME: Implement a function that performs proper
	//        ray-cylinder intersection detection
	// TIPS: The implement is provided by the ray-tracer starter code.
}

GUI::GUI(GLFWwindow* window, int view_width, int view_height, int preview_height, int preview_width)
	:window_(window), preview_height_(preview_height), preview_width_(preview_width)
{
	glfwSetWindowUserPointer(window_, this);
	glfwSetKeyCallback(window_, KeyCallback);
	glfwSetCursorPosCallback(window_, MousePosCallback);
	glfwSetMouseButtonCallback(window_, MouseButtonCallback);
	glfwSetScrollCallback(window_, MouseScrollCallback);

	glfwGetWindowSize(window_, &window_width_, &window_height_);
	if (view_width < 0 || view_height < 0) {
		view_width_ = window_width_;
		view_height_ = window_height_;
	} else {
		view_width_ = view_width;
		view_height_ = view_height;
	}
	float aspect_ = static_cast<float>(view_width_) / view_height_;
	projection_matrix_ = glm::perspective((float)(kFov * (M_PI / 180.0f)), aspect_, kNear, kFar);
}

GUI::~GUI()
{
}

void GUI::assignMesh(Mesh* mesh)
{
	mesh_ = mesh;
	center_ = mesh_->getCenter();
}

void GUI::keyCallback(int key, int scancode, int action, int mods)
{
#if 0
	if (action != 2)
		std::cerr << "Key: " << key << " action: " << action << std::endl;
#endif
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window_, GL_TRUE);
		return ;
	}
	if (key == GLFW_KEY_J && action == GLFW_RELEASE) {
		//FIXME save out a screenshot using SaveJPEG
		std::cout << "Saving to capture.jpeg..." << std::endl;
		SaveJPEG("capture.jpeg", view_width_, view_height_, (const unsigned char*)pixel_buffer);
		std::cout << "File saved!" << std::endl;
	}
	if (key == GLFW_KEY_S && (mods & GLFW_MOD_CONTROL)) {
		if (action == GLFW_RELEASE)
			mesh_->saveAnimationTo("animation.json");
		return ;
	}

	if (mods == 0 && captureWASDUPDOWN(key, action))
		return ;
	if (key == GLFW_KEY_LEFT || key == GLFW_KEY_RIGHT) {
		float roll_speed;
		if (key == GLFW_KEY_RIGHT)
			roll_speed = -roll_speed_;
		else
			roll_speed = roll_speed_;
		// FIXME: actually roll the bone here
		if (current_bone_ != -1) {
			Joint curr_joint = mesh_->skeleton.joints[current_bone_];
			glm::vec3 parent_joint_loc;
			if (curr_joint.parent_index == -1) {
				parent_joint_loc = glm::vec3(0, 0, 0);
			}
			else {
				parent_joint_loc = mesh_->skeleton.joints[curr_joint.parent_index].position;
			}
			/*glm::vec3 beg_pos = curr_joint.position;
			glm::vec3 end_pos = parent_joint_loc;*/
			glm::vec3 beg_pos = parent_joint_loc;
			glm::vec3 end_pos = curr_joint.position;

			glm::vec3 cylinder_axis = glm::normalize(end_pos - beg_pos);
			glm::mat4 rotation_matrix = glm::rotate(roll_speed, cylinder_axis);
			updateAllTransformations(current_bone_, rotation_matrix, true);
			pose_changed_ = true;
		}
	} else if (key == GLFW_KEY_C && action != GLFW_RELEASE) {
		fps_mode_ = !fps_mode_;
	} else if (key == GLFW_KEY_LEFT_BRACKET && action == GLFW_RELEASE) {
		current_bone_--;
		current_bone_ += mesh_->getNumberOfBones();
		current_bone_ %= mesh_->getNumberOfBones();
	} else if (key == GLFW_KEY_RIGHT_BRACKET && action == GLFW_RELEASE) {
		current_bone_++;
		current_bone_ += mesh_->getNumberOfBones();
		current_bone_ %= mesh_->getNumberOfBones();
	} else if (key == GLFW_KEY_T && action != GLFW_RELEASE) {
		transparent_ = !transparent_;
	}
	else if (key == GLFW_KEY_F && action == GLFW_RELEASE) {
		//std::cerr << "F" << std::endl;
		mesh_->addKeyframe();
		Keyframe* keyframe = mesh_->getLastKeyFrame();
		keyframe->texture.create(preview_width_, preview_height_);
		texture_to_render = &(keyframe->texture);
		/*keyframe->texture.bind();
		CHECK_GL_ERROR(glClear(GL_DEPTH_BUFFER_BIT));
		keyframe->texture.unbind();*/
	}
	else if (key == GLFW_KEY_R && action == GLFW_RELEASE) {
		//std::cerr << "R" << std::endl;
		mesh_->setPoseFromKeyframe(0);
		start = std::chrono::system_clock::now();
		curr_time = std::chrono::system_clock::now();
		reset_ = true;
		pose_changed_ = true;
		pause_dur = curr_time - start;
	}
	else if (key == GLFW_KEY_P && action == GLFW_RELEASE) {
		//std::cerr << "P: ";
		if (!play_ && reset_) {
			start = std::chrono::system_clock::now();
			play_ = true;
			//std::cerr << "playing!" << std::endl;
			reset_ = false;
		}
		else if (!play_) {
			curr_time = std::chrono::system_clock::now();
			pause_dur += (curr_time - pause_start);
			//std::cerr << "playing!" << std::endl;
			//std::cerr << pause_dur.count() << std::endl;
			play_ = true;
		}
		else if (play_) {
			pause_start = std::chrono::system_clock::now();
			//std::cerr << "pausing!" << std::endl;
			play_ = false;
			reset_ = false;
		}
		pose_changed_ = true;
	}
	else if (key == GLFW_KEY_PAGE_UP && action == GLFW_RELEASE) {
		selected_keyframe -= 1;
		if (selected_keyframe < 0) {
			selected_keyframe = 0;
		}
		if (mesh_->keyframes.size() == 0) {
			selected_keyframe = -1;
		}
	}
	else if (key == GLFW_KEY_PAGE_DOWN && action == GLFW_RELEASE) {
		selected_keyframe += 1;
		if (selected_keyframe >= (int)(mesh_->keyframes.size())) {
			selected_keyframe = (int)(mesh_->keyframes.size()-1);
		}
		if (mesh_->keyframes.size() == 0) {
			selected_keyframe = -1;
		}
	}
	else if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE) {
		if (selected_keyframe != -1 && !play_) {
			mesh_->setPoseFromKeyframe(selected_keyframe);
			pose_changed_ = true;
		}
	}
	else if (key == GLFW_KEY_U && action == GLFW_RELEASE) {
		if (selected_keyframe != -1 && !play_) {
			mesh_->updateKeyframe(selected_keyframe);
			Keyframe* keyframe = mesh_->keyframes[selected_keyframe];
			keyframe->texture.~TextureToRender();
			keyframe->texture.create(preview_width_, preview_height_);
			texture_to_render = &(keyframe->texture);
			pose_changed_ = true;
		}
	}
	else if (key == GLFW_KEY_DELETE && action == GLFW_RELEASE) {
		if (selected_keyframe != -1 && !play_) {
			mesh_->deleteKeyframe(selected_keyframe);
			selected_keyframe = -1;
			/*Keyframe* keyframe = mesh_->keyframes[selected_keyframe];
			keyframe->texture.~TextureToRender();
			keyframe->texture.create(preview_width_, preview_height_);
			texture_to_render = &(keyframe->texture);*/
			pose_changed_ = true;
		}
	}

	// FIXME: implement other controls here.
}

void GUI::updateAllTransformations(int curr_bone, glm::mat4 updateT, bool use_update)
{
	Joint* curr_joint = &mesh_->skeleton.joints[curr_bone];
	int parent_index = curr_joint->parent_index;
	Joint* parent_joint;

	if (use_update) {
		//std::cerr << curr_bone << std::endl;
		if (parent_index == -1) {
			curr_joint->T = updateT * curr_joint->T;

			//curr_joint->orientation = glm::quat_cast(updateT * glm::mat4(curr_joint->orientation));
			/*curr_joint->skinning_T = updateT * curr_joint->skinning_T;*/
		}
		/*curr_joint->passed_up_T = updateT * curr_joint->passed_up_T;*/
		else {
			parent_joint = &mesh_->skeleton.joints[parent_index];
			/*parent_joint->skinning_T = updateT * parent_joint->skinning_T;
			parent_joint->skinning_D = mesh_->calculateSkinningD(parent_index);*/
			curr_joint->T = updateT * curr_joint->T;
			//curr_joint->orientation = glm::quat_cast(updateT * glm::mat4(curr_joint->orientation));
		}
	}
	if (parent_index != -1) {
		parent_joint = &mesh_->skeleton.joints[parent_index];
		curr_joint->D = mesh_->calculateD(curr_bone);
		//parent_joint->D = mesh_->calculateD(curr_bone);
		curr_joint->position = glm::vec3(curr_joint->D * glm::inverse(curr_joint->U) * glm::vec4(curr_joint->init_position, 1));
		curr_joint->wcoord = glm::vec3(curr_joint->D * glm::inverse(curr_joint->U) * glm::vec4(curr_joint->init_wcoord, 1));
		/*curr_joint->skinning_D = mesh_->calculateSkinningD(curr_bone);
		curr_joint->skinning_position = glm::vec3(curr_joint->skinning_D * glm::inverse(curr_joint->skinning_U) * glm::vec4(curr_joint->init_position, 1));*/
		//std::cerr << "Correct position: " << glm::to_string(curr_joint->position) << ", other position: " << glm::to_string(curr_joint->skinning_position) << std::endl;
		//curr_joint->position = glm::vec3(curr_joint->skinning_D * glm::inverse(curr_joint->skinning_U) * glm::vec4(curr_joint->init_position, 1));
		glm::mat4 extract_translation = glm::mat4(0.0f);
		extract_translation[3] = -((curr_joint->D * glm::inverse(curr_joint->U))[3]);
		//extract_translation[3] = -((parent_joint->D * glm::inverse(parent_joint->U))[3]);
		extract_translation[3][3] = 0;
		/*curr_joint->orientation = glm::quat_cast(curr_joint->skinning_D * glm::inverse(curr_joint->skinning_U) + extract_translation);*/
		//curr_joint->orientation = glm::quat_cast(curr_joint->D * glm::inverse(curr_joint->U) + extract_translation);
		if (use_update || true) {
			//updateAllParentsOrientation(current_bone_);
			/*extract_translation[3] = -((curr_joint->D * glm::inverse(curr_joint->U))[3]);
			extract_translation[3][3] = 0;*/
			/*parent_joint->orientation = glm::quat_cast(parent_joint->skinning_D * glm::inverse(parent_joint->skinning_U) + extract_translation);*/
			//parent_joint->D = curr_joint->D;
			//parent_joint->U = curr_joint->U;
			curr_joint->orientation = glm::quat_cast(curr_joint->D * glm::inverse(curr_joint->U) + extract_translation);
			curr_joint->rel_orientation = glm::quat_cast(curr_joint->T);
			//parent_joint->orientation = glm::quat_cast(curr_joint->D * glm::inverse(curr_joint->U) + extract_translation);
			//std::cerr << "Parent: " << glm::to_string(parent_joint->orientation) << " - " << glm::length(parent_joint->orientation) << std::endl;
			//parent_joint->orientation = glm::quat_cast(mesh_->calculateD(parent_index, curr_joint->passed_up_T) * glm::inverse(parent_joint->skinning_U) + extract_translation);
		}
		//curr_joint->orientation = glm::quat_cast(curr_joint->D * glm::inverse(curr_joint->U));
		//std::cerr << glm::to_string(curr_joint->position) << std::endl;
		//std::cerr << glm::to_string(curr_joint->orientation) << std::endl;
		//std::cerr << glm::to_string(curr_joint->D) << " \ncompared to\n " << glm::to_string(curr_joint->skinning_D) << std::endl << std::endl;
		/*parent_joint->D = mesh_->calculateD(parent_index, parent_joint->passed_up_T);
		parent_joint->position = glm::vec3(parent_joint->D * glm::inverse(parent_joint->U) * glm::vec4(parent_joint->init_position, 1));
		parent_joint->orientation = glm::quat_cast(updateT * parent_joint->passed_up_T);*/
	}
	else {
		curr_joint->D = mesh_->calculateD(curr_bone);
		/*curr_joint->position = curr_joint->T * glm::vec4(curr_joint->init_position, 1);*/
		curr_joint->position = glm::vec3(curr_joint->D * glm::inverse(curr_joint->U) * glm::vec4(curr_joint->init_position, 1));
		curr_joint->wcoord = glm::vec3(curr_joint->D * glm::inverse(curr_joint->U) * glm::vec4(curr_joint->init_wcoord, 1));
		glm::mat4 extract_translation = glm::mat4(0.0f);
		extract_translation[3] = -((curr_joint->D * glm::inverse(curr_joint->U))[3]);
		extract_translation[3][3] = 0;
		//curr_joint->orientation = glm::quat_cast(curr_joint->D * glm::inverse(curr_joint->U) + extract_translation);
		//curr_joint->orientation = glm::quat_cast(curr_joint->T);
		/*curr_joint->orientation = glm::quat_cast(curr_joint->skinning_D * glm::inverse(curr_joint->skinning_U) + extract_translation);*/
		curr_joint->orientation = glm::quat_cast(curr_joint->D * glm::inverse(curr_joint->U) + extract_translation);
	}
	for (auto child : curr_joint->children)
	{
		updateAllTransformations(child, glm::mat4(1.0f), false);
	}
	//std::cout << "Bone: " << curr_bone << ", Beginning: " << glm::to_string(curr_joint->wcoord) << ", End: " << glm::to_string(curr_joint->position) << std::endl;
}

int GUI::intersectCylinder(glm::vec3 direction, glm::vec3 position)
{
	double best_t = 1e6;
	int best_i = -1;
	auto epsilon = 1e-7;

	for (int i = 0; (unsigned)i < mesh_->skeleton.joints.size(); i++) {
		Joint curr_joint = mesh_->skeleton.joints[i];
		glm::vec3 parent_joint_loc;
		if (curr_joint.parent_index == -1) {
			parent_joint_loc = glm::vec3(0, 0, 0);
		}
		else {
			parent_joint_loc = mesh_->skeleton.joints[curr_joint.parent_index].position;
		}


		glm::vec3 beg_pos = parent_joint_loc;
		glm::vec3 end_pos = curr_joint.position;

		/*glm::vec3 cylinder_axis = glm::normalize(beg_pos - end_pos);*/
		glm::vec3 cylinder_axis = glm::normalize(end_pos - beg_pos);
		/*auto height = glm::length(beg_pos - end_pos);*/
		auto height = glm::length(end_pos - beg_pos);
		glm::vec3 z_axis = glm::normalize(glm::vec3(0, 0, 1));
		auto dot = glm::dot(cylinder_axis, z_axis);
		auto cross = glm::cross(cylinder_axis, z_axis);
		auto mag_cross = glm::length(cross);
		glm::mat3 change_of_coordinates;
		if (mag_cross < epsilon) {
			change_of_coordinates = glm::mat3(1.0f);
			if (glm::dot(cylinder_axis, z_axis) < 0) {
				change_of_coordinates = glm::mat3(-1.0f);
			}
		}
		else {
			auto u = cylinder_axis;
			auto v = glm::normalize(z_axis - dot * cylinder_axis);
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

		glm::vec3 changed_direction = glm::normalize(change_of_coordinates * direction);
		glm::vec3 changed_position = change_of_coordinates * (position - beg_pos);
		auto vx = changed_direction.x;
		auto vy = changed_direction.y;
		auto px = changed_position.x;
		auto py = changed_position.y;
		auto a = vx * vx + vy * vy;
		auto b = 2 * (px * vx + py * vy);
		auto c = px * px + py * py - kCylinderRadius * kCylinderRadius;

		if (a == 0) {
			continue;
		}
		auto discriminant = b * b - 4 * a * c;
		if (discriminant < 0) {
			continue;
		}
		discriminant = sqrt(discriminant);
		auto t2 = (-b + discriminant) / (2 * a);
		if (t2 < 0) {
			continue;
		}
		auto t1 = (-b - discriminant) / (2 * a);
		double z = t1 * (double)changed_direction.z + (double)changed_position.z;
		if (t1 < best_t && z >= 0 && z <= height) {
			best_t = t1;
			best_i = i;
		}
	}
	return best_i;
}

void GUI::mousePosCallback(double mouse_x, double mouse_y)
{
	last_x_ = current_x_;
	last_y_ = current_y_;
	current_x_ = mouse_x;
	current_y_ = window_height_ - mouse_y;
	float delta_x = current_x_ - last_x_;
	float delta_y = current_y_ - last_y_;
	if (sqrt(delta_x * delta_x + delta_y * delta_y) < 1e-15)
		return;
	if (mouse_x > view_width_)
		return;
	glm::vec3 mouse_direction = glm::normalize(glm::vec3(delta_x, delta_y, 0.0f));
	glm::vec2 mouse_start = glm::vec2(last_x_, last_y_);
	glm::vec2 mouse_end = glm::vec2(current_x_, current_y_);
	glm::uvec4 viewport = glm::uvec4(0, 0, view_width_, view_height_);

	bool drag_camera = drag_state_ && current_button_ == GLFW_MOUSE_BUTTON_RIGHT;
	bool drag_bone = drag_state_ && current_button_ == GLFW_MOUSE_BUTTON_LEFT;

	glm::vec3 current_mouse_position = (glm::unProject(glm::vec3(current_x_, current_y_, -1), view_matrix_ * model_matrix_, projection_matrix_, viewport));
	glm::vec3 previous_mouse_position = (glm::unProject(glm::vec3(last_x_, last_y_, -1), view_matrix_ * model_matrix_, projection_matrix_, viewport));
	glm::vec3 mouse_ray = glm::normalize(current_mouse_position - eye_);
	glm::vec3 mouse_world_direction = glm::normalize(current_mouse_position - previous_mouse_position);
	int intersection_index = intersectCylinder(mouse_ray, eye_);

	current_bone_ = intersection_index;
	if (current_bone_ != -1) {
		//std::cerr << current_bone_ << std::endl;
	}

	if (drag_camera) {
		glm::vec3 axis = glm::normalize(
				orientation_ *
				glm::vec3(mouse_direction.y, -mouse_direction.x, 0.0f)
				);
		orientation_ =
			glm::mat3(glm::rotate(rotation_speed_, axis) * glm::mat4(orientation_));
		tangent_ = glm::column(orientation_, 0);
		up_ = glm::column(orientation_, 1);
		look_ = glm::column(orientation_, 2);
	} else if (drag_bone && current_bone_ != -1) {
		// FIXME: Handle bone rotation
		glm::vec3 axis = glm::normalize(
			orientation_ *
			glm::vec3(mouse_direction.y, -mouse_direction.x, 0.0f)
		);

		Joint curr_joint = mesh_->skeleton.joints[current_bone_];
		glm::vec3 end_pos = curr_joint.position;
		glm::vec3 beg_pos = curr_joint.wcoord;

		glm::vec3 cylinder_axis = glm::normalize(beg_pos - end_pos);

		glm::vec3 mouse_cross = glm::cross(mouse_world_direction, cylinder_axis);
		glm::vec3 look_cross = glm::cross(look_, cylinder_axis);
		glm::vec3 tan_cross = glm::cross(tangent_, cylinder_axis);
		bool use_look = std::abs(glm::dot(mouse_cross, look_)) > std::abs(glm::dot(mouse_cross, tangent_));
		auto rotation_amount = glm::dot(glm::normalize(mouse_cross), use_look ? look_ : -tangent_) * rotation_speed_;
		if (std::abs(rotation_amount) < 1e-6) {
			return;
		}
		glm::mat4 rotation_matrix = glm::rotate(rotation_amount, look_);
		updateAllTransformations(current_bone_, rotation_matrix, true);
		pose_changed_ = true;
		return ;
	}

	// FIXME: highlight bones that have been moused over
}

void GUI::mouseButtonCallback(int button, int action, int mods)
{
	if (current_x_ <= view_width_) {
		drag_state_ = (action == GLFW_PRESS);
		current_button_ = button;
		return ;
	}
	else
	{
		//drag_state_ = (action == GLFW_PRESS);
		current_button_ = button;
	}
	// FIXME: Key Frame Selection
	if (current_button_ == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		/*std::cerr << "Left click at: " << current_x_ << ", " << current_y_ << " with current scroll: " << current_scroll  << " frame: " << 
			(view_height_ - current_y_ - current_scroll)/preview_height_<< std::endl;*/
		int attempted_keyframe = (view_height_ - current_y_ - current_scroll) / preview_height_;
		if (attempted_keyframe >= 0 && attempted_keyframe < (int)(mesh_->keyframes.size())) {
			selected_keyframe = attempted_keyframe;
		}
	}
}

void GUI::mouseScrollCallback(double dx, double dy)
{
	if (current_x_ < view_width_)
		return;
	// FIXME: Mouse Scrolling
	if (mesh_->keyframes.size() == 0) {
		current_scroll = 0;
		return;
	}
	current_scroll += (int)(scroll_speed * dy);
	if (current_scroll > 0) {
		current_scroll = 0;
	}
	if (current_scroll < -(int)(mesh_->keyframes.size()-1) * preview_height_) {
		current_scroll = -(int)(mesh_->keyframes.size()-1) * preview_height_;
	}
}

void GUI::updateMatrices()
{
	// Compute our view, and projection matrices.
	if (fps_mode_)
		center_ = eye_ + camera_distance_ * look_;
	else
		eye_ = center_ - camera_distance_ * look_;

	view_matrix_ = glm::lookAt(eye_, center_, up_);
	light_position_ = glm::vec4(eye_, 1.0f);

	aspect_ = static_cast<float>(view_width_) / view_height_;
	projection_matrix_ =
		glm::perspective((float)(kFov * (M_PI / 180.0f)), aspect_, kNear, kFar);
	model_matrix_ = glm::mat4(1.0f);
}

MatrixPointers GUI::getMatrixPointers() const
{
	MatrixPointers ret;
	ret.projection = &projection_matrix_;
	ret.model= &model_matrix_;
	ret.view = &view_matrix_;
	return ret;
}

bool GUI::setCurrentBone(int i)
{
	if (i < 0 || i >= mesh_->getNumberOfBones())
		return false;
	current_bone_ = i;
	return true;
}

float GUI::getCurrentPlayTime() const
{
	std::chrono::duration<float> dur;
	std::chrono::time_point<std::chrono::system_clock> curr = std::chrono::system_clock::now();
	dur = curr - start;
	if (!play_) {
		return 0.0f;
	}
	return dur.count()-pause_dur.count();
}


bool GUI::captureWASDUPDOWN(int key, int action)
{
	if (key == GLFW_KEY_W) {
		if (fps_mode_)
			eye_ += zoom_speed_ * look_;
		else
			camera_distance_ -= zoom_speed_;
		return true;
	} else if (key == GLFW_KEY_S) {
		if (fps_mode_)
			eye_ -= zoom_speed_ * look_;
		else
			camera_distance_ += zoom_speed_;
		return true;
	} else if (key == GLFW_KEY_A) {
		if (fps_mode_)
			eye_ -= pan_speed_ * tangent_;
		else
			center_ -= pan_speed_ * tangent_;
		return true;
	} else if (key == GLFW_KEY_D) {
		if (fps_mode_)
			eye_ += pan_speed_ * tangent_;
		else
			center_ += pan_speed_ * tangent_;
		return true;
	} else if (key == GLFW_KEY_DOWN) {
		if (fps_mode_)
			eye_ -= pan_speed_ * up_;
		else
			center_ -= pan_speed_ * up_;
		return true;
	} else if (key == GLFW_KEY_UP) {
		if (fps_mode_)
			eye_ += pan_speed_ * up_;
		else
			center_ += pan_speed_ * up_;
		return true;
	}
	return false;
}


// Delegrate to the actual GUI object.
void GUI::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	GUI* gui = (GUI*)glfwGetWindowUserPointer(window);
	gui->keyCallback(key, scancode, action, mods);
}

void GUI::MousePosCallback(GLFWwindow* window, double mouse_x, double mouse_y)
{
	GUI* gui = (GUI*)glfwGetWindowUserPointer(window);
	gui->mousePosCallback(mouse_x, mouse_y);
}

void GUI::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	GUI* gui = (GUI*)glfwGetWindowUserPointer(window);
	gui->mouseButtonCallback(button, action, mods);
}

void GUI::MouseScrollCallback(GLFWwindow* window, double dx, double dy)
{
	GUI* gui = (GUI*)glfwGetWindowUserPointer(window);
	gui->mouseScrollCallback(dx, dy);
}

#include "config.h"
#include "bone_geometry.h"
#include "texture_to_render.h"
#include <fstream>
#include <queue>
#include <iostream>
#include <stdexcept>
#include <glm/gtx/io.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {
	constexpr glm::fquat identity_quat(1.0, 0.0, 0.0, 0.0);
}

/*
 * For debugging purpose.
 */
template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
	size_t count = std::min(v.size(), static_cast<size_t>(10));
	for (size_t i = 0; i < count; ++i) os << i << " " << v[i] << "\n";
	os << "size = " << v.size() << "\n";
	return os;
}

std::ostream& operator<<(std::ostream& os, const BoundingBox& bounds)
{
	os << "min = " << bounds.min << " max = " << bounds.max;
	return os;
}



const glm::vec3* Skeleton::collectJointTrans() const
{
	return cache.trans.data();
}

const glm::fquat* Skeleton::collectJointRot() const
{
	return cache.rot.data();
}

// FIXME: Implement bone animation.

void Skeleton::refreshCache(Configuration* target)
{
	if (target == nullptr)
		target = &cache;
	target->rot.resize(joints.size());
	target->trans.resize(joints.size());
	for (size_t i = 0; i < joints.size(); i++) {
		target->rot[i] = joints[i].orientation;
		target->trans[i] = joints[i].position;
	}
}


Mesh::Mesh()
{
}

Mesh::~Mesh()
{
}

Keyframe* Mesh::getLastKeyFrame()
{
	Keyframe* keyframe = keyframes[keyframes.size() - 1];
	return keyframe;
}

void Mesh::addKeyframe()
{
	Keyframe* newKeyframe = new Keyframe();
	keyframes.push_back(newKeyframe);
	Keyframe* keyframe = keyframes[keyframes.size()-1];
	keyframe->U.clear();
	keyframe->T.clear();
	keyframe->D.clear();
	keyframe->orientation.clear();
	keyframe->rel_orientation.clear();
	updateAllMatrices();
	for (int i = 0; (unsigned)i < skeleton.joints.size(); i++) {
		Joint* curr_joint = &(skeleton.joints[i]);
		keyframe->U.push_back(curr_joint->U);
		keyframe->T.push_back(curr_joint->T);
		keyframe->D.push_back(curr_joint->D);
		keyframe->orientation.push_back(curr_joint->orientation);
		keyframe->rel_orientation.push_back(curr_joint->rel_orientation);
	}
}

void Mesh::updateKeyframe(int keyframeid)
{
	Keyframe* keyframe = keyframes[keyframeid];
	keyframe->U.clear();
	keyframe->T.clear();
	keyframe->D.clear();
	keyframe->orientation.clear();
	keyframe->rel_orientation.clear();
	updateAllMatrices();
	for (int i = 0; (unsigned)i < skeleton.joints.size(); i++) {
		Joint* curr_joint = &(skeleton.joints[i]);
		keyframe->U.push_back(curr_joint->U);
		keyframe->T.push_back(curr_joint->T);
		keyframe->D.push_back(curr_joint->D);
		keyframe->orientation.push_back(curr_joint->orientation);
		keyframe->rel_orientation.push_back(curr_joint->rel_orientation);
	}
}

void Mesh::deleteKeyframe(int keyframeid)
{
	Keyframe* keyframe = keyframes[keyframeid];
	keyframes.erase(keyframes.begin()+keyframeid, keyframes.begin()+keyframeid+1);
	keyframe->U.clear();
	keyframe->T.clear();
	keyframe->D.clear();
	keyframe->orientation.clear();
	keyframe->rel_orientation.clear();
	keyframe->texture.~TextureToRender();
	free(keyframe);
}

void Mesh::setPoseFromKeyframe(int keyframeid)
{
	if (keyframes.size() == 0) {
		return;
	}
	Keyframe* keyframe = keyframes[keyframeid];
	for (int i = 0; (unsigned)i < skeleton.joints.size(); i++) {
		Joint* curr_joint = &(skeleton.joints[i]);
		glm::mat4 curr_T = keyframe->T[i];
		glm::mat4 curr_U = keyframe->U[i];
		glm::mat4 curr_D = keyframe->D[i];
		glm::fquat curr_orientation = keyframe->orientation[i];
		glm::fquat rel_orientation = keyframe->rel_orientation[i];
		curr_joint->U = curr_U;
		curr_joint->T = curr_T;
		curr_joint->D = curr_D;
		curr_joint->orientation = curr_orientation;
		curr_joint->rel_orientation = rel_orientation;
	}
	updateAllPositionsAndRotations();
}

void Mesh::setInterpolation(int keyframeid, float percent)
{
	//MAYBE: NEED TO DO THIS METHOD STILL
	Keyframe* curr_keyframe = keyframes[keyframeid];
	Keyframe* next_keyframe = keyframes[keyframeid + 1];
	for (int i = 0; (unsigned)i < skeleton.joints.size(); i++) {
		Joint* curr_joint = &(skeleton.joints[i]);
		glm::mat4 curr_T = curr_keyframe->T[i];
		glm::mat4 curr_U = curr_keyframe->U[i];
		glm::mat4 curr_D = curr_keyframe->D[i];

		glm::mat4 next_T = next_keyframe->T[i];
		glm::mat4 next_U = next_keyframe->U[i];
		glm::mat4 next_D = next_keyframe->D[i];

		glm::mat4 curr_mat = curr_D * glm::inverse(curr_U);
		glm::mat4 next_mat = next_D * glm::inverse(next_U);
		
		glm::mat4 extract_translation = glm::mat4(0.0f);
		glm::vec4 curr_trans, next_trans;

		extract_translation = glm::mat4(0.0f);
		extract_translation[3] = -((curr_mat)[3]);
		curr_trans = -extract_translation[3];
		extract_translation[3][3] = 0;
		glm::fquat curr_orientation = glm::quat_cast(curr_mat + extract_translation);

		extract_translation = glm::mat4(0.0f);
		extract_translation[3] = -((next_mat)[3]);
		next_trans = -extract_translation[3];
		extract_translation[3][3] = 0;
		glm::fquat next_orientation = glm::quat_cast(next_mat + extract_translation);

		//glm::vec4 trans = glm::mix(curr_trans, next_trans, percent);

		glm::fquat set_orientation = glm::mix(curr_orientation, next_orientation, percent);
		//glm::mat4 mat = glm::toMat4(set_orientation);
		//mat[3] = trans;

		glm::fquat curr_rel_orientation = curr_keyframe->rel_orientation[i];
		glm::fquat next_rel_orientation = next_keyframe->rel_orientation[i];
		glm::fquat set_rel_orientation = glm::mix(curr_rel_orientation, next_rel_orientation, percent);
		curr_joint->rel_orientation = set_rel_orientation;
		curr_joint->orientation = set_orientation;
	}
	setTFromRelOrientation();
	updateAllPositionsAndRotations();
}

void Mesh::setTFromRelOrientation()
{
	for (int i = 0; (unsigned)i < skeleton.joints.size(); i++) {
		Joint* curr_joint = &(skeleton.joints[i]);
		curr_joint->T = glm::toMat4(curr_joint->rel_orientation);
	}
	updateAllMatrices();
}

void Mesh::updateAllMatrices()
{
	for (int i = 0; (unsigned)i < skeleton.joints.size(); i++) {
		Joint* curr_joint = &(skeleton.joints[i]);
		curr_joint->D = calculateD(i);
	}
	//updateAllRotations();
}

void Mesh::updateAllPositionsAndRotations()
{
	for (int i = 0; (unsigned)i < skeleton.joints.size(); i++) {
		Joint* curr_joint = &(skeleton.joints[i]);

		curr_joint->position = glm::vec3(curr_joint->D * glm::inverse(curr_joint->U) * glm::vec4(curr_joint->init_position, 1));
		curr_joint->wcoord = glm::vec3(curr_joint->D * glm::inverse(curr_joint->U) * glm::vec4(curr_joint->init_wcoord, 1));

		glm::mat4 extract_translation = glm::mat4(0.0f);
		extract_translation[3] = -((curr_joint->D * glm::inverse(curr_joint->U))[3]);
		extract_translation[3][3] = 0;
		curr_joint->orientation = glm::quat_cast(curr_joint->D * glm::inverse(curr_joint->U) + extract_translation);
	}
}

void Mesh::updateAllRotations()
{
	return;
	for (int i = 0; (unsigned)i < skeleton.joints.size(); i++) {
		Joint* curr_joint = &(skeleton.joints[i]);
		curr_joint->D = calculateD(i);
	}
}

void Mesh::loadDefaults()
{
	for (int i = 0; (unsigned)i < skeleton.joints.size(); i++) {
		Joint* curr_joint = &(skeleton.joints[i]);
		curr_joint->T = glm::mat4(1.0f);
		curr_joint->orientation = glm::fquat(1.0f, 0.0f, 0.0f, 0.0f);
		curr_joint->rel_orientation = glm::fquat(1.0f, 0.0f, 0.0f, 0.0f);

		/*curr_joint->position = glm::vec3(curr_joint->D * glm::inverse(curr_joint->U) * glm::vec4(curr_joint->init_position, 1));
		curr_joint->wcoord = glm::vec3(curr_joint->D * glm::inverse(curr_joint->U) * glm::vec4(curr_joint->init_wcoord, 1));

		glm::mat4 extract_translation = glm::mat4(0.0f);
		extract_translation[3] = -((curr_joint->D * glm::inverse(curr_joint->U))[3]);
		extract_translation[3][3] = 0;
		curr_joint->orientation = glm::quat_cast(curr_joint->D * glm::inverse(curr_joint->U) + extract_translation);*/
	}
	updateAllMatrices();
	updateAllPositionsAndRotations();
}

glm::mat4 Mesh::calculateU(int id)
{
	Joint* curr_joint = &skeleton.joints[id];
	if (curr_joint->parent_index == -1) {
		glm::mat4 B = glm::mat4(1.0f);
		B[3] = glm::vec4(curr_joint->init_wcoord, 1) - glm::vec4(0, 0, 0, 0);
		return B * glm::mat4(1.0f);
	}
	else {
		glm::mat4 parentU = skeleton.joints[curr_joint->parent_index].U;
		glm::mat4 B = glm::mat4(1.0f);
		B[3] = glm::vec4(curr_joint->init_wcoord, 1) - glm::vec4(skeleton.joints[curr_joint->parent_index].init_wcoord, 0);
		skeleton.joints[id].init_rel_position = glm::vec3(B[3]);
		return parentU * B;
	}
}

glm::mat4 Mesh::calculateD(int id)
{
	Joint* curr_joint = &skeleton.joints[id];
	Joint* parent_joint;
	if (curr_joint->parent_index == -1) {
		glm::mat4 B = glm::mat4(1.0f);
		B[3] = glm::vec4(curr_joint->init_wcoord, 1) - glm::vec4(0, 0, 0, 0);
		return B * curr_joint->T;
	}
	else {
		parent_joint = &skeleton.joints[curr_joint->parent_index];
		glm::mat4 B = glm::mat4(1.0f);
		B[3] = glm::vec4(curr_joint->init_wcoord, 1) - glm::vec4(parent_joint->init_wcoord, 0);
		//return calculateD(curr_joint->parent_index) * B * glm::toMat4(curr_joint->orientation);
		return calculateD(curr_joint->parent_index) * B * curr_joint->total_roll * curr_joint->T;
	}
}

void Mesh::loadPmd(const std::string& fn)
{
	MMDReader mr;
	mr.open(fn);
	mr.getMesh(vertices, faces, vertex_normals, uv_coordinates);
	computeBounds();
	mr.getMaterial(materials);

	// FIXME: load skeleton and blend weights from PMD file,
	//        initialize std::vectors for the vertex attributes,
	//        also initialize the skeleton as needed
	glm::vec3 wcoord;
	int parent;

	int curr_id = 0;
	skeleton.joints.clear();
	while (mr.getJoint(curr_id, wcoord, parent)) {
		Joint curr_joint;
		if (parent == -1) {
			curr_joint = Joint(curr_id, wcoord, parent);
			curr_joint.wcoord = glm::vec3(0, 0, 0);
			curr_joint.init_wcoord = curr_joint.wcoord;
			//std::cerr << glm::to_string(curr_joint.wcoord) << " " << glm::to_string(wcoord) << std::endl;
		}
		else
		{
			curr_joint = Joint(curr_id, wcoord, parent);
			curr_joint.wcoord = skeleton.joints[parent].position;
			curr_joint.init_wcoord = curr_joint.wcoord;
			//std::cerr << glm::to_string(curr_joint.wcoord) << " " << glm::to_string(wcoord) << std::endl;
		}
		//curr_joint = Joint(curr_id, wcoord, parent);
		curr_joint.T = glm::mat4(1.0f);
		curr_joint.total_roll = glm::mat4(1.0f);
		//curr_joint.passed_up_T = glm::mat4(1.0f);
		curr_joint.children.clear();
		skeleton.joints.push_back(curr_joint);
		Joint* curr = &skeleton.joints[skeleton.joints.size() - 1];
		curr->U = calculateU(curr_id);
		curr->D = calculateD(curr_id);
		//curr->skinning_U = calculateSkinningU(curr_id);
		//curr->skinning_D = calculateSkinningD(curr_id);
		//curr->skinning_T = glm::mat4(1.0f);
		curr->orientation = glm::fquat(1.0f, 0.0f, 0.0f, 0.0f);
		curr->rel_orientation = glm::fquat(1.0f, 0.0f, 0.0f, 0.0f);
		curr->is_root = parent == -1;
		if (curr->parent_index != -1) {
			skeleton.joints[skeleton.joints[skeleton.joints.size() - 1].parent_index].children.push_back(curr_id);
		}
		curr_id++;
	}
	std::vector<SparseTuple> jointWeights;
	jointWeights.clear();
	mr.getJointWeights(jointWeights);
	for (auto tuple : jointWeights)
	{
		joint0.push_back(tuple.jid0);
		joint1.push_back(tuple.jid1);
		weight_for_joint0.push_back(tuple.weight0);
		int parent0 = skeleton.joints[tuple.jid0].parent_index;
		Joint* parent_0 = &skeleton.joints[parent0];
		vector_from_joint0.push_back(glm::vec3(vertices[tuple.vid]) - skeleton.joints[tuple.jid0].init_position);
		if (tuple.jid1 >= 0) {
			vector_from_joint1.push_back(glm::vec3(vertices[tuple.vid]) - skeleton.joints[tuple.jid1].init_position);
		}
		else
		{
			vector_from_joint1.push_back(glm::vec3(vertices[tuple.vid]));
		}
	}
}

int Mesh::getNumberOfBones() const
{
	return skeleton.joints.size();
}

void Mesh::computeBounds()
{
	bounds.min = glm::vec3(std::numeric_limits<float>::max());
	bounds.max = glm::vec3(-std::numeric_limits<float>::max());
	for (const auto& vert : vertices) {
		bounds.min = glm::min(glm::vec3(vert), bounds.min);
		bounds.max = glm::max(glm::vec3(vert), bounds.max);
	}
}

void Mesh::updateAnimation(float t)
{
	skeleton.refreshCache(&currentQ_);
	if (t == -1.0f) {
		return;
	}
	// FIXME: Support Animation Here
	if (keyframes.size() == 0) {
		return;
	}
	if (t <= 0 && keyframes.size() >= 1) {
		setPoseFromKeyframe(0);
	}
	else if (t > keyframes.size()-1 && t > 0) {
		setPoseFromKeyframe(keyframes.size()-1);
	}
	else {
		int frame = (int)t;
		float percent = t - frame;
		setInterpolation(frame, percent);
	}
	skeleton.refreshCache(&currentQ_);
}

const Configuration*
Mesh::getCurrentQ() const
{
	return &currentQ_;
}


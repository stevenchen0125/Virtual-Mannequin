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

#include <glm/gtx/string_cast.hpp>

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

const glm::mat4* Skeleton::collectJointRotations() const
{
	return cache.rotations.data();
}

// FIXME: Implement bone animation.

void Skeleton::refreshCache(Configuration* target)
{
	if (target == nullptr)
		target = &cache;
	target->rot.resize(joints.size());
	target->trans.resize(joints.size());
	target->rotations.resize(joints.size());
	for (size_t i = 0; i < joints.size(); i++) {
		target->rot[i] = joints[i].orientation;
		target->trans[i] = joints[i].position;
		//target->rotations[i] = joints[i].D * glm::inverse(joints[i].U);
	}
}


Mesh::Mesh()
{
}

Mesh::~Mesh()
{
}

//glm::mat4 Mesh::calculateSkinningU(int id)
//{
//	return calculateU(id);
//}

void Mesh::updateAllMatrices()
{
	for (int i = 0; (unsigned)i < skeleton.joints.size(); i++) {
		Joint* curr_joint = &(skeleton.joints[i]);
		curr_joint->D = calculateD(i);
	}
	updateAllRotations();
}

void Mesh::updateAllRotations()
{
	return;
	for (int i = 0; (unsigned)i < skeleton.joints.size(); i++) {
		Joint* curr_joint = &(skeleton.joints[i]);
		curr_joint->D = calculateD(i);
	}
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

//glm::mat4 Mesh::calculateSkinningD(int id)
//{
//	Joint* curr_joint = &skeleton.joints[id];
//	Joint* parent_joint;
//	int parent_index = curr_joint->parent_index;
//	if (parent_index == -1) {
//		glm::mat4 B = glm::mat4(1.0f);
//		B[3] = glm::vec4(curr_joint->position, 1) - glm::vec4(0, 0, 0, 0);
//		return B * curr_joint->skinning_T;
//	}
//	else
//	{
//		parent_joint = &skeleton.joints[curr_joint->parent_index];
//		glm::mat4 B = glm::mat4(1.0f);
//		B[3] = glm::vec4(curr_joint->init_position, 1) - glm::vec4(parent_joint->init_position, 0);
//		return calculateSkinningD(parent_index) * B * curr_joint->skinning_T;
//	}
//}

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
			curr_joint.wcoord = glm::vec3(0,0,0);
			curr_joint.init_wcoord = curr_joint.wcoord;
		}
		else
		{
			curr_joint = Joint(curr_id, wcoord, parent);
			curr_joint.wcoord = skeleton.joints[parent].position;
			curr_joint.init_wcoord = curr_joint.wcoord;
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
	// FIXME: Support Animation Here
}

const Configuration*
Mesh::getCurrentQ() const
{
	return &currentQ_;
}


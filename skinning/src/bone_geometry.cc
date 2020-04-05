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

glm::mat4 calculateU(std::vector<Joint> joints, int id)
{
	Joint curr_joint = joints[id];
	if (curr_joint.parent_index == -1) {
		glm::mat4 B = glm::mat4(1.0f);
		B[3] = glm::vec4(curr_joint.position, 1) - glm::vec4(0, 0, 0, 0);
		return B * glm::mat4(1.0f);
	}
	else {
		glm::mat4 parentU = joints[curr_joint.parent_index].U;
		joints[curr_joint.parent_index].children.push_back(id);
		glm::mat4 B = glm::mat4(1.0f);
		B[3] = glm::vec4(curr_joint.position, 1) - glm::vec4(joints[curr_joint.parent_index].position, 0);
		joints[id].init_rel_position = glm::vec3(B[3]);
		return parentU * B;
	}
}

glm::mat4 calculateD(std::vector<Joint> joints, int id)
{
	Joint curr_joint = joints[id];
	if (curr_joint.parent_index == -1) {
		glm::mat4 B = glm::mat4(1.0f);
		B[3] = glm::vec4(curr_joint.position, 1) - glm::vec4(0, 0, 0, 0);
		return B * curr_joint.T;
	}
	else {
		glm::mat4 parentD = joints[curr_joint.parent_index].D;
		glm::mat4 B = glm::mat4(1.0f);
		B[3] = glm::vec4(curr_joint.position, 1) - glm::vec4(joints[curr_joint.parent_index].position, 0);
		return parentD * B * curr_joint.T;
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
		Joint curr_joint = Joint(curr_id, wcoord, parent);
		curr_joint.T = glm::mat4(1.0f);
		curr_joint.children.clear();
		skeleton.joints.push_back(curr_joint);
		skeleton.joints[skeleton.joints.size() - 1].U = calculateU(skeleton.joints, curr_id);
		skeleton.joints[skeleton.joints.size() - 1].D = calculateD(skeleton.joints, curr_id);
		curr_id++;
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


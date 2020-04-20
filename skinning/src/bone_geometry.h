#ifndef BONE_GEOMETRY_H
#define BONE_GEOMETRY_H

#include <ostream>
#include <vector>
#include <string>
#include <map>
#include <limits>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <mmdadapter.h>

#include <glm/gtx/string_cast.hpp>
#include "texture_to_render.h"

class TextureToRender;

struct BoundingBox {
	BoundingBox()
		: min(glm::vec3(-std::numeric_limits<float>::max())),
		max(glm::vec3(std::numeric_limits<float>::max())) {}
	glm::vec3 min;
	glm::vec3 max;
};

struct Joint {
	Joint()
		: joint_index(-1),
		  parent_index(-1),
		  position(glm::vec3(0.0f)),
		  init_position(glm::vec3(0.0f))
	{
	}
	Joint(int id, glm::vec3 wcoord, int parent)
		: joint_index(id),
		  parent_index(parent),
		  position(wcoord),
		  init_position(wcoord),
		  init_rel_position(init_position)
	{
	}

	int joint_index;
	int parent_index;
	glm::vec3 wcoord;	//beginning of joint
	glm::vec3 init_wcoord;
	glm::vec3 position;             // position of the joint - actually the end of the joint
	glm::fquat orientation = glm::fquat(1.0, 0.0, 0.0, 0.0);         // rotation w.r.t. initial configuration
	glm::fquat rel_orientation = glm::fquat(1.0, 0.0, 0.0, 0.0); // rotation w.r.t. it's parent. Used for animation.
	glm::vec3 init_position;        // initial position of this joint
	glm::vec3 init_rel_position;    // initial relative position to its parent
	std::vector<int> children;
	bool is_root;

	glm::mat4 T;
	glm::mat4 D;
	glm::mat4 U;
	glm::mat4 total_roll;
};

struct Configuration {
	std::vector<glm::vec3> trans;
	std::vector<glm::fquat> rot;

	const auto& transData() const { return trans; }
	const auto& rotData() const { return rot; }
};

//struct KeyFrame {
//	std::vector<glm::fquat> rel_rot;
//
//	static void interpolate(const KeyFrame& from,
//	                        const KeyFrame& to,
//	                        float tau,
//	                        KeyFrame& target);
//};

struct LineMesh {
	std::vector<glm::vec4> vertices;
	std::vector<glm::uvec2> indices;
};

struct Skeleton {
	std::vector<Joint> joints;

	Configuration cache;

	void refreshCache(Configuration* cache = nullptr);
	const glm::vec3* collectJointTrans() const;
	const glm::fquat* collectJointRot() const;

	// FIXME: create skeleton and bone data structures
};

struct Keyframe {
	std::vector<glm::mat4> T;
	std::vector<glm::mat4> D;
	std::vector<glm::mat4> U;

	std::vector<glm::fquat> orientation;
	std::vector<glm::fquat> rel_orientation;

	TextureToRender texture;
};

struct Mesh {
	Mesh();
	~Mesh();
	std::vector<glm::vec4> vertices;
	/*
	 * Static per-vertex attrributes for Shaders
	 */
	std::vector<int32_t> joint0;
	std::vector<int32_t> joint1;
	std::vector<float> weight_for_joint0; // weight_for_joint1 can be calculated
	std::vector<glm::vec3> vector_from_joint0;
	std::vector<glm::vec3> vector_from_joint1;
	std::vector<glm::vec4> vertex_normals;
	std::vector<glm::vec4> face_normals;
	std::vector<glm::vec2> uv_coordinates;
	std::vector<glm::uvec3> faces;

	std::vector<Material> materials;
	BoundingBox bounds;
	Skeleton skeleton;

	void loadPmd(const std::string& fn);
	int getNumberOfBones() const;
	glm::vec3 getCenter() const { return 0.5f * glm::vec3(bounds.min + bounds.max); }
	const Configuration* getCurrentQ() const; // Configuration is abbreviated as Q
	void updateAnimation(float t = -1.0);

	void saveAnimationTo(const std::string& fn);
	void loadAnimationFrom(const std::string& fn);

	glm::mat4 calculateU(int id);
	glm::mat4 calculateD(int id);

	void updateAllMatrices();
	void updateAllRotations();

	std::vector<Keyframe*> keyframes;
	void addKeyframe();
	void updateKeyframe(int keyframeid);
	void deleteKeyframe(int keyframeid);
	void setInterpolation(int keyframeid, float percent);
	void setPoseFromKeyframe(int keyframeid);
	void updateAllPositionsAndRotations();
	void setTFromRelOrientation();
	Keyframe* getLastKeyFrame();

	void loadDefaults();

private:
	void computeBounds();
	void computeNormals();
	Configuration currentQ_;
};


#endif

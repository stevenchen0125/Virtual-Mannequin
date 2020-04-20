#include "bone_geometry.h"
#include "texture_to_render.h"
#include <fstream>
#include <iostream>
#include <glm/gtx/io.hpp>
#include <unordered_map>
/*
 * We put these functions to a separated file because the following jumbo
 * header consists of 20K LOC, and it is very slow to compile.
 */
#include "json.hpp"

#include <string>

extern int preview_width;
extern int preview_height;

using json = nlohmann::json;
namespace {
	const glm::fquat identity(1.0, 0.0, 0.0, 0.0);
}

json createQuaternionObject(glm::fquat quat) {
	json output;
	output["x"] = quat.x;
	output["y"] = quat.y;
	output["z"] = quat.z;
	output["w"] = quat.w;
	return output;
}

json createMatrixObject(glm::mat4 mat) {
	json output;
	for (int i = 0; i < 16; i++) {
		output[std::to_string(i)] = mat[i/4][i%4];
	}
	return output;
}

json createJointObject(Keyframe* keyframe, int i) {
	json output;
	output["T"] = createMatrixObject(keyframe->T[i]);
	output["D"] = createMatrixObject(keyframe->D[i]);
	output["U"] = createMatrixObject(keyframe->U[i]);
	output["orientation"] = createQuaternionObject(keyframe->orientation[i]);
	output["rel_orientation"] = createQuaternionObject(keyframe->rel_orientation[i]);
	return output;
}

json createKeyframeObject(Keyframe* keyframe) {
	json output;
	for (int i = 0; i < (int)keyframe->T.size(); i++) {
		output[std::to_string(i)] = createJointObject(keyframe, i);
	}
	return output;
}

void Mesh::saveAnimationTo(const std::string& fn)
{
	// FIXME: Save keyframes to json file.
	std::ofstream jsonfile;
	jsonfile.open(fn);
	json output;
	for (int i = 0; i < (int)keyframes.size(); i++) {
		Keyframe* keyframe = keyframes[i];
		output[std::to_string(i)] = createKeyframeObject(keyframe);
	}
	jsonfile << output.dump(4);
	jsonfile.close();
}

glm::fquat loadQuaternion(json input)
{
	glm::fquat output;
	output.x = input["x"];
	output.y = input["y"];
	output.z = input["z"];
	output.w = input["w"];
	return output;
}

glm::mat4 loadMatrix(json input)
{
	glm::mat4 output = glm::mat4(0.0f);
	for (int i = 0; i < 16; i++) {
		output[i / 4][i % 4] = input[std::to_string(i)];
	}
	return output;
}

Keyframe* loadKeyframe(json input, int preview_width_, int preview_height_)
{
	Keyframe* output = new Keyframe();
	output->T.clear();
	output->U.clear();
	output->D.clear();
	output->rel_orientation.clear();
	output->orientation.clear();
	for (int i = 0; i < input.size(); i++) {
		int j = 0;
		//std::cerr << input[std::to_string(i)] << std::endl;
		json it = input[std::to_string(i)];
		output->T.push_back(loadMatrix(it["T"]));
		output->D.push_back(loadMatrix(it["D"]));
		output->U.push_back(loadMatrix(it["U"]));
		output->orientation.push_back(loadQuaternion(it["orientation"]));
		output->rel_orientation.push_back(loadQuaternion(it["rel_orientation"]));
		/*for (auto it1: it) {
			switch (j)
			{
				case 0: output->T.push_back(loadMatrix(it1));  break;
				case 1: output->D.push_back(loadMatrix(it1)); break;
				case 2: output->U.push_back(loadMatrix(it1)); break;
				case 3: output->orientation.push_back(loadQuaternion(it1));  break;
				case 4: output->rel_orientation.push_back(loadQuaternion(it1)); break;
			}
			j++;
		}*/
		//i++;
	}
	output->texture.create(preview_width_, preview_height_);
	return output;
}

void Mesh::loadAnimationFrom(const std::string& fn)
{
	// FIXME: Load keyframes from json file.
	std::ifstream jsonfile;
	jsonfile.open(fn);
	json input;
	jsonfile >> input;
	//for (json::iterator it = input.begin(); it != input.end(); ++it) {
	//	//std::cerr << it.key() << " : " << it.value() << "\n" << std::endl;
	//	std::cerr << it.key() << std::endl;
	//}
	keyframes.clear();
	for (int i = 0; i < input.size(); i++)
	{
		// "it" is of type json::reference and has no key() member
		/*std::cerr << it << '\n';*/
		keyframes.push_back(loadKeyframe(input[std::to_string(i)], preview_width, preview_height));
	}
	/*if (input.find("0") != input.end()) {
		std::cerr << "Here" << std::endl;
	}*/
	jsonfile.close();
}

#include "Model.h"
#include <filesystem>
#include <algorithm>
#include <limits>

namespace fs = std::filesystem;

Model::Model(const char* file)
{

	std::string text = get_file_contents(file);
	JSON = json::parse(text);


	Model::file = file;
	data = getData();


	traverseNode(0);
}

glm::vec2 Model::GetVerticalBounds(glm::vec3 scale) const
{
	float minimumY = std::numeric_limits<float>::max();
	float maximumY = std::numeric_limits<float>::lowest();
	for (std::size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		glm::mat4 transform = glm::scale(glm::mat4(1.0f), scale) * matricesMeshes[meshIndex];
		for (const Vertex& vertex : meshes[meshIndex].vertices)
		{
			float vertexY = (transform * glm::vec4(vertex.position, 1.0f)).y;
			minimumY = std::min(minimumY, vertexY);
			maximumY = std::max(maximumY, vertexY);
		}
	}
	if (minimumY == std::numeric_limits<float>::max())
		return glm::vec2(0.0f);
	return glm::vec2(minimumY, maximumY);
}

glm::vec3 Model::GetMinimumBounds(glm::vec3 scale) const
{
	glm::vec3 minimum(std::numeric_limits<float>::max());
	for (std::size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		glm::mat4 transform = glm::scale(glm::mat4(1.0f), scale) * matricesMeshes[meshIndex];
		for (const Vertex& vertex : meshes[meshIndex].vertices)
			minimum = glm::min(minimum, glm::vec3(transform * glm::vec4(vertex.position, 1.0f)));
	}
	return minimum;
}

glm::vec3 Model::GetMaximumBounds(glm::vec3 scale) const
{
	glm::vec3 maximum(std::numeric_limits<float>::lowest());
	for (std::size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		glm::mat4 transform = glm::scale(glm::mat4(1.0f), scale) * matricesMeshes[meshIndex];
		for (const Vertex& vertex : meshes[meshIndex].vertices)
			maximum = glm::max(maximum, glm::vec3(transform * glm::vec4(vertex.position, 1.0f)));
	}
	return maximum;
}

std::vector<std::array<glm::vec3, 3>> Model::GetTriangles(glm::vec3 translation, glm::vec3 scale) const
{
	std::vector<std::array<glm::vec3, 3>> triangles;
	for (std::size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		const Mesh& mesh = meshes[meshIndex];
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation) *
			glm::scale(glm::mat4(1.0f), scale) * matricesMeshes[meshIndex];
		for (std::size_t index = 0; index + 2 < mesh.indices.size(); index += 3)
		{
			std::array<glm::vec3, 3> triangle;
			for (int corner = 0; corner < 3; ++corner)
			{
				GLuint vertexIndex = mesh.indices[index + corner];
				if (vertexIndex >= mesh.vertices.size())
					continue;
				triangle[corner] = glm::vec3(transform * glm::vec4(mesh.vertices[vertexIndex].position, 1.0f));
			}
			triangles.push_back(triangle);
		}
	}
	return triangles;
}

void Model::Draw(Shader& shader, Camera& camera, glm::vec3 translation)
{
	shader.Activate();
	glUniform1i(glGetUniformLocation(shader.ID, "useSolidBaseColor"), 0);
	glUniform3f(glGetUniformLocation(shader.ID, "objectTint"), 1.0f, 1.0f, 1.0f);
	for (unsigned int i = 0; i < meshes.size(); i++)
	{

		glm::mat4 worldMatrix = glm::translate(glm::mat4(1.0f), translation) * matricesMeshes[i];
		meshes[i].Mesh::Draw
		(
			shader,
			camera,
			worldMatrix
		);
	}
}

void Model::DrawWithTint(Shader& shader, Camera& camera, glm::vec3 translation, glm::vec3 tint, glm::vec3 scale, glm::mat4 rotation)
{
	shader.Activate();
	glUniform1i(glGetUniformLocation(shader.ID, "useSolidBaseColor"), 0);
	glUniform3f(glGetUniformLocation(shader.ID, "objectTint"), tint.r, tint.g, tint.b);
	for (std::size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		glm::mat4 worldMatrix = glm::translate(glm::mat4(1.0f), translation) * rotation *
			glm::scale(glm::mat4(1.0f), scale) * matricesMeshes[meshIndex];
		meshes[meshIndex].Mesh::Draw(shader, camera, worldMatrix);
	}
	glUniform3f(glGetUniformLocation(shader.ID, "objectTint"), 1.0f, 1.0f, 1.0f);
}

void Model::Draw(Shader& shader, Camera& camera, glm::vec3 translation, glm::vec3 scale)
{
	shader.Activate();
	glUniform1i(glGetUniformLocation(shader.ID, "useSolidBaseColor"), 0);
	glUniform3f(glGetUniformLocation(shader.ID, "objectTint"), 0.2f, 1.0f, 0.2f);
	for (unsigned int i = 0; i < meshes.size(); i++)
	{
		glm::mat4 worldMatrix = glm::translate(glm::mat4(1.0f), translation) *
			glm::scale(glm::mat4(1.0f), scale) * matricesMeshes[i];
		meshes[i].Mesh::Draw(shader, camera, worldMatrix);
	}
}

void Model::Draw(Shader& shader, Camera& camera)
{
	shader.Activate();
	glUniform1i(glGetUniformLocation(shader.ID, "useSolidBaseColor"), 0);
	glUniform3f(glGetUniformLocation(shader.ID, "objectTint"), 1.0f, 1.0f, 1.0f);

	for (unsigned int i = 0; i < meshes.size(); i++)
	{
		meshes[i].Mesh::Draw(shader, camera, matricesMeshes[i]);
	}
}

void Model::DrawWithBaseColor(Shader& shader, Camera& camera, glm::vec3 translation, glm::vec3 baseColor, glm::vec3 scale)
{
	shader.Activate();
	glUniform1i(glGetUniformLocation(shader.ID, "useSolidBaseColor"), 1);
	glUniform3f(glGetUniformLocation(shader.ID, "solidBaseColor"), baseColor.r, baseColor.g, baseColor.b);
	glUniform3f(glGetUniformLocation(shader.ID, "objectTint"), 1.0f, 1.0f, 1.0f);
	for (std::size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		glm::mat4 worldMatrix = glm::translate(glm::mat4(1.0f), translation) *
			glm::scale(glm::mat4(1.0f), scale) * matricesMeshes[meshIndex];
		meshes[meshIndex].Mesh::Draw(shader, camera, worldMatrix);
	}
	glUniform1i(glGetUniformLocation(shader.ID, "useSolidBaseColor"), 0);
}

void Model::loadMesh(unsigned int indMesh)
{

	unsigned int posAccInd = JSON["meshes"][indMesh]["primitives"][0]["attributes"]["POSITION"];
	unsigned int normalAccInd = JSON["meshes"][indMesh]["primitives"][0]["attributes"]["NORMAL"];
	unsigned int texAccInd = JSON["meshes"][indMesh]["primitives"][0]["attributes"]["TEXCOORD_0"];
	unsigned int indAccInd = JSON["meshes"][indMesh]["primitives"][0]["indices"];


	std::vector<float> posVec = getFloats(JSON["accessors"][posAccInd]);
	std::vector<glm::vec3> positions = groupFloatsVec3(posVec);
	std::vector<float> normalVec = getFloats(JSON["accessors"][normalAccInd]);
	std::vector<glm::vec3> normals = groupFloatsVec3(normalVec);
	std::vector<float> texVec = getFloats(JSON["accessors"][texAccInd]);
	std::vector<glm::vec2> texUVs = groupFloatsVec2(texVec);


	std::vector<Vertex> vertices = assembleVertices(positions, normals, texUVs);
	std::vector<GLuint> indices = getIndices(JSON["accessors"][indAccInd]);
	std::vector<Texture> textures = getTextures();


	meshes.push_back(Mesh(vertices, indices, textures));
}

void Model::traverseNode(unsigned int nextNode, glm::mat4 matrix)
{
	json node = JSON["nodes"][nextNode];


	glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f);
	if (node.find("translation") != node.end())
	{
		float transValues[3];
		for (unsigned int i = 0; i < node["translation"].size(); i++)
			transValues[i] = (node["translation"][i]);
		translation = glm::make_vec3(transValues);
	}
	glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	if (node.find("rotation") != node.end())
	{
		float rotValues[4] =
		{
			node["rotation"][3],
			node["rotation"][0],
			node["rotation"][1],
			node["rotation"][2]
		};
		rotation = glm::make_quat(rotValues);
	}
	glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
	if (node.find("scale") != node.end())
	{
		float scaleValues[3];
		for (unsigned int i = 0; i < node["scale"].size(); i++)
			scaleValues[i] = (node["scale"][i]);
		scale = glm::make_vec3(scaleValues);
	}
	glm::mat4 matNode = glm::mat4(1.0f);
	if (node.find("matrix") != node.end())
	{
		float matValues[16];
		for (unsigned int i = 0; i < node["matrix"].size(); i++)
			matValues[i] = (node["matrix"][i]);
		matNode = glm::make_mat4(matValues);
	}


	glm::mat4 trans = glm::mat4(1.0f);
	glm::mat4 rot = glm::mat4(1.0f);
	glm::mat4 sca = glm::mat4(1.0f);


	trans = glm::translate(trans, translation);
	rot = glm::mat4_cast(rotation);
	sca = glm::scale(sca, scale);


	glm::mat4 matNextNode = matrix * matNode * trans * rot * sca;


	if (node.find("mesh") != node.end())
	{
		translationsMeshes.push_back(translation);
		rotationsMeshes.push_back(rotation);
		scalesMeshes.push_back(scale);
		matricesMeshes.push_back(matNextNode);


		loadMesh(node["mesh"]);
	}


	if (node.find("children") != node.end())
	{
		for (unsigned int i = 0; i < node["children"].size(); i++)
			traverseNode(node["children"][i], matNextNode);
	}
}

std::vector<unsigned char> Model::getData()
{

	std::string bytesText;
	std::string uri = JSON["buffers"][0]["uri"];


	fs::path fileDirectory = fs::path(file).parent_path();
	fs::path binaryPath = fileDirectory / fs::path(uri);
	std::string binaryFile = binaryPath.string();
	bytesText = get_file_contents(binaryFile.c_str());


	std::vector<unsigned char> bytes(bytesText.begin(), bytesText.end());
	return bytes;
}

std::vector<float> Model::getFloats(json accessor)
{
	std::vector<float> floatVec;


	unsigned int buffViewInd = accessor.value("bufferView", 1);
	unsigned int count = accessor["count"];
	unsigned int accByteOffset = accessor.value("byteOffset", 0);
	std::string type = accessor["type"];


	json bufferView = JSON["bufferViews"][buffViewInd];
	unsigned int byteOffset = bufferView["byteOffset"];


	unsigned int numPerVert;
	if (type == "SCALAR") numPerVert = 1;
	else if (type == "VEC2") numPerVert = 2;
	else if (type == "VEC3") numPerVert = 3;
	else if (type == "VEC4") numPerVert = 4;
	else throw std::invalid_argument("Type is invalid (not SCALAR, VEC2, VEC3, or VEC4)");


	unsigned int beginningOfData = byteOffset + accByteOffset;
	unsigned int lengthOfData = count * 4 * numPerVert;
	for (unsigned int i = beginningOfData; i < beginningOfData + lengthOfData; i += 4)
	{
		unsigned char bytes[] = { data[i], data[i + 1], data[i + 2], data[i + 3] };
		float value;
		std::memcpy(&value, bytes, sizeof(float));
		floatVec.push_back(value);
	}


	return floatVec;
}

std::vector<GLuint> Model::getIndices(json accessor)
{
	std::vector<GLuint> indices;


	unsigned int buffViewInd = accessor.value("bufferView", 0);
	unsigned int count = accessor["count"];
	unsigned int accByteOffset = accessor.value("byteOffset", 0);
	unsigned int componentType = accessor["componentType"];


	json bufferView = JSON["bufferViews"][buffViewInd];
	unsigned int byteOffset = bufferView["byteOffset"];


	unsigned int beginningOfData = byteOffset + accByteOffset;
	if (componentType == 5125)
	{
		for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 4; i += 4)
		{
			unsigned char bytes[] = { data[i], data[i + 1], data[i + 2], data[i + 3] };
			unsigned int value;
			std::memcpy(&value, bytes, sizeof(unsigned int));
			indices.push_back((GLuint)value);
		}
	}
	else if (componentType == 5123)
	{
		for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i += 2)
		{
			unsigned char bytes[] = { data[i], data[i + 1] };
			unsigned short value;
			std::memcpy(&value, bytes, sizeof(unsigned short));
			indices.push_back((GLuint)value);
		}
	}
	else if (componentType == 5122)
	{
		for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i += 2)
		{
			unsigned char bytes[] = { data[i], data[i + 1] };
			short value;
			std::memcpy(&value, bytes, sizeof(short));
			indices.push_back((GLuint)value);
		}
	}


	return indices;
}

std::vector<Texture> Model::getTextures()
{
	std::vector<Texture> textures;

	fs::path fileDirectory = fs::path(file).parent_path();


	for (unsigned int i = 0; i < JSON["images"].size(); i++)
	{
		std::string texPath = JSON["images"][i]["uri"];
		fs::path texturePath = fileDirectory / fs::path(texPath);


		bool skip = false;
		for (unsigned int j = 0; j < loadedTexName.size(); j++)
		{
			if (loadedTexName[j] == texPath)
			{
				textures.push_back(loadedTex[j]);
				skip = true;
				break;
			}
		}


		if (!skip)
		{
			if (texPath.find("baseColor") != std::string::npos)
			{
				Texture diffuse = Texture(texturePath.string().c_str(), "diffuse", static_cast<GLuint>(loadedTex.size()));
				textures.push_back(diffuse);
				loadedTex.push_back(diffuse);
				loadedTexName.push_back(texPath);
			}
			else if (texPath.find("metallicRoughness") != std::string::npos)
			{
				Texture specular = Texture(texturePath.string().c_str(), "specular", static_cast<GLuint>(loadedTex.size()));
				textures.push_back(specular);
				loadedTex.push_back(specular);
				loadedTexName.push_back(texPath);
			}
			else
			{
				// glTF base-color images are not required to use a "baseColor" filename.
				// Treat otherwise-unclassified images as diffuse so assets such as redorb.png render.
				Texture diffuse = Texture(texturePath.string().c_str(), "diffuse", static_cast<GLuint>(loadedTex.size()));
				textures.push_back(diffuse);
				loadedTex.push_back(diffuse);
				loadedTexName.push_back(texPath);
			}
		}
	}


	return textures;
}

std::vector<Vertex> Model::assembleVertices
(
	std::vector<glm::vec3> positions,
	std::vector<glm::vec3> normals,
	std::vector<glm::vec2> texUVs
)
{
	std::vector<Vertex> vertices;
	for (int i = 0; i < positions.size(); i++)
	{
		vertices.push_back
		(
			Vertex
			{
				positions[i],
				normals[i],
				glm::vec3(1.0f, 1.0f, 1.0f),
				texUVs[i]
			}
		);
	}
	return vertices;
}

std::vector<glm::vec2> Model::groupFloatsVec2(std::vector<float> floatVec)
{
	const unsigned int floatsPerVector = 2;

	std::vector<glm::vec2> vectors;
	for (unsigned int i = 0; i < floatVec.size(); i += floatsPerVector)
	{
		vectors.push_back(glm::vec2(0, 0));


		for (unsigned int j = 0; j < floatsPerVector; j++)
		{
			vectors.back()[j] = floatVec[i + j];
		}
	}
	return vectors;
}
std::vector<glm::vec3> Model::groupFloatsVec3(std::vector<float> floatVec)
{
	const unsigned int floatsPerVector = 3;

	std::vector<glm::vec3> vectors;
	for (unsigned int i = 0; i < floatVec.size(); i += floatsPerVector)
	{
		vectors.push_back(glm::vec3(0, 0, 0));


		for (unsigned int j = 0; j < floatsPerVector; j++)
		{
			vectors.back()[j] = floatVec[i + j];
		}
	}
	return vectors;
}
std::vector<glm::vec4> Model::groupFloatsVec4(std::vector<float> floatVec)
{
	const unsigned int floatsPerVector = 4;

	std::vector<glm::vec4> vectors;
	for (unsigned int i = 0; i < floatVec.size(); i += floatsPerVector)
	{
		vectors.push_back(glm::vec4(0, 0, 0, 0));


		for (unsigned int j = 0; j < floatsPerVector; j++)
		{
			vectors.back()[j] = floatVec[i + j];
		}
	}
	return vectors;
}

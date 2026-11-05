#include<filesystem>
#define NOMINMAX
#include<Windows.h>
#include<mmsystem.h>
#include<cstdint>
#include<cstring>
#pragma comment(lib, "winmm.lib")
namespace fs = std::filesystem;

#define GLM_ENABLE_EXPERIMENTAL
#include"Model.h"
#include<iostream>
#include<array>
#include<algorithm>
#include<cmath>
#include<limits>
#include<random>
#include<string>
#include<vector>
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<stb/stb_image.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>

#include"Texture.h"
#include"shaderClass.h"
#include"VAO.h"
#include"VBO.h"
#include"EBO.h"

const int defaultWidth = 800;
const int defaultHeight = 800;

std::vector<std::uint8_t> makeConcreteJumpWav()
{
	constexpr std::uint32_t sampleRate = 22050;
	constexpr std::uint32_t sampleCount = sampleRate / 5;
	constexpr std::uint32_t dataSize = sampleCount * sizeof(std::int16_t);
	std::vector<std::uint8_t> wav(44 + dataSize, 0);
	auto write16 = [&wav](std::size_t offset, std::uint16_t value)
	{
		wav[offset] = static_cast<std::uint8_t>(value & 0xff);
		wav[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
	};
	auto write32 = [&wav](std::size_t offset, std::uint32_t value)
	{
		wav[offset] = static_cast<std::uint8_t>(value & 0xff);
		wav[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
		wav[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xff);
		wav[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xff);
	};
	std::memcpy(wav.data(), "RIFF", 4);
	write32(4, 36 + dataSize);
	std::memcpy(wav.data() + 8, "WAVEfmt ", 8);
	write32(16, 16);
	write16(20, 1);
	write16(22, 1);
	write32(24, sampleRate);
	write32(28, sampleRate * sizeof(std::int16_t));
	write16(32, sizeof(std::int16_t));
	write16(34, 16);
	std::memcpy(wav.data() + 36, "data", 4);
	write32(40, dataSize);
	std::uint32_t noiseState = 0x13579bdu;
	for (std::uint32_t i = 0; i < sampleCount; ++i)
	{
		float time = static_cast<float>(i) / sampleRate;
		float envelope = std::exp(-time * 24.0f);
		noiseState = noiseState * 1664525u + 1013904223u;
		float noise = static_cast<float>((noiseState >> 8) & 0xffffu) / 32767.5f - 1.0f;
		float thump = std::sin(6.2831853f * 105.0f * time) * std::exp(-time * 30.0f);
		float crack = noise * std::exp(-time * 55.0f);
		float sample = (thump * 0.6f + crack * 0.4f) * envelope * 0.8f;
		std::int16_t pcm = static_cast<std::int16_t>(glm::clamp(sample, -1.0f, 1.0f) * 32767.0f);
		write16(44 + static_cast<std::size_t>(i) * 2, static_cast<std::uint16_t>(pcm));
	}
	return wav;
}

std::vector<std::uint8_t> makeAimTargetHitWav()
{
	constexpr std::uint32_t sampleRate = 22050;
	constexpr std::uint32_t sampleCount = sampleRate / 7;
	constexpr std::uint32_t dataSize = sampleCount * sizeof(std::int16_t);
	std::vector<std::uint8_t> wav(44 + dataSize, 0);
	auto write16 = [&wav](std::size_t offset, std::uint16_t value)
	{
		wav[offset] = static_cast<std::uint8_t>(value & 0xff);
		wav[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
	};
	auto write32 = [&wav](std::size_t offset, std::uint32_t value)
	{
		wav[offset] = static_cast<std::uint8_t>(value & 0xff);
		wav[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
		wav[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xff);
		wav[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xff);
	};
	std::memcpy(wav.data(), "RIFF", 4);
	write32(4, 36 + dataSize);
	std::memcpy(wav.data() + 8, "WAVEfmt ", 8);
	write32(16, 16);
	write16(20, 1);
	write16(22, 1);
	write32(24, sampleRate);
	write32(28, sampleRate * sizeof(std::int16_t));
	write16(32, sizeof(std::int16_t));
	write16(34, 16);
	std::memcpy(wav.data() + 36, "data", 4);
	write32(40, dataSize);
	for (std::uint32_t i = 0; i < sampleCount; ++i)
	{
		float time = static_cast<float>(i) / sampleRate;
		float envelope = std::exp(-time * 20.0f);
		float chime = std::sin(6.2831853f * 880.0f * time) * 0.65f +
			std::sin(6.2831853f * 1320.0f * time) * 0.35f;
		std::int16_t pcm = static_cast<std::int16_t>(glm::clamp(chime * envelope * 0.55f, -1.0f, 1.0f) * 32767.0f);
		write16(44 + static_cast<std::size_t>(i) * 2, static_cast<std::uint16_t>(pcm));
	}
	return wav;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

std::array<const char*, 5> getFpsGlyph(char character)
{
	if (character == 'F') return { "111", "100", "110", "100", "100" };
	if (character == 'P') return { "110", "101", "110", "100", "100" };
	if (character == 'S') return { "011", "100", "010", "001", "110" };
	if (character == 'D') return { "110", "101", "101", "101", "110" };
	if (character == ':') return { "000", "010", "000", "010", "000" };
	if (character == '0') return { "111", "101", "101", "101", "111" };
	if (character == '1') return { "010", "110", "010", "010", "111" };
	if (character == '2') return { "110", "001", "010", "100", "111" };
	if (character == '3') return { "110", "001", "010", "001", "110" };
	if (character == '4') return { "101", "101", "111", "001", "001" };
	if (character == '5') return { "111", "100", "110", "001", "110" };
	if (character == '6') return { "011", "100", "110", "101", "010" };
	if (character == '7') return { "111", "001", "010", "010", "010" };
	if (character == '8') return { "010", "101", "010", "101", "010" };
	if (character == '9') return { "010", "101", "011", "001", "110" };
	return { "000", "000", "000", "000", "000" };
}

std::vector<GLfloat> buildFpsVertices(const std::string& text, int width, int height, float cellSize, float topOffset = 16.0f)
{
	std::vector<GLfloat> vertices;
	if (width <= 0 || height <= 0)
		return vertices;

	for (std::size_t characterIndex = 0; characterIndex < text.size(); ++characterIndex)
	{
		auto glyph = getFpsGlyph(text[characterIndex]);
		for (int row = 0; row < 5; ++row)
		{
			for (int column = 0; column < 3; ++column)
			{
				if (glyph[row][column] != '1')
					continue;

				float pixelX = 16.0f + (static_cast<float>(characterIndex) * 4.0f + column + 0.5f) * cellSize;
				float pixelY = topOffset + (row + 0.5f) * cellSize;
				vertices.push_back(2.0f * pixelX / static_cast<float>(width) - 1.0f);
				vertices.push_back(1.0f - 2.0f * pixelY / static_cast<float>(height));
			}
		}
	}
	return vertices;
}

glm::vec3 closestPointOnTriangle(const glm::vec3& point, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
	glm::vec3 ab = b - a;
	glm::vec3 ac = c - a;
	glm::vec3 ap = point - a;
	float d1 = glm::dot(ab, ap);
	float d2 = glm::dot(ac, ap);
	if (d1 <= 0.0f && d2 <= 0.0f) return a;

	glm::vec3 bp = point - b;
	float d3 = glm::dot(ab, bp);
	float d4 = glm::dot(ac, bp);
	if (d3 >= 0.0f && d4 <= d3) return b;

	float vc = d1 * d4 - d3 * d2;
	if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
		return a + (d1 / (d1 - d3)) * ab;

	glm::vec3 cp = point - c;
	float d5 = glm::dot(ab, cp);
	float d6 = glm::dot(ac, cp);
	if (d6 >= 0.0f && d5 <= d6) return c;

	float vb = d5 * d2 - d1 * d6;
	if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
		return a + (d2 / (d2 - d6)) * ac;

	float va = d3 * d6 - d5 * d4;
	if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
		return b + ((d4 - d3) / ((d4 - d3) + (d5 - d6))) * (c - b);

	float denominator = 1.0f / (va + vb + vc);
	float v = vb * denominator;
	float w = vc * denominator;
	return a + ab * v + ac * w;
}

int main()
{
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* nativeMode = primaryMonitor ? glfwGetVideoMode(primaryMonitor) : nullptr;
	int windowWidth = nativeMode ? nativeMode->width : defaultWidth;
	int windowHeight = nativeMode ? nativeMode->height : defaultHeight;
	GLFWwindow* window = glfwCreateWindow
	(
		windowWidth,
		windowHeight,
		"Aim Trainer",
		nativeMode ? primaryMonitor : nullptr,
		nullptr
	);
	if (window == NULL)
	{
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSwapInterval(0);

	gladLoadGL();
	int framebufferWidth = 0;
	int framebufferHeight = 0;
	glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
	glViewport(0, 0, framebufferWidth, framebufferHeight);
	glEnable(GL_PROGRAM_POINT_SIZE);

	const char* crosshairVertexSource = "#version 330 core\nlayout(location = 0) in vec2 position;\nuniform float aspectRatio;\nuniform float pointSize;\nvoid main() { gl_Position = vec4(position.x / aspectRatio, position.y, 0.0, 1.0); gl_PointSize = pointSize; }";
	const char* crosshairFragmentSource = "#version 330 core\nout vec4 FragColor;\nuniform vec4 crosshairColor;\nvoid main() { FragColor = crosshairColor; }";
	auto compileCrosshairShader = [](GLenum type, const char* source)
	{
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &source, nullptr);
		glCompileShader(shader);
		GLint compiled = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (compiled != GL_TRUE)
		{
			char log[512] = {};
			glGetShaderInfoLog(shader, static_cast<GLsizei>(sizeof(log)), nullptr, log);
			std::cerr << "Crosshair shader compilation failed: " << log << std::endl;
			glDeleteShader(shader);
			return GLuint(0);
		}
		return shader;
	};

	GLuint crosshairVertexShader = compileCrosshairShader(GL_VERTEX_SHADER, crosshairVertexSource);
	GLuint crosshairFragmentShader = compileCrosshairShader(GL_FRAGMENT_SHADER, crosshairFragmentSource);
	GLuint crosshairProgram = glCreateProgram();
	glAttachShader(crosshairProgram, crosshairVertexShader);
	glAttachShader(crosshairProgram, crosshairFragmentShader);
	glLinkProgram(crosshairProgram);
	glDeleteShader(crosshairVertexShader);
	glDeleteShader(crosshairFragmentShader);
	GLint crosshairLinked = GL_FALSE;
	glGetProgramiv(crosshairProgram, GL_LINK_STATUS, &crosshairLinked);
	if (crosshairLinked != GL_TRUE)
	{
		char log[512] = {};
		glGetProgramInfoLog(crosshairProgram, static_cast<GLsizei>(sizeof(log)), nullptr, log);
		std::cerr << "Crosshair shader link failed: " << log << std::endl;
	}
	GLint crosshairAspectLocation = glGetUniformLocation(crosshairProgram, "aspectRatio");
	GLint crosshairColorLocation = glGetUniformLocation(crosshairProgram, "crosshairColor");
	GLint crosshairPointSizeLocation = glGetUniformLocation(crosshairProgram, "pointSize");

	const char* skyboxVertexSource = "#version 330 core\nlayout(location = 0) in vec3 position;\nuniform mat4 skyboxMatrix;\nout vec3 skyDirection;\nvoid main() { skyDirection = position; vec4 clip = skyboxMatrix * vec4(position, 1.0); gl_Position = clip.xyww; }";
	const char* skyboxFragmentSource = R"GLSL(#version 330 core
in vec3 skyDirection;
uniform float dayAmount;
uniform float cloudTime;
out vec4 FragColor;
float hash21(vec2 p)
{
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}
float noise2(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash21(i), hash21(i + vec2(1.0, 0.0)), f.x),
		mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x), f.y);
}
float cloudNoise(vec2 p)
{
	float n = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 4; ++i)
	{
		n += noise2(p) * amplitude;
		p = p * 2.03 + vec2(17.1, 9.2);
		amplitude *= 0.5;
	}
	return n;
}
void main()
{
	vec3 direction = normalize(skyDirection);
	float height = clamp(direction.y * 0.5 + 0.5, 0.0, 1.0);
	vec3 dayHorizon = vec3(0.62, 0.78, 0.94);
	vec3 dayZenith = vec3(0.12, 0.36, 0.68);
	vec3 nightHorizon = vec3(0.10, 0.13, 0.22);
	vec3 nightZenith = vec3(0.012, 0.025, 0.075);
	vec3 horizon = mix(nightHorizon, dayHorizon, dayAmount);
	vec3 zenith = mix(nightZenith, dayZenith, dayAmount);
	vec3 sky = mix(horizon, zenith, smoothstep(0.30, 0.96, height));
	vec2 cloudUv = direction.xz / max(direction.y + 0.28, 0.12);
	cloudUv = cloudUv * 2.1 + vec2(cloudTime * 0.012, cloudTime * 0.004);
	float clouds = smoothstep(0.53, 0.70, cloudNoise(cloudUv));
	float cloudVisibility = smoothstep(-0.04, 0.28, direction.y);
	float cloudAmount = clouds * cloudVisibility * 0.82;
	vec3 cloudColor = mix(vec3(0.24, 0.27, 0.36), vec3(0.98, 0.99, 1.0), dayAmount);
	sky = mix(sky, cloudColor, cloudAmount);
	FragColor = vec4(sky, 1.0);
})GLSL";
	auto compileSkyboxShader = [](GLenum type, const char* source)
	{
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &source, nullptr);
		glCompileShader(shader);
		GLint compiled = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (compiled != GL_TRUE)
		{
			char log[512] = {};
			glGetShaderInfoLog(shader, static_cast<GLsizei>(sizeof(log)), nullptr, log);
			std::cerr << "Skybox shader compilation failed: " << log << std::endl;
		}
		return shader;
	};
	GLuint skyboxVertexShader = compileSkyboxShader(GL_VERTEX_SHADER, skyboxVertexSource);
	GLuint skyboxFragmentShader = compileSkyboxShader(GL_FRAGMENT_SHADER, skyboxFragmentSource);
	GLuint skyboxProgram = glCreateProgram();
	glAttachShader(skyboxProgram, skyboxVertexShader);
	glAttachShader(skyboxProgram, skyboxFragmentShader);
	glLinkProgram(skyboxProgram);
	glDeleteShader(skyboxVertexShader);
	glDeleteShader(skyboxFragmentShader);
	GLint skyboxMatrixLocation = glGetUniformLocation(skyboxProgram, "skyboxMatrix");
	GLint skyboxDayAmountLocation = glGetUniformLocation(skyboxProgram, "dayAmount");
	GLint skyboxCloudTimeLocation = glGetUniformLocation(skyboxProgram, "cloudTime");
	const glm::vec3 skyboxCorners[] =
	{
		{-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},
		{-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}
	};
	const unsigned int skyboxIndices[] =
	{
		0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1,
		5, 4, 7, 7, 6, 5, 4, 0, 3, 3, 7, 4,
		3, 2, 6, 6, 7, 3, 4, 5, 1, 1, 0, 4
	};
	std::array<GLfloat, 108> skyboxVertices{};
	for (std::size_t index = 0; index < std::size(skyboxIndices); ++index)
	{
		const glm::vec3& corner = skyboxCorners[skyboxIndices[index]];
		skyboxVertices[index * 3] = corner.x;
		skyboxVertices[index * 3 + 1] = corner.y;
		skyboxVertices[index * 3 + 2] = corner.z;
	}
	GLuint skyboxVAO = 0;
	GLuint skyboxVBO = 0;
	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(skyboxVertices)), skyboxVertices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	const char* shadowVertexSource = "#version 330 core\nlayout(location = 0) in vec2 position;\nuniform mat4 camMatrix;\nuniform vec2 shadowCenter;\nuniform vec2 shadowRadius;\nuniform float shadowHeight;\nout vec2 shadowCoord;\nvoid main() { shadowCoord = position; vec3 worldPos = vec3(shadowCenter.x + position.x * shadowRadius.x, shadowHeight, shadowCenter.y + position.y * shadowRadius.y); gl_Position = camMatrix * vec4(worldPos, 1.0); }";
	const char* shadowFragmentSource = "#version 330 core\nin vec2 shadowCoord;\nuniform float shadowOpacity;\nout vec4 FragColor;\nvoid main() { float r = length(shadowCoord); float alpha = (1.0 - smoothstep(0.35, 1.0, r)) * shadowOpacity; if (alpha < 0.01) discard; FragColor = vec4(0.015, 0.025, 0.035, alpha); }";
	GLuint shadowVertexShader = compileSkyboxShader(GL_VERTEX_SHADER, shadowVertexSource);
	GLuint shadowFragmentShader = compileSkyboxShader(GL_FRAGMENT_SHADER, shadowFragmentSource);
	GLuint shadowProgram = glCreateProgram();
	glAttachShader(shadowProgram, shadowVertexShader);
	glAttachShader(shadowProgram, shadowFragmentShader);
	glLinkProgram(shadowProgram);
	glDeleteShader(shadowVertexShader);
	glDeleteShader(shadowFragmentShader);
	GLint shadowMatrixLocation = glGetUniformLocation(shadowProgram, "camMatrix");
	GLint shadowCenterLocation = glGetUniformLocation(shadowProgram, "shadowCenter");
	GLint shadowRadiusLocation = glGetUniformLocation(shadowProgram, "shadowRadius");
	GLint shadowHeightLocation = glGetUniformLocation(shadowProgram, "shadowHeight");
	GLint shadowOpacityLocation = glGetUniformLocation(shadowProgram, "shadowOpacity");
	const GLfloat shadowVertices[] = { -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f };
	GLuint shadowVAO = 0;
	GLuint shadowVBO = 0;
	glGenVertexArrays(1, &shadowVAO);
	glGenBuffers(1, &shadowVBO);
	glBindVertexArray(shadowVAO);
	glBindBuffer(GL_ARRAY_BUFFER, shadowVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(shadowVertices), shadowVertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	const GLfloat crosshairVertices[] =
	{
		-0.012f, 0.0f, 0.012f, 0.0f,
		0.0f, -0.012f, 0.0f, 0.012f
	};
	GLuint crosshairVAO = 0;
	GLuint crosshairVBO = 0;
	glGenVertexArrays(1, &crosshairVAO);
	glGenBuffers(1, &crosshairVBO);
	glBindVertexArray(crosshairVAO);
	glBindBuffer(GL_ARRAY_BUFFER, crosshairVBO);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(crosshairVertices)), crosshairVertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	GLuint fpsVAO = 0;
	GLuint fpsVBO = 0;
	glGenVertexArrays(1, &fpsVAO);
	glGenBuffers(1, &fpsVBO);
	glBindVertexArray(fpsVAO);
	glBindBuffer(GL_ARRAY_BUFFER, fpsVBO);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	std::vector<wchar_t> executablePathBuffer(32768);
	DWORD executablePathLength = GetModuleFileNameW(
		nullptr,
		executablePathBuffer.data(),
		static_cast<DWORD>(executablePathBuffer.size())
	);
	fs::path executableDirectory = executablePathLength > 0
		? fs::path(std::wstring(executablePathBuffer.data(), executablePathLength)).parent_path()
		: fs::current_path();

	auto hasRequiredAssets = [](const fs::path& root)
	{
		return fs::exists(root / "default.vert") &&
			fs::exists(root / "default.frag") &&
			fs::exists(root / "Resources" / "models" / "plane" / "plane.gltf") &&
			fs::exists(root / "Resources" / "models" / "orb" / "redorb.gltf");
	};

	fs::path assetRoot = fs::current_path();
	if (!hasRequiredAssets(assetRoot))
	{
		if (hasRequiredAssets(executableDirectory))
			assetRoot = executableDirectory;
		else
		{
			fs::path projectDirectory = executableDirectory.parent_path().parent_path();
			if (hasRequiredAssets(projectDirectory))
				assetRoot = projectDirectory;
			else
			{
				std::cerr << "Game shaders or model resources were not found." << std::endl;
				glDeleteBuffers(1, &fpsVBO);
				glDeleteVertexArrays(1, &fpsVAO);
				glDeleteBuffers(1, &crosshairVBO);
				glDeleteVertexArrays(1, &crosshairVAO);
				glDeleteProgram(crosshairProgram);
				glfwDestroyWindow(window);
				glfwTerminate();
				return -1;
			}
		}
	}

	std::string vertexShaderPath = (assetRoot / "default.vert").string();
	std::string fragmentShaderPath = (assetRoot / "default.frag").string();
	Shader shaderProgram(vertexShaderPath.c_str(), fragmentShaderPath.c_str());

	glm::vec4 lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	glm::vec3 lightPos = glm::vec3(0.5f, 0.5f, 0.5f);
	glm::mat4 lightModel = glm::mat4(1.0f);
	lightModel = glm::translate(lightModel, lightPos);

	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

	glEnable(GL_DEPTH_TEST);
	double dayNightStartTime = glfwGetTime();
	constexpr float dayNightCycleSeconds = 180.0f;

	Camera camera(framebufferWidth, framebufferHeight, glm::vec3(0.0f, 2.5f, 6.0f));
	glfwSetWindowUserPointer(window, &camera);

	fs::path modelPath = assetRoot / "Resources" / "models" / "plane" / "plane.gltf";
	std::string modelFile = modelPath.string();

	if (!fs::exists(modelPath))
	{
		std::cerr << "Model file not found: " << modelPath << std::endl;
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}

	Model model(modelFile.c_str());

	fs::path orbPath = assetRoot / "Resources" / "models" / "orb" / "redorb.gltf";
	std::string orbFile = orbPath.string();
	if (!fs::exists(orbPath))
	{
		std::cerr << "Orb model not found: " << orbPath << std::endl;
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}
	Model orbModel(orbFile.c_str());
	fs::path prismPath = assetRoot / "Resources" / "models" / "prism" / "prism.gltf";
	if (!fs::exists(prismPath))
	{
		std::cerr << "Prism model not found: " << prismPath << std::endl;
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}
	std::string prismFile = prismPath.string();
	Model prismModel(prismFile.c_str());

	struct Ball
	{
		glm::vec3 position;
		glm::vec3 velocity = glm::vec3(0.0f);
		bool grounded = false;
	};

	float platformScale = 3.0f;
	const float platformTop = model.GetVerticalBounds(glm::vec3(platformScale, 1.0f, platformScale)).y;
	glm::vec2 orbVerticalBounds = orbModel.GetVerticalBounds();
	float orbBottomOffset = orbVerticalBounds.x;
	const float gravity = 15.0f;
	const float cameraGravity = 42.0f;
	const float ballMoveSpeed = 5.0f;
	const float jumpSpeed = 17.0f;
	const float cameraGroundY = platformTop + 1.5f;
	const float ballRadius = 1.0f;
	const float cameraColliderRadius = 0.6f;
	const float cameraColliderTopOffset = 0.2f;
	const float cameraColliderBottomOffset = 1.5f;
	const glm::vec3 prismScale(24.0f, 3.0f, 3.0f);
	const float prismOffsetX = 5.0f;
	const float prismSurfaceClearance = 0.25f;
	const glm::vec3 prismLocalMinimum = prismModel.GetMinimumBounds(prismScale);
	const float rightmostBallSpawnX = 3.0f;
	const glm::vec3 prismPosition(
		rightmostBallSpawnX + ballRadius + prismOffsetX - prismLocalMinimum.x,
		platformTop + prismSurfaceClearance - prismLocalMinimum.y,
		0.0f
	);
	const glm::vec3 prismWorldMinimum = prismLocalMinimum + prismPosition;
	const glm::vec3 prismWorldMaximum = prismModel.GetMaximumBounds(prismScale) + prismPosition;
	platformScale = std::max(
		std::max(std::abs(prismWorldMinimum.x), std::abs(prismWorldMaximum.x)) / 24.206254f,
		std::max(std::abs(prismWorldMinimum.z), std::abs(prismWorldMaximum.z)) / 21.531103f
	) + 1.0f;
	const float platformHalfWidth = 24.206254f * platformScale + 0.25f;
	const float platformHalfDepth = 21.531103f * platformScale + 0.25f;
	const std::vector<std::array<glm::vec3, 3>> prismTriangles = prismModel.GetTriangles(prismPosition, prismScale);
	const float ballGroundY = platformTop - orbBottomOffset;
	const float orbSpawnHeight = ballGroundY + 15.0f;
	const int playerBallIndex = 0;
	const float cameraToPlayerBallOffset = cameraGroundY - ballGroundY;
	const float playerBallVisualScale = 1.0f;
	const float playerBallCollisionRadius = ballRadius * playerBallVisualScale;
	const float aimTrainerMinX = -7.0f;
	const float aimTrainerMaxX = 7.0f;
	const float aimTrainerMinZ = -20.0f;
	const float aimTrainerMaxZ = -6.0f;
	std::array<glm::vec3, 3> aimTargets =
	{
		glm::vec3(-4.0f, ballGroundY + 3.0f, -26.0f),
		glm::vec3(0.0f, ballGroundY + 5.0f, -26.0f),
		glm::vec3(4.0f, ballGroundY + 3.0f, -26.0f)
	};
	std::mt19937 aimTargetRandomEngine(std::random_device{}());
	std::uniform_real_distribution<float> aimTargetX(aimTrainerMinX, aimTrainerMaxX);
	std::uniform_real_distribution<float> aimTargetY(ballGroundY + 2.0f, ballGroundY + 8.0f);
	std::uniform_real_distribution<float> aimTargetZ(-30.0f, -23.0f);
	auto respawnAimTarget = [&](std::size_t targetIndex)
	{
		glm::vec3 previousPosition = aimTargets[targetIndex];
		for (int attempt = 0; attempt < 32; ++attempt)
		{
			glm::vec3 candidate(aimTargetX(aimTargetRandomEngine), aimTargetY(aimTargetRandomEngine), aimTargetZ(aimTargetRandomEngine));
			if (glm::length(candidate - previousPosition) < 2.5f)
				continue;

			bool overlapsAnotherTarget = false;
			for (std::size_t otherIndex = 0; otherIndex < aimTargets.size(); ++otherIndex)
			{
				if (otherIndex != targetIndex && glm::length(candidate - aimTargets[otherIndex]) < 2.5f)
				{
					overlapsAnotherTarget = true;
					break;
				}
			}
			if (!overlapsAnotherTarget)
			{
				aimTargets[targetIndex] = candidate;
				return;
			}
		}

		aimTargets[targetIndex] = glm::vec3(aimTargetX(aimTargetRandomEngine), aimTargetY(aimTargetRandomEngine), aimTargetZ(aimTargetRandomEngine));
	};
	glm::mat4 playerBallRotation(1.0f);
	auto keepInsidePlatform = [&](glm::vec3& position, glm::vec3& velocity, float radius)
	{
		float minX = -platformHalfWidth + radius;
		float maxX = platformHalfWidth - radius;
		float minZ = -platformHalfDepth + radius;
		float maxZ = platformHalfDepth - radius;

		if (position.x < minX)
		{
			position.x = minX;
			if (velocity.x < 0.0f) velocity.x = 0.0f;
		}
		else if (position.x > maxX)
		{
			position.x = maxX;
			if (velocity.x > 0.0f) velocity.x = 0.0f;
		}
		if (position.z < minZ)
		{
			position.z = minZ;
			if (velocity.z < 0.0f) velocity.z = 0.0f;
		}
		else if (position.z > maxZ)
		{
			position.z = maxZ;
			if (velocity.z > 0.0f) velocity.z = 0.0f;
		}
	};
	std::array<Ball, 3> balls =
	{
		Ball{ glm::vec3(0.0f, ballGroundY, 6.0f), glm::vec3(0.0f), true },
		Ball{ glm::vec3(0.0f, orbSpawnHeight, 0.0f) },
		Ball{ glm::vec3(3.0f, orbSpawnHeight, 0.0f) }
	};
	camera.Position = balls[playerBallIndex].position + glm::vec3(0.0f, cameraToPlayerBallOffset, 0.0f);
	int activeBall = 0;
	bool previousX = false;
	bool previousZ = false;
	bool previousLeftMouse = false;
	float cameraVerticalVelocity = 0.0f;
	float jumpBufferRemaining = 0.0f;
	auto resolveCameraBallCollisions = [&]()
	{
		for (int collisionPass = 0; collisionPass < 3; ++collisionPass)
		{
			bool resolvedContact = false;
			for (const auto& triangle : prismTriangles)
			{
				glm::vec3 ballCenter = camera.Position - glm::vec3(0.0f, cameraToPlayerBallOffset, 0.0f);
				glm::vec3 closestPoint = closestPointOnTriangle(ballCenter, triangle[0], triangle[1], triangle[2]);
				glm::vec3 collisionOffset = ballCenter - closestPoint;
				float distanceSquared = glm::dot(collisionOffset, collisionOffset);
				if (distanceSquared >= playerBallCollisionRadius * playerBallCollisionRadius)
					continue;

				float distance = std::sqrt(distanceSquared);
				glm::vec3 normal;
				if (distance > 0.0001f)
					normal = collisionOffset / distance;
				else
				{
					normal = glm::normalize(glm::cross(triangle[1] - triangle[0], triangle[2] - triangle[0]));
					glm::vec3 triangleCenter = (triangle[0] + triangle[1] + triangle[2]) / 3.0f;
					if (glm::dot(ballCenter - triangleCenter, normal) < 0.0f)
						normal = -normal;
				}

				camera.Position += normal * (playerBallCollisionRadius - distance);
				glm::vec3 cameraVelocity(camera.horizontalVelocity.x, cameraVerticalVelocity, camera.horizontalVelocity.z);
				float velocityIntoFace = glm::dot(cameraVelocity, normal);
				if (velocityIntoFace < 0.0f)
				{
					glm::vec3 tangentVelocity = cameraVelocity - normal * velocityIntoFace;
					float surfAcceleration = std::min(-velocityIntoFace * 1.75f, 90.0f);
					if (glm::length(tangentVelocity) > 0.0001f)
						tangentVelocity = glm::normalize(tangentVelocity) * (glm::length(tangentVelocity) + surfAcceleration / 120.0f);
					cameraVelocity = tangentVelocity;
					camera.horizontalVelocity.x = cameraVelocity.x;
					cameraVerticalVelocity = cameraVelocity.y;
					camera.horizontalVelocity.z = cameraVelocity.z;
				}
				resolvedContact = true;
			}
			if (!resolvedContact)
				break;
		}

		for (std::size_t ballIndex = 0; ballIndex < balls.size(); ++ballIndex)
		{
			if (ballIndex == playerBallIndex)
				continue;
			const Ball& currentBall = balls[ballIndex];
			glm::vec3 playerBallCenter = camera.Position - glm::vec3(0.0f, cameraToPlayerBallOffset, 0.0f);
			glm::vec3 offset = playerBallCenter - currentBall.position;
			float distanceSquared = glm::dot(offset, offset);
			float minimumDistance = playerBallCollisionRadius + ballRadius;
			if (distanceSquared >= minimumDistance * minimumDistance)
				continue;

			float distance = std::sqrt(distanceSquared);
			glm::vec3 collisionNormal = distance > 0.0001f
				? offset / distance
				: glm::vec3(0.0f, 1.0f, 0.0f);
			camera.Position += collisionNormal * (minimumDistance - distance);

			if (collisionNormal.y > 0.5f && cameraVerticalVelocity < 0.0f)
			{
				cameraVerticalVelocity = jumpSpeed;
			}
			else
			{
				glm::vec2 horizontalNormal(collisionNormal.x, collisionNormal.z);
				float horizontalNormalLength = glm::length(horizontalNormal);
				if (horizontalNormalLength <= 0.0001f)
					continue;
				horizontalNormal /= horizontalNormalLength;
				float velocityIntoBall = glm::dot(
					glm::vec2(camera.horizontalVelocity.x, camera.horizontalVelocity.z),
					horizontalNormal
				);
				if (velocityIntoBall < 0.0f)
				{
					camera.horizontalVelocity.x -= horizontalNormal.x * velocityIntoBall;
					camera.horizontalVelocity.z -= horizontalNormal.y * velocityIntoBall;
				}
			}
		}
	};
	double previousTime = glfwGetTime();
	float physicsAccumulator = 0.0f;
	double fpsWindowStart = previousTime;
	double hudWindowStart = previousTime;
	unsigned int fpsFrameCount = 0;
	int displayedFps = 0;
	glm::vec3 previousCameraPosition = camera.Position;
	std::vector<std::uint8_t> jumpSound = makeConcreteJumpWav();
	std::vector<std::uint8_t> aimTargetHitSound = makeAimTargetHitWav();
	float fpsCellSize = std::max(2.0f, static_cast<float>(framebufferHeight) / 300.0f);
	std::string fpsLabel = "FPS: 0";
	std::vector<GLfloat> fpsVertices = buildFpsVertices(fpsLabel, framebufferWidth, framebufferHeight, fpsCellSize);
	std::string speedLabel = "SPD: 0";
	std::vector<GLfloat> speedVertices = buildFpsVertices(speedLabel, framebufferWidth, framebufferHeight, fpsCellSize, 16.0f + 7.0f * fpsCellSize);
	fpsVertices.insert(fpsVertices.end(), speedVertices.begin(), speedVertices.end());
	glBindBuffer(GL_ARRAY_BUFFER, fpsVBO);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(fpsVertices.size() * sizeof(GLfloat)), fpsVertices.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	while (!glfwWindowShouldClose(window))
	{
		processInput(window);

		double currentTime = glfwGetTime();
		float deltaTime = static_cast<float>(std::min(currentTime - previousTime, 0.05));
		previousTime = currentTime;
		physicsAccumulator += deltaTime;
		++fpsFrameCount;
		bool fpsTextChanged = false;
		if (currentTime - fpsWindowStart >= 1.0)
		{
			displayedFps = static_cast<int>(std::round(fpsFrameCount / (currentTime - fpsWindowStart)));
			fpsFrameCount = 0;
			fpsWindowStart = currentTime;
			fpsLabel = "FPS: " + std::to_string(displayedFps);
			fpsTextChanged = true;
		}
		if (currentTime - hudWindowStart >= 0.1 || fpsTextChanged)
		{
			hudWindowStart = currentTime;
			float horizontalSpeed = glm::length(glm::vec2(camera.horizontalVelocity.x, camera.horizontalVelocity.z));
			speedLabel = "SPD: " + std::to_string(static_cast<int>(std::round(horizontalSpeed)));
			glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
			fpsCellSize = std::max(2.0f, static_cast<float>(framebufferHeight) / 300.0f);
			fpsVertices = buildFpsVertices(fpsLabel, framebufferWidth, framebufferHeight, fpsCellSize);
			speedVertices = buildFpsVertices(speedLabel, framebufferWidth, framebufferHeight, fpsCellSize, 16.0f + 7.0f * fpsCellSize);
			fpsVertices.insert(fpsVertices.end(), speedVertices.begin(), speedVertices.end());
			glBindBuffer(GL_ARRAY_BUFFER, fpsVBO);
			glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(fpsVertices.size() * sizeof(GLfloat)), fpsVertices.data(), GL_DYNAMIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		camera.Inputs(window, true);

		bool cameraJumpPressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
		jumpBufferRemaining = cameraJumpPressed
			? 0.12f
			: jumpBufferRemaining;
		const float physicsStep = 1.0f / 120.0f;
		while (physicsAccumulator >= physicsStep)
		{
			previousCameraPosition = camera.Position;
			float step = physicsStep;
			bool cameraGrounded = camera.Position.y <= cameraGroundY + 0.001f &&
				cameraVerticalVelocity <= 0.0f;
			if (cameraGrounded)
			{
				camera.Position.y = cameraGroundY;
				cameraVerticalVelocity = 0.0f;
			}
			if (jumpBufferRemaining > 0.0f && cameraGrounded)
			{
				cameraVerticalVelocity = jumpSpeed;
				jumpBufferRemaining = 0.0f;
				PlaySoundA(reinterpret_cast<LPCSTR>(jumpSound.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
			}

			cameraVerticalVelocity -= cameraGravity * step;
			camera.Position.y += cameraVerticalVelocity * step;
			if (camera.Position.y < cameraGroundY && cameraVerticalVelocity < 0.0f)
			{
				camera.Position.y = cameraGroundY;
				cameraVerticalVelocity = 0.0f;
			}

			bool cameraOnGround = camera.Position.y <= cameraGroundY + 0.001f &&
				cameraVerticalVelocity <= 0.0f;
			float yawRadians = glm::radians(camera.yaw);
			glm::vec3 cameraForward(std::cos(yawRadians), 0.0f, std::sin(yawRadians));
			glm::vec3 cameraRight(-std::sin(yawRadians), 0.0f, std::cos(yawRadians));
			glm::vec3 wishDirection(0.0f);
			bool forwardPressed = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
			bool backwardPressed = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
			bool leftPressed = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
			bool rightPressed = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
			if (forwardPressed) wishDirection += cameraForward;
			if (backwardPressed) wishDirection -= cameraForward;
			if (leftPressed) wishDirection -= cameraRight;
			if (rightPressed) wishDirection += cameraRight;
			bool hasMovementInput = forwardPressed || backwardPressed || leftPressed || rightPressed;
			if (!cameraOnGround && !hasMovementInput)
				wishDirection = cameraForward;

			glm::vec2 planarVelocity(camera.horizontalVelocity.x, camera.horizontalVelocity.z);
			float planarSpeed = glm::length(planarVelocity);
			float friction = cameraOnGround && !hasMovementInput ? 6.0f : 0.0f;
			if (planarSpeed > 0.0f && friction > 0.0f)
			{
				float newSpeed = std::max(0.0f, planarSpeed - planarSpeed * friction * step);
				planarVelocity *= newSpeed / planarSpeed;
				camera.horizontalVelocity.x = planarVelocity.x;
				camera.horizontalVelocity.z = planarVelocity.y;
			}

			float wishLength = glm::length(wishDirection);
			if (wishLength > 0.0f)
			{
				wishDirection /= wishLength;
				if (cameraOnGround)
				{
					float speedAlongWishDirection = glm::dot(camera.horizontalVelocity, wishDirection);
					float accelerationSpeed = camera.speed - speedAlongWishDirection;
					if (accelerationSpeed > 0.0f)
					{
						float accelerationStep = std::min(30.0f * step * camera.speed, accelerationSpeed);
						camera.horizontalVelocity += wishDirection * accelerationStep;
					}
				}
				else
				{
					glm::vec2 horizontalWish(wishDirection.x, wishDirection.z);
					float horizontalWishLength = glm::length(horizontalWish);
					if (horizontalWishLength > 0.0001f)
						horizontalWish /= horizontalWishLength;
					float airWishSpeed = std::min(camera.speed, 30.0f);
					float speedAlongWishDirection = glm::dot(glm::vec2(camera.horizontalVelocity.x, camera.horizontalVelocity.z), horizontalWish);
					float accelerationSpeed = airWishSpeed - speedAlongWishDirection;
					if (accelerationSpeed > 0.0f)
					{
						float accelerationStep = std::min(36.0f * airWishSpeed * step, accelerationSpeed);
						camera.horizontalVelocity.x += horizontalWish.x * accelerationStep;
						camera.horizontalVelocity.z += horizontalWish.y * accelerationStep;
					}
					if (planarSpeed > airWishSpeed && horizontalWishLength > 0.0001f && !hasMovementInput)
					{
						glm::vec2 currentVelocity(camera.horizontalVelocity.x, camera.horizontalVelocity.z);
						float currentSpeed = glm::length(currentVelocity);
						if (currentSpeed > 0.0001f)
						{
							glm::vec2 velocityDirection = currentVelocity / currentSpeed;
							glm::vec2 lateralWish = horizontalWish - velocityDirection * glm::dot(horizontalWish, velocityDirection);
							currentVelocity += lateralWish * (36.0f * step);
							float steeredSpeed = glm::length(currentVelocity);
							if (steeredSpeed > 0.0001f)
								currentVelocity *= planarSpeed / steeredSpeed;
							camera.horizontalVelocity.x = currentVelocity.x;
							camera.horizontalVelocity.z = currentVelocity.y;
						}
					}
				}
			}

			glm::vec3 positionBeforeMovement = camera.Position;
			camera.Position += camera.horizontalVelocity * step;
			resolveCameraBallCollisions();
			jumpBufferRemaining = std::max(0.0f, jumpBufferRemaining - step);
			physicsAccumulator -= step;
		}

		bool xPressed = glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS;
		if (xPressed && !previousX)
			activeBall = (activeBall + 1) % static_cast<int>(balls.size());
		previousX = xPressed;

		bool zPressed = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;
		if (zPressed && !previousZ)
			activeBall = (activeBall + static_cast<int>(balls.size()) - 1) % static_cast<int>(balls.size());
		previousZ = zPressed;

		for (std::size_t ballIndex = 0; ballIndex < balls.size(); ++ballIndex)
		{
			if (ballIndex == playerBallIndex)
				continue;
			Ball& currentBall = balls[ballIndex];
			currentBall.velocity.y -= gravity * deltaTime;
			currentBall.position += currentBall.velocity * deltaTime;
			keepInsidePlatform(currentBall.position, currentBall.velocity, ballRadius);

			bool overPlatform =
				std::abs(currentBall.position.x) <= platformHalfWidth &&
				std::abs(currentBall.position.z) <= platformHalfDepth;
			if (overPlatform && currentBall.position.y <= ballGroundY)
			{
				currentBall.position.y = ballGroundY;
				currentBall.velocity.y = 0.0f;
				currentBall.grounded = true;
			}
			else
			{
				currentBall.grounded = false;
			}
		}
		balls[playerBallIndex].position = camera.Position - glm::vec3(0.0f, cameraToPlayerBallOffset, 0.0f);
		balls[playerBallIndex].velocity = glm::vec3(
			camera.horizontalVelocity.x,
			cameraVerticalVelocity,
			camera.horizontalVelocity.z
		);
		balls[playerBallIndex].grounded = camera.Position.y <= cameraGroundY + 0.001f && cameraVerticalVelocity <= 0.0f;
		resolveCameraBallCollisions();

		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::vec3 simulationCameraPosition = camera.Position;
		float interpolationAlpha = glm::clamp(physicsAccumulator / physicsStep, 0.0f, 1.0f);
		glm::vec3 interpolatedPlayerEye = glm::mix(previousCameraPosition, simulationCameraPosition, interpolationAlpha);
		glm::vec3 playerBallRenderPosition = interpolatedPlayerEye - glm::vec3(0.0f, cameraToPlayerBallOffset, 0.0f);
		glm::vec3 simulationOrientation = camera.Orientation;
		glm::vec3 cameraForward = glm::normalize(simulationOrientation);
		glm::vec3 cameraRight = glm::normalize(glm::cross(cameraForward, camera.Up));
		bool aimTrainerActive =
			playerBallRenderPosition.x >= aimTrainerMinX && playerBallRenderPosition.x <= aimTrainerMaxX &&
			playerBallRenderPosition.z >= aimTrainerMinZ && playerBallRenderPosition.z <= aimTrainerMaxZ;
		if (aimTrainerActive)
		{
			camera.Position = playerBallRenderPosition + glm::vec3(0.0f, 1.6f, 0.0f);
			camera.Orientation = simulationOrientation;
		}
		else
		{
			camera.Position = playerBallRenderPosition - cameraForward * 5.0f - cameraRight * 0.7f + glm::vec3(0.0f, 1.7f, 0.0f);
			camera.Orientation = glm::normalize(playerBallRenderPosition + glm::vec3(0.0f, 2.0f, 0.0f) - camera.Position);
		}
		camera.updateMatrix(45.0f, 0.1f, 500.0f);

		bool leftMousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
		if (aimTrainerActive && leftMousePressed && !previousLeftMouse)
		{
			glm::vec3 shotDirection = glm::normalize(camera.Orientation);
			float closestHitDistance = std::numeric_limits<float>::max();
			int hitTargetIndex = -1;
			for (std::size_t targetIndex = 0; targetIndex < aimTargets.size(); ++targetIndex)
			{
				glm::vec3 toTarget = aimTargets[targetIndex] - camera.Position;
				float alongRay = glm::dot(toTarget, shotDirection);
				if (alongRay <= 0.0f || alongRay >= closestHitDistance)
					continue;

				float perpendicularDistanceSquared = glm::dot(toTarget, toTarget) - alongRay * alongRay;
				if (perpendicularDistanceSquared <= ballRadius * ballRadius)
				{
					closestHitDistance = alongRay;
					hitTargetIndex = static_cast<int>(targetIndex);
				}
			}
			if (hitTargetIndex >= 0)
			{
				respawnAimTarget(static_cast<std::size_t>(hitTargetIndex));
				PlaySoundA(reinterpret_cast<LPCSTR>(aimTargetHitSound.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
			}
		}
		previousLeftMouse = leftMousePressed;

		glm::mat4 skyboxProjection = glm::perspective(glm::radians(45.0f),
			static_cast<float>(camera.width) / static_cast<float>(camera.height), 0.1f, 500.0f);
		glm::mat4 skyboxView = glm::lookAt(glm::vec3(0.0f), camera.Orientation, camera.Up);
		float dayCyclePhase = static_cast<float>(std::fmod(glfwGetTime() - dayNightStartTime, dayNightCycleSeconds) / dayNightCycleSeconds);
		float sunHeight = std::sin(dayCyclePhase * 6.2831853f - 1.5707963f);
		float dayAmount = glm::smoothstep(-0.18f, 0.22f, sunHeight);
		glm::vec3 sunDirection = glm::normalize(glm::vec3(std::cos(dayCyclePhase * 6.2831853f), sunHeight, 0.25f));
		glm::vec3 sceneLightColor = glm::mix(glm::vec3(0.18f, 0.22f, 0.38f), glm::vec3(1.0f, 0.92f, 0.78f), dayAmount);
		glUseProgram(shaderProgram.ID);
		glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), sceneLightColor.r, sceneLightColor.g, sceneLightColor.b, 1.0f);
		glUniform3f(glGetUniformLocation(shaderProgram.ID, "sunDirection"), sunDirection.x, sunDirection.y, sunDirection.z);
		glUniform3f(glGetUniformLocation(shaderProgram.ID, "ambientColor"), 0.09f + dayAmount * 0.11f, 0.11f + dayAmount * 0.12f, 0.19f + dayAmount * 0.05f);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);
		glUseProgram(skyboxProgram);
		glUniformMatrix4fv(skyboxMatrixLocation, 1, GL_FALSE, glm::value_ptr(skyboxProjection * skyboxView));
		glUniform1f(skyboxDayAmountLocation, dayAmount);
		glUniform1f(skyboxCloudTimeLocation, static_cast<float>(glfwGetTime() - dayNightStartTime));
		glBindVertexArray(skyboxVAO);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);
		glUseProgram(shadowProgram);
		glUniformMatrix4fv(shadowMatrixLocation, 1, GL_FALSE, glm::value_ptr(camera.cameraMatrix));
		glUniform1f(shadowHeightLocation, platformTop + 0.03f);
		glBindVertexArray(shadowVAO);
		auto drawShadow = [&](float centerX, float centerZ, float radiusX, float radiusZ, float opacity)
		{
			glUniform2f(shadowCenterLocation, centerX, centerZ);
			glUniform2f(shadowRadiusLocation, radiusX, radiusZ);
			glUniform1f(shadowOpacityLocation, opacity);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		};
		drawShadow(
			(prismWorldMinimum.x + prismWorldMaximum.x) * 0.5f,
			(prismWorldMinimum.z + prismWorldMaximum.z) * 0.5f,
			(prismWorldMaximum.x - prismWorldMinimum.x) * 0.55f,
			(prismWorldMaximum.z - prismWorldMinimum.z) * 0.55f,
			0.32f);
		for (std::size_t ballIndex = 0; ballIndex < balls.size(); ++ballIndex)
		{
			glm::vec3 ballShadowPosition = balls[ballIndex].position;
			if (ballIndex == playerBallIndex)
				ballShadowPosition = playerBallRenderPosition;
			float heightAboveGround = std::max(0.0f, ballShadowPosition.y - ballGroundY);
			float shadowScale = 1.0f + heightAboveGround * 0.12f;
			float shadowOpacity = 0.38f / (1.0f + heightAboveGround * 0.16f);
			drawShadow(ballShadowPosition.x, ballShadowPosition.z,
				ballRadius * shadowScale, ballRadius * shadowScale, shadowOpacity);
		}
		glBindVertexArray(0);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
		prismModel.DrawWithBaseColor(shaderProgram, camera, prismPosition, glm::vec3(1.0f, 0.0f, 0.0f), prismScale);
		for (std::size_t ballIndex = 0; ballIndex < balls.size(); ++ballIndex)
		{
			glm::vec3 ballRenderPosition = balls[ballIndex].position;
			if (ballIndex == playerBallIndex)
				ballRenderPosition = playerBallRenderPosition;
			glm::vec3 renderScale = ballIndex == playerBallIndex
				? glm::vec3(playerBallVisualScale)
				: glm::vec3(1.0f);
			glm::mat4 renderRotation = ballIndex == playerBallIndex ? playerBallRotation : glm::mat4(1.0f);
			orbModel.DrawWithTint(shaderProgram, camera, ballRenderPosition, glm::vec3(1.0f, 0.12f, 0.12f), renderScale, renderRotation);
		}
		for (const glm::vec3& targetPosition : aimTargets)
			orbModel.DrawWithTint(shaderProgram, camera, targetPosition, glm::vec3(1.0f, 0.12f, 0.12f), glm::vec3(1.0f));
		camera.Position = simulationCameraPosition;
		camera.Orientation = simulationOrientation;

		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glUseProgram(crosshairProgram);
		int crosshairWidth = 0;
		int crosshairHeight = 0;
		glfwGetFramebufferSize(window, &crosshairWidth, &crosshairHeight);
		float crosshairAspect = crosshairHeight > 0
			? static_cast<float>(crosshairWidth) / static_cast<float>(crosshairHeight)
			: 1.0f;
		glUniform1f(crosshairAspectLocation, crosshairAspect);
		glBindVertexArray(crosshairVAO);
		glUniform4f(crosshairColorLocation, 0.0f, 1.0f, 1.0f, 1.0f);
		glLineWidth(1.5f);
		glDrawArrays(GL_LINES, 0, 4);
		glBindVertexArray(0);
		glUniform1f(crosshairAspectLocation, 1.0f);
		glUniform1f(crosshairPointSizeLocation, fpsCellSize + 2.0f);
		glUniform4f(crosshairColorLocation, 0.0f, 0.0f, 0.0f, 0.85f);
		glBindVertexArray(fpsVAO);
		glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(fpsVertices.size() / 2));
		glUniform1f(crosshairPointSizeLocation, fpsCellSize);
		glUniform4f(crosshairColorLocation, 1.0f, 1.0f, 1.0f, 1.0f);
		glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(fpsVertices.size() / 2));
		glBindVertexArray(0);
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);

		glfwSwapBuffers(window);
		glfwPollEvents();
	};

	shaderProgram.Delete();
	glDeleteBuffers(1, &shadowVBO);
	glDeleteVertexArrays(1, &shadowVAO);
	glDeleteProgram(shadowProgram);
	glDeleteBuffers(1, &skyboxVBO);
	glDeleteVertexArrays(1, &skyboxVAO);
	glDeleteProgram(skyboxProgram);
	glDeleteBuffers(1, &fpsVBO);
	glDeleteVertexArrays(1, &fpsVAO);
	glDeleteBuffers(1, &crosshairVBO);
	glDeleteVertexArrays(1, &crosshairVAO);
	glDeleteProgram(crosshairProgram);
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
};

void processInput(GLFWwindow *window)
{
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	static bool escapeWasPressed = false;
	static int windowedX = 100;
	static int windowedY = 100;
	static int windowedWidth = defaultWidth;
	static int windowedHeight = defaultHeight;

	bool escapePressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
	if (escapePressed && !escapeWasPressed)
	{
		if (glfwGetWindowMonitor(window) != nullptr)
		{
			glfwSetWindowMonitor(
				window,
				nullptr,
				windowedX,
				windowedY,
				windowedWidth,
				windowedHeight,
				0
			);
		}
		else
		{
			glfwGetWindowPos(window, &windowedX, &windowedY);
			glfwGetWindowSize(window, &windowedWidth, &windowedHeight);

			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
			if (mode != nullptr)
			{
				glfwSetWindowMonitor(
					window,
					monitor,
					0,
					0,
					mode->width,
					mode->height,
					mode->refreshRate
				);
			}
		}
	}
	escapeWasPressed = escapePressed;
};

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
	if (width > 0 && height > 0)
	{
		Camera* camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
		if (camera != nullptr)
		{
			camera->width = width;
			camera->height = height;
		}
	}
};
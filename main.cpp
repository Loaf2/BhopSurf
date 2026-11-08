#include<filesystem>
#include<fstream>
#include<algorithm>
#include<cmath>
#include<cstdlib>
#include<cstdint>
#include<cstring>
#include<array>
#include<mutex>
#include<vector>
#include<unordered_map>
#ifndef _WIN32
#ifndef AIMTRAINER_ANDROID
#include<curl/curl.h>
#endif
#include<ft2build.h>
#include FT_FREETYPE_H
#define MINIAUDIO_IMPLEMENTATION
#include<miniaudio.h>
#ifdef __APPLE__
#include<mach-o/dyld.h>
#else
#include<unistd.h>
#endif
#endif
#include<thread>
#define NOMINMAX
#ifdef _WIN32
#include<Windows.h>
#include<winhttp.h>
#include<mmsystem.h>
#endif
#ifdef _WIN32
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "winhttp.lib")
#endif
namespace fs = std::filesystem;

#ifdef _WIN32
struct NativeWindowIcon
{
	HICON handle = nullptr;
	~NativeWindowIcon() { if (handle) DestroyIcon(handle); }
};
#endif

#ifndef _WIN32
struct PortableAudioState
{
	ma_engine engine{};
	ma_decoder jumpDecoder{};
	ma_decoder hitDecoder{};
	ma_sound jumpSound{};
	ma_sound hitSound{};
	bool initialized = false;
	bool jumpInitialized = false;
	bool hitInitialized = false;
	bool jumpSoundInitialized = false;
	bool hitSoundInitialized = false;

	~PortableAudioState()
	{
		if (hitSoundInitialized) ma_sound_uninit(&hitSound);
		if (jumpSoundInitialized) ma_sound_uninit(&jumpSound);
		if (hitInitialized) ma_decoder_uninit(&hitDecoder);
		if (jumpInitialized) ma_decoder_uninit(&jumpDecoder);
		if (initialized) ma_engine_uninit(&engine);
	}
};

PortableAudioState portableAudio;

bool initializePortableAudio(const std::vector<std::uint8_t>& jumpWav, const std::vector<std::uint8_t>& hitWav)
{
	ma_engine_config engineConfig = ma_engine_config_init();
	if (ma_engine_init(&engineConfig, &portableAudio.engine) != MA_SUCCESS)
		return false;
	portableAudio.initialized = true;
ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32,
	ma_engine_get_channels(&portableAudio.engine), ma_engine_get_sample_rate(&portableAudio.engine));
	if (ma_decoder_init_memory(jumpWav.data(), jumpWav.size(), &decoderConfig, &portableAudio.jumpDecoder) != MA_SUCCESS)
		return false;
	portableAudio.jumpInitialized = true;
	if (ma_sound_init_from_data_source(&portableAudio.engine, &portableAudio.jumpDecoder,
		MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &portableAudio.jumpSound) != MA_SUCCESS)
		return false;
	portableAudio.jumpSoundInitialized = true;
	if (ma_decoder_init_memory(hitWav.data(), hitWav.size(), &decoderConfig, &portableAudio.hitDecoder) != MA_SUCCESS)
		return false;
	portableAudio.hitInitialized = true;
	if (ma_sound_init_from_data_source(&portableAudio.engine, &portableAudio.hitDecoder,
		MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &portableAudio.hitSound) != MA_SUCCESS)
		return false;
	portableAudio.hitSoundInitialized = true;
	return true;
}

void playPortableSound(bool hitSound)
{
	if (!portableAudio.initialized)
		return;
	ma_sound& sound = hitSound ? portableAudio.hitSound : portableAudio.jumpSound;
	if ((hitSound && !portableAudio.hitSoundInitialized) || (!hitSound && !portableAudio.jumpSoundInitialized))
		return;
	ma_sound_seek_to_pcm_frame(&sound, 0);
	ma_sound_start(&sound);
}

void setPortableAudioVolume(float volume)
{
	if (portableAudio.initialized)
		ma_engine_set_volume(&portableAudio.engine, std::clamp(volume, 0.0f, 1.0f));
}
#endif

fs::path getApplicationDataDirectory()
{
#ifdef _WIN32
	char* localAppData = nullptr;
	size_t localAppDataLength = 0;
	if (_dupenv_s(&localAppData, &localAppDataLength, "LOCALAPPDATA") == 0 && localAppData && *localAppData)
	{
		fs::path applicationDataDirectory = fs::path(localAppData) / "AimTrainer";
		free(localAppData);
		return applicationDataDirectory;
	}
	free(localAppData);
#elif defined(__APPLE__)
	if (const char* path = std::getenv("HOME"); path && *path)
		return fs::path(path) / "Library" / "Application Support" / "AimTrainer";
#else
	if (const char* path = std::getenv("XDG_DATA_HOME"); path && *path)
		return fs::path(path) / "AimTrainer";
	if (const char* path = std::getenv("HOME"); path && *path)
		return fs::path(path) / ".local" / "share" / "AimTrainer";
#endif
	return fs::temp_directory_path() / "AimTrainer";
}

#ifdef _WIN32
bool extractEmbeddedResource(HMODULE module, int resourceId, const fs::path& destination)
{
	HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), MAKEINTRESOURCEW(10));
	if (resource == nullptr)
		return false;

	HGLOBAL loadedResource = LoadResource(module, resource);
	if (loadedResource == nullptr)
		return false;
	const void* resourceBytes = LockResource(loadedResource);
	DWORD resourceSize = SizeofResource(module, resource);
	if (resourceBytes == nullptr || resourceSize == 0)
		return false;

	std::error_code error;
	fs::create_directories(destination.parent_path(), error);
	if (error)
		return false;

	std::ofstream output(destination, std::ios::binary | std::ios::trunc);
	if (!output)
		return false;
	output.write(static_cast<const char*>(resourceBytes), static_cast<std::streamsize>(resourceSize));
	return output.good();
}
#endif

struct TemporaryAssetDirectory
{
	fs::path path;
	~TemporaryAssetDirectory()
	{
		if (!path.empty())
		{
			std::error_code error;
			fs::remove_all(path, error);
		}
	}
};

fs::path getAimHighScorePath()
{
	return getApplicationDataDirectory() / "rank_highscore_v2.txt";
}

fs::path getTwoBallsHighScorePath()
{
	return getApplicationDataDirectory() / "two_balls_highscore.txt";
}

fs::path getSixBallsHighScorePath()
{
	return getApplicationDataDirectory() / "six_balls_highscore.txt";
}

fs::path getMouseSensitivityPath()
{
	return getApplicationDataDirectory() / "sensitivity.txt";
}

fs::path getAudioVolumePath()
{
	return getApplicationDataDirectory() / "audio_volume.txt";
}

fs::path getAimTargetSizePath()
{
	return getApplicationDataDirectory() / "target_size.txt";
}

fs::path getSessionTokenPath()
{
	return getApplicationDataDirectory() / "session_token.txt";
}

fs::path getActivationMarkerPath()
{
	return getApplicationDataDirectory() / "activation.key";
}

fs::path getPlayerNamePath()
{
	return getApplicationDataDirectory() / "player_name.txt";
}

std::string loadSessionToken(const fs::path& tokenPath)
{
	std::ifstream input(tokenPath);
	std::string token;
	std::getline(input, token);
	if (token.size() > 512)
		return {};
	return token;
}

void saveSessionToken(const fs::path& tokenPath, const std::string& token)
{
	std::error_code error;
	fs::create_directories(tokenPath.parent_path(), error);
	if (error)
		return;

	std::ofstream output(tokenPath, std::ios::trunc);
	if (output)
		output << token;
}

void deleteSessionToken(const fs::path& tokenPath)
{
	std::error_code error;
	fs::remove(tokenPath, error);
}

float loadMouseSensitivity(const fs::path& sensitivityPath)
{
	std::ifstream input(sensitivityPath);
	float sensitivity = 0.1f;
	if (input >> sensitivity && sensitivity >= 0.02f && sensitivity <= 0.30f)
		return sensitivity;
	return 0.1f;
}

void saveMouseSensitivity(const fs::path& sensitivityPath, float sensitivity)
{
	std::error_code error;
	fs::create_directories(sensitivityPath.parent_path(), error);
	if (error)
		return;

	std::ofstream output(sensitivityPath, std::ios::trunc);
	if (output)
	{
		output.precision(8);
		output << sensitivity;
	}
}

float loadAimTargetSize(const fs::path& sizePath)
{
	std::ifstream input(sizePath);
	float size = 1.0f;
	if (input >> size && size >= 0.25f && size <= 1.0f)
		return size;
	return 1.0f;
}

void saveAimTargetSize(const fs::path& sizePath, float size)
{
	std::error_code error;
	fs::create_directories(sizePath.parent_path(), error);
	if (error)
		return;
	std::ofstream output(sizePath, std::ios::trunc);
	if (output)
	{
		output.precision(8);
		output << size;
	}
}

float loadAudioVolume(const fs::path& volumePath)
{
	std::ifstream input(volumePath);
	float volume = 1.0f;
	if (input >> volume && volume >= 0.0f && volume <= 1.0f)
		return volume;
	return 1.0f;
}

void saveAudioVolume(const fs::path& volumePath, float volume)
{
	std::error_code error;
	fs::create_directories(volumePath.parent_path(), error);
	if (error)
		return;
	std::ofstream output(volumePath, std::ios::trunc);
	if (output)
	{
		output.precision(8);
		output << volume;
	}
}

void setWavVolume(const std::vector<std::uint8_t>& source, std::vector<std::uint8_t>& destination, float volume)
{
	destination = source;
	volume = std::clamp(volume, 0.0f, 1.0f);
	for (std::size_t offset = 44; offset + 1 < source.size(); offset += 2)
	{
		std::uint16_t encodedSample = static_cast<std::uint16_t>(source[offset]) |
			(static_cast<std::uint16_t>(source[offset + 1]) << 8);
		std::int16_t sample = static_cast<std::int16_t>(encodedSample);
		std::int16_t scaledSample = static_cast<std::int16_t>(std::round(static_cast<float>(sample) * volume));
		std::uint16_t scaledEncoded = static_cast<std::uint16_t>(scaledSample);
		destination[offset] = static_cast<std::uint8_t>(scaledEncoded & 0xff);
		destination[offset + 1] = static_cast<std::uint8_t>((scaledEncoded >> 8) & 0xff);
	}
}

int loadAimHighScore(const fs::path& scorePath)
{
	std::ifstream input(scorePath);
	int score = 0;
	if (input >> score)
		return score < 0 ? 0 : score;
	return 0;
}

void saveAimHighScore(const fs::path& scorePath, int score)
{
	std::error_code error;
	fs::create_directories(scorePath.parent_path(), error);
	if (error)
		return;

	std::ofstream output(scorePath, std::ios::trunc);
	if (output)
		output << score;
}

#define GLM_ENABLE_EXPERIMENTAL
#include"Model.h"
#include<iostream>
#include<memory>
#include<array>
#include<limits>
#include<random>
#include<string>
#include<string_view>
#include<system_error>
#include<thread>
#ifdef __ANDROID__
#include<glad/gles2.h>
#else
#include<glad/glad.h>
#endif
#include<stb/stb_image.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>

#include"Texture.h"
#include"shaderClass.h"
#include"VAO.h"
#include"VBO.h"
#include"EBO.h"
#include"auth_api_config.h"
#include"website_api_config.h"

#ifdef __ANDROID__
#define AIMTRAINER_GLSL_VERSION "300 es"
#else
#define AIMTRAINER_GLSL_VERSION "330 core"
#endif

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

void framebuffer_size_callback(int width, int height);
void processInput();
Camera* cameraForFramebufferCallback = nullptr;

std::array<const char*, 5> getFpsGlyph(char character)
{
	if (character == 'a') return { "000", "010", "101", "111", "101" };
	if (character == 'b') return { "100", "100", "110", "101", "110" };
	if (character == 'c') return { "000", "011", "100", "100", "011" };
	if (character == 'd') return { "001", "001", "011", "101", "011" };
	if (character == 'e') return { "000", "010", "101", "110", "011" };
	if (character == 'f') return { "011", "010", "111", "010", "010" };
	if (character == 'g') return { "000", "011", "101", "011", "001" };
	if (character == 'h') return { "100", "100", "110", "101", "101" };
	if (character == 'i') return { "010", "000", "110", "010", "111" };
	if (character == 'j') return { "001", "000", "001", "101", "010" };
	if (character == 'k') return { "100", "100", "101", "110", "101" };
	if (character == 'l') return { "110", "010", "010", "010", "111" };
	if (character == 'm') return { "000", "110", "111", "101", "101" };
	if (character == 'n') return { "000", "110", "101", "101", "101" };
	if (character == 'o') return { "000", "010", "101", "101", "010" };
	if (character == 'p') return { "000", "110", "101", "110", "100" };
	if (character == 'q') return { "000", "011", "101", "011", "001" };
	if (character == 'r') return { "000", "101", "110", "100", "100" };
	if (character == 's') return { "000", "011", "100", "010", "110" };
	if (character == 't') return { "010", "111", "010", "010", "011" };
	if (character == 'u') return { "000", "101", "101", "101", "011" };
	if (character == 'v') return { "000", "101", "101", "101", "010" };
	if (character == 'w') return { "000", "101", "101", "111", "101" };
	if (character == 'x') return { "000", "101", "010", "010", "101" };
	if (character == 'y') return { "000", "101", "011", "001", "110" };
	if (character == 'z') return { "000", "111", "001", "010", "111" };
	if (character == 'F') return { "111", "100", "110", "100", "100" };
	if (character == 'G') return { "011", "100", "101", "101", "011" };
	if (character == 'P') return { "110", "101", "110", "100", "100" };
	if (character == 'S') return { "011", "100", "010", "001", "110" };
	if (character == 'D') return { "110", "101", "101", "101", "110" };
	if (character == 'A') return { "010", "101", "111", "101", "101" };
	if (character == 'C') return { "011", "100", "100", "100", "011" };
	if (character == 'I') return { "111", "010", "010", "010", "111" };
	if (character == 'J') return { "001", "001", "001", "101", "010" };
	if (character == 'K') return { "101", "101", "110", "101", "101" };
	if (character == 'L') return { "100", "100", "100", "100", "111" };
	if (character == 'M') return { "101", "111", "111", "101", "101" };
	if (character == 'N') return { "101", "111", "111", "111", "101" };
	if (character == 'O') return { "010", "101", "101", "101", "010" };
	if (character == 'R') return { "110", "101", "110", "101", "101" };
	if (character == 'V') return { "101", "101", "101", "101", "010" };
	if (character == 'W') return { "101", "101", "111", "111", "101" };
	if (character == 'X') return { "101", "101", "010", "101", "101" };
	if (character == 'Y') return { "101", "101", "010", "010", "010" };
	if (character == 'Z') return { "111", "001", "010", "100", "111" };
	if (character == 'U') return { "101", "101", "101", "101", "111" };
	if (character == 'Q') return { "010", "101", "101", "011", "001" };
	if (character == 'E') return { "111", "100", "110", "100", "111" };
	if (character == 'T') return { "111", "010", "010", "010", "010" };
	if (character == 'H') return { "101", "101", "111", "101", "101" };
	if (character == 'B') return { "110", "101", "110", "101", "110" };
	if (character == '@') return { "111", "101", "111", "100", "011" };
	if (character == '.') return { "000", "000", "000", "000", "010" };
	if (character == '_') return { "000", "000", "000", "000", "111" };
	if (character == '-') return { "000", "000", "111", "000", "000" };
	if (character == '+') return { "000", "010", "111", "010", "000" };
	if (character == '*') return { "000", "101", "010", "101", "000" };
	if (character == ':') return { "000", "010", "000", "010", "000" };
	if (character == '>') return { "100", "010", "001", "010", "100" };
	if (character == '/') return { "001", "001", "010", "100", "100" };
	if (character == '!') return { "010", "010", "010", "000", "010" };
	if (character == '"') return { "101", "101", "000", "000", "000" };
	if (character == '#') return { "101", "111", "101", "111", "101" };
	if (character == '$') return { "011", "110", "010", "011", "110" };
	if (character == '%') return { "101", "001", "010", "100", "101" };
	if (character == '&') return { "010", "101", "010", "101", "011" };
	if (character == '\'') return { "010", "010", "000", "000", "000" };
	if (character == '(') return { "001", "010", "010", "010", "001" };
	if (character == ')') return { "100", "010", "010", "010", "100" };
	if (character == ',') return { "000", "000", "000", "010", "100" };
	if (character == ';') return { "000", "010", "000", "010", "100" };
	if (character == '<') return { "001", "010", "100", "010", "001" };
	if (character == '=') return { "000", "111", "000", "111", "000" };
	if (character == '?') return { "110", "001", "010", "000", "010" };
	if (character == '[') return { "011", "010", "010", "010", "011" };
	if (character == '\\') return { "100", "100", "010", "001", "001" };
	if (character == ']') return { "110", "010", "010", "010", "110" };
	if (character == '^') return { "010", "101", "000", "000", "000" };
	if (character == '`') return { "100", "010", "000", "000", "000" };
	if (character == '{') return { "001", "010", "110", "010", "001" };
	if (character == '|') return { "010", "010", "010", "010", "010" };
	if (character == '}') return { "100", "010", "011", "010", "100" };
	if (character == '~') return { "000", "010", "101", "000", "000" };
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
	if (character == ' ') return { "000", "000", "000", "000", "000" };
	return { "110", "001", "010", "000", "010" };
}

enum class SignUpField
{
	Email,
	Password,
	ConfirmPassword,
	ActivationKey
};

struct SignUpFormState
{
	std::string email;
	std::string password;
	std::string confirmPassword;
	std::string status;
	SignUpField focusedField = SignUpField::Email;
	bool signInMode = false;
	bool active = true;
	bool submitRequested = false;
	bool submitting = false;
	bool complete = false;
	bool showPassword = false;
	bool emailConfirmationPending = false;
	bool activationMode = false;
	std::string activationKey;
};

SignUpFormState signUpForm;

struct NameEditorState
{
	std::string text;
	bool active = false;
	bool submitRequested = false;
};

NameEditorState nameEditor;

std::string& getFocusedSignUpText()
{
	if (signUpForm.activationMode)
		return signUpForm.activationKey;
	if (signUpForm.focusedField == SignUpField::Password)
		return signUpForm.password;
	if (signUpForm.focusedField == SignUpField::ConfirmPassword)
		return signUpForm.confirmPassword;
	return signUpForm.email;
}

void signUpCharCallback(unsigned int codepoint)
{
	if (nameEditor.active)
	{
		if (codepoint >= 0x20 && codepoint <= 0x7e && nameEditor.text.size() < 32)
			nameEditor.text.push_back(static_cast<char>(codepoint));
		return;
	}
	if (!signUpForm.active || signUpForm.submitting || codepoint < 0x20 || codepoint > 0x7e)
		return;

	std::string& input = getFocusedSignUpText();
	std::size_t maximumLength = signUpForm.focusedField == SignUpField::Email ? 254 : 128;
	if (input.size() >= maximumLength)
		return;

	char character = static_cast<char>(codepoint);
	if (signUpForm.activationMode)
	{
		if ((character >= 'a' && character <= 'z'))
			character = static_cast<char>(character - ('a' - 'A'));
		if (!((character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == '-'))
			return;
		if (input.size() >= 43)
			return;
		input.push_back(character);
		return;
	}
	if (signUpForm.focusedField == SignUpField::Email)
	{
		bool validEmailCharacter = (character >= 'a' && character <= 'z') ||
			(character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') ||
			character == '.' || character == '_' || character == '%' || character == '+' ||
			character == '-' || character == '@';
		if (!validEmailCharacter)
			return;
	}
	input.push_back(character);
}

void signUpKeyCallback(platform::Key key, platform::KeyAction action)
{
	if (nameEditor.active)
	{
		if (action == platform::KeyAction::Press || action == platform::KeyAction::Repeat)
		{
			if (key == platform::Key::Backspace && !nameEditor.text.empty())
				nameEditor.text.pop_back();
			else if (key == platform::Key::Enter)
			{
				nameEditor.active = false;
				nameEditor.submitRequested = true;
			}
		}
		return;
	}
	if (!signUpForm.active || signUpForm.submitting || (action != platform::KeyAction::Press && action != platform::KeyAction::Repeat))
		return;
	const bool controlDown = platform::isKeyDown(platform::Key::Control);
	if (signUpForm.activationMode && action == platform::KeyAction::Press && key == platform::Key::V && controlDown)
	{
		const char* clipboardText = platform::getClipboardString();
		if (clipboardText != nullptr)
		{
			for (const char* character = clipboardText; *character != '\0' && signUpForm.activationKey.size() < 43; ++character)
			{
				char pastedCharacter = *character;
				if (pastedCharacter >= 'a' && pastedCharacter <= 'z')
					pastedCharacter = static_cast<char>(pastedCharacter - ('a' - 'A'));
				if ((pastedCharacter >= 'A' && pastedCharacter <= 'Z') ||
					(pastedCharacter >= '0' && pastedCharacter <= '9') || pastedCharacter == '-')
					signUpForm.activationKey.push_back(pastedCharacter);
			}
		}
		return;
	}

	if (key == platform::Key::Backspace)
	{
		std::string& input = getFocusedSignUpText();
		if (controlDown)
			input.clear();
		else if (!input.empty())
			input.pop_back();
	}
	else if (action == platform::KeyAction::Press && key == platform::Key::Tab)
	{
		bool backwards = platform::isKeyDown(platform::Key::LeftShift) || platform::isKeyDown(platform::Key::RightShift);
		int field = static_cast<int>(signUpForm.focusedField);
		int fieldCount = signUpForm.activationMode ? 1 : (signUpForm.signInMode ? 2 : 3);
		signUpForm.focusedField = static_cast<SignUpField>((field + (backwards ? fieldCount - 1 : 1)) % fieldCount);
	}
	else if (action == platform::KeyAction::Press && key == platform::Key::Enter)
	{
		signUpForm.submitRequested = true;
	}
}

struct SignUpResult
{
	bool authenticated = false;
	bool activationAccepted = false;
	bool confirmationPending = false;
	bool ranksSynchronized = false;
	std::string message;
	std::string sessionToken;
	int aimBestScore = 0;
	int twoBallsBestScore = 0;
	int sixBallsBestScore = 0;
};

std::mutex signUpResultMutex;
SignUpResult signUpResult;
bool signUpResultReady = false;
std::thread signUpAuthThread;

#if !defined(_WIN32) && !defined(AIMTRAINER_ANDROID)
size_t appendCurlResponse(char* data, size_t size, size_t count, void* context)
{
	size_t bytes = size * count;
	auto* response = static_cast<std::string*>(context);
	if (response->size() + bytes > 1024 * 1024)
		return 0;
	response->append(data, bytes);
	return bytes;
}
#endif

bool isAuthApiConfigured()
{
	std::string_view baseUrl = auth_api_config::baseUrl;
	bool secureUrl = baseUrl.starts_with("https://");
	bool localDevelopmentUrl = baseUrl.starts_with("http://localhost") || baseUrl.starts_with("http://127.0.0.1");
	return (secureUrl || localDevelopmentUrl) && baseUrl.find("YOUR-API-HOST") == std::string_view::npos;
}

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& value)
{
	if (value.empty())
		return {};
	int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
	if (length <= 0)
		return {};
	std::wstring converted(static_cast<std::size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), converted.data(), length);
	return converted;
}
#endif

struct ApiResponse
{
#ifdef _WIN32
	DWORD statusCode = 0;
#else
	long statusCode = 0;
#endif
	bool received = false;
	std::string body;
};

ApiResponse sendApiRequest(const std::string& method, const std::string& path, const std::string& body,
	const std::string& bearerToken = {}, std::string_view baseUrlValue = auth_api_config::baseUrl)
{
	ApiResponse response;
#ifdef _WIN32
	std::wstring baseUrl = utf8ToWide(std::string(baseUrlValue));
	URL_COMPONENTS urlComponents{};
	urlComponents.dwStructSize = sizeof(urlComponents);
	urlComponents.dwHostNameLength = static_cast<DWORD>(-1);
	urlComponents.dwUrlPathLength = static_cast<DWORD>(-1);
	if (baseUrl.empty() || !WinHttpCrackUrl(baseUrl.c_str(), 0, 0, &urlComponents) ||
		(urlComponents.nScheme != INTERNET_SCHEME_HTTPS && urlComponents.nScheme != INTERNET_SCHEME_HTTP) ||
		urlComponents.dwHostNameLength == 0)
		return response;

	std::wstring host(urlComponents.lpszHostName, urlComponents.dwHostNameLength);
	bool useHttps = urlComponents.nScheme == INTERNET_SCHEME_HTTPS;
	bool localHost = host == L"localhost" || host == L"127.0.0.1" || host == L"::1";
	if (!useHttps && !localHost)
		return response;
	std::wstring requestPath = urlComponents.dwUrlPathLength > 0
		? std::wstring(urlComponents.lpszUrlPath, urlComponents.dwUrlPathLength)
		: L"";
	while (!requestPath.empty() && requestPath.back() == L'/')
		requestPath.pop_back();
	requestPath += utf8ToWide(path);

	HINTERNET session = WinHttpOpen(L"BhopSurf/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (session == nullptr)
		return response;
	WinHttpSetTimeouts(session, 5000, 5000, 5000, 10000);
	HINTERNET connection = WinHttpConnect(session, host.c_str(), urlComponents.nPort, 0);
	std::wstring wideMethod = utf8ToWide(method);
	HINTERNET request = connection != nullptr
		? WinHttpOpenRequest(connection, wideMethod.c_str(), requestPath.c_str(), nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, useHttps ? WINHTTP_FLAG_SECURE : 0)
		: nullptr;
	if (request == nullptr)
	{
		if (connection != nullptr) WinHttpCloseHandle(connection);
		WinHttpCloseHandle(session);
		return response;
	}

	std::wstring headers = L"Content-Type: application/json\r\n";
	if (!bearerToken.empty())
		headers += L"Authorization: Bearer " + utf8ToWide(bearerToken) + L"\r\n";
	BOOL sent = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(headers.size()),
		body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
		static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
	response.received = sent && WinHttpReceiveResponse(request, nullptr);
	DWORD statusCodeSize = sizeof(response.statusCode);
	if (response.received)
		WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &response.statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
	if (response.received)
	{
		DWORD available = 0;
		while (WinHttpQueryDataAvailable(request, &available) && available > 0 && response.body.size() < 1024 * 1024)
		{
			std::vector<char> buffer(std::min<DWORD>(available, 16384));
			DWORD bytesRead = 0;
			if (!WinHttpReadData(request, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead) || bytesRead == 0)
				break;
			response.body.append(buffer.data(), bytesRead);
		}
	}
	WinHttpCloseHandle(request);
	WinHttpCloseHandle(connection);
	WinHttpCloseHandle(session);
	return response;
#elif defined(AIMTRAINER_ANDROID)
	return response;
#else
	CURL* request = curl_easy_init();
	if (!request)
		return response;
	std::string url = std::string(baseUrlValue);
	while (!url.empty() && url.back() == '/')
		url.pop_back();
	url += path;
	struct curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
	if (headers == nullptr)
	{
		curl_easy_cleanup(request);
		return response;
	}
	std::string authorization;
	if (!bearerToken.empty())
	{
		authorization = "Authorization: Bearer " + bearerToken;
		struct curl_slist* updatedHeaders = curl_slist_append(headers, authorization.c_str());
		if (updatedHeaders == nullptr)
		{
			curl_slist_free_all(headers);
			curl_easy_cleanup(request);
			return response;
		}
		headers = updatedHeaders;
	}
	curl_easy_setopt(request, CURLOPT_URL, url.c_str());
	curl_easy_setopt(request, CURLOPT_CUSTOMREQUEST, method.c_str());
	curl_easy_setopt(request, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(request, CURLOPT_TIMEOUT, 15L);
	curl_easy_setopt(request, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, appendCurlResponse);
	curl_easy_setopt(request, CURLOPT_WRITEDATA, &response.body);
	if (!body.empty())
		curl_easy_setopt(request, CURLOPT_POSTFIELDS, body.c_str());
	response.received = curl_easy_perform(request) == CURLE_OK;
	if (response.received)
		curl_easy_getinfo(request, CURLINFO_RESPONSE_CODE, &response.statusCode);
	curl_slist_free_all(headers);
	curl_easy_cleanup(request);
	return response;
#endif
}

std::string getJsonStringValue(const std::string& json, const std::string& key)
{
	std::string marker = "\"" + key + "\":\"";
	std::size_t valueStart = json.find(marker);
	if (valueStart == std::string::npos)
		return {};
	valueStart += marker.size();
	std::size_t valueEnd = json.find('"', valueStart);
	return valueEnd == std::string::npos ? std::string() : json.substr(valueStart, valueEnd - valueStart);
}

bool getJsonIntValue(const std::string& json, const std::string& key, int& value)
{
	std::string marker = "\"" + key + "\":";
	std::size_t valueStart = json.find(marker);
	if (valueStart == std::string::npos)
		return false;
	valueStart += marker.size();
	try
	{
		std::size_t parsedLength = 0;
		int parsedValue = std::stoi(json.substr(valueStart), &parsedLength);
		if (parsedLength == 0 || parsedValue < 0)
			return false;
		value = parsedValue;
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

bool sendRankScores(const std::string& sessionToken, int& aimBestScore, int& twoBallsBestScore, int& sixBallsBestScore)
{
	std::string body = "{\"aimBestScore\":" + std::to_string(aimBestScore) +
		",\"twoBallsBestScore\":" + std::to_string(twoBallsBestScore) +
		",\"sixBallsBestScore\":" + std::to_string(sixBallsBestScore) + "}";
	ApiResponse response = sendApiRequest("PUT", "/api/ranks", body, sessionToken);
	return response.received && response.statusCode == 200 &&
		getJsonIntValue(response.body, "aimBestScore", aimBestScore) &&
		getJsonIntValue(response.body, "twoBallsBestScore", twoBallsBestScore) &&
		getJsonIntValue(response.body, "sixBallsBestScore", sixBallsBestScore);
}

bool loadRankScores(const std::string& sessionToken, int& aimBestScore, int& twoBallsBestScore, int& sixBallsBestScore, std::string& accountName)
{
	ApiResponse response = sendApiRequest("GET", "/api/ranks", {}, sessionToken);
	accountName = getJsonStringValue(response.body, "accountName");
	return response.received && response.statusCode == 200 &&
		getJsonIntValue(response.body, "aimBestScore", aimBestScore) &&
		getJsonIntValue(response.body, "twoBallsBestScore", twoBallsBestScore) &&
		getJsonIntValue(response.body, "sixBallsBestScore", sixBallsBestScore);
}

std::string escapeJsonString(const std::string& value)
{
	std::string escaped;
	escaped.reserve(value.size());
	for (unsigned char character : value)
	{
		if (character == '"' || character == '\\')
		{
			escaped.push_back('\\');
			escaped.push_back(static_cast<char>(character));
		}
		else if (character >= 0x20)
			escaped.push_back(static_cast<char>(character));
	}
	return escaped;
}

SignUpResult sendAuthApiRequest(const std::string& email, const std::string& password, bool signIn)
{
	SignUpResult result;
	if (!isAuthApiConfigured())
	{
		result.message = "SET API URL";
		return result;
	}

	std::string body = "{\"email\":\"" + escapeJsonString(email) + "\"";
	body += ",\"password\":\"" + escapeJsonString(password) + "\"";
	body += "}";
	std::string requestPath = signIn ? "/api/auth/login" : "/api/auth/register";
	ApiResponse response = sendApiRequest("POST", requestPath, body);
	if (!response.received)
	{
		std::string_view baseUrl = auth_api_config::baseUrl;
		result.message = baseUrl.starts_with("http://localhost") || baseUrl.starts_with("http://127.0.0.1")
			? "START LOCAL API" : "NETWORK ERROR";
		return result;
	}
	constexpr const char* confirmationSuccessMessage = "EMAIL SENT CHECK INBOX";
	if (response.statusCode >= 200 && response.statusCode < 300)
	{
		if (response.statusCode == 202)
		{
			result.confirmationPending = true;
			result.message = confirmationSuccessMessage;
		}
		else
		{
			result.authenticated = true;
			result.sessionToken = getJsonStringValue(response.body, "sessionToken");
			if (signIn && result.sessionToken.empty())
			{
				result.authenticated = false;
				result.message = "LOGIN RESPONSE ERROR";
			}
		}
		return result;
	}
	if (response.statusCode == 409)
		result.message = "EMAIL ALREADY USED";
	else if (response.statusCode == 400 || response.statusCode == 422)
		result.message = "CHECK EMAIL AND PASSWORD";
	else if (response.statusCode == 401)
		result.message = "INVALID LOGIN";
	else if (response.statusCode == 403)
		result.message = "EMAIL NOT CONFIRMED";
	else if (response.statusCode == 503 && !signIn)
	{
		result.confirmationPending = true;
		result.message = "EMAIL SEND FAILED";
	}
	else if (response.statusCode == 429)
		result.message = "RATE LIMIT 429";
	else if (response.statusCode >= 500)
		result.message = "SERVER ERROR";
	else
		result.message = "HTTP " + std::to_string(response.statusCode);
	return result;
}

bool isWebsiteApiConfigured()
{
	std::string_view baseUrl = website_api_config::baseUrl;
	return baseUrl.starts_with("https://") && baseUrl.find("YOUR-WEBSITE-HOST") == std::string_view::npos;
}

SignUpResult sendActivationApiRequest(const std::string& email, const std::string& activationKey, const std::string& sessionToken)
{
	SignUpResult result;
	if (!isWebsiteApiConfigured())
	{
		result.message = "SET WEBSITE URL";
		return result;
	}
	std::string body = "{\"email\":\"" + escapeJsonString(email) +
		"\",\"activationKey\":\"" + escapeJsonString(activationKey) + "\"}";
	ApiResponse response = sendApiRequest("POST", "/api/game/activate", body, sessionToken, website_api_config::baseUrl);
	if (response.received && response.statusCode == 200)
		result.activationAccepted = true;
	else if (response.statusCode == 403)
		result.message = "INVALID ACTIVATION KEY";
	else if (response.statusCode == 400)
		result.message = "INVALID KEY FORMAT";
	else if (!response.received)
		result.message = "NETWORK ERROR";
	else
		result.message = "ACTIVATION SERVER ERROR";
	return result;
}

std::vector<GLfloat> buildFpsVertices(const std::string& text, int width, int height, float cellSize, float topOffset = 16.0f, float leftOffset = 16.0f, float characterAdvance = 4.0f)
{
	std::vector<GLfloat> vertices;
	if (width <= 0 || height <= 0)
		return vertices;

	static std::unordered_map<std::string, std::vector<glm::vec2>> renderedTextCache;
	int fontHeight = std::max(1, static_cast<int>(std::lround(cellSize * 6.5f)));
	int renderPadding = std::max(2, static_cast<int>(std::ceil(cellSize * 2.0f)));
	int bitmapHeight = fontHeight + renderPadding * 2;
	int textWidth = std::max(1, static_cast<int>(std::ceil(
		(text.empty() ? 0.0f : (static_cast<float>(text.size() - 1) * characterAdvance + 3.0f) * cellSize) +
		cellSize + renderPadding * 2.0f)));
	std::string cacheKey = text + '\0' + std::to_string(fontHeight) + ':' + std::to_string(textWidth) + ':' +
		std::to_string(bitmapHeight) + ':' + std::to_string(characterAdvance);
	auto cachedText = renderedTextCache.find(cacheKey);
	if (cachedText == renderedTextCache.end())
	{
		std::vector<glm::vec2> pixels;
		if (!text.empty())
		{
#ifdef _WIN32
			BITMAPINFO bitmapInfo{};
			bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bitmapInfo.bmiHeader.biWidth = textWidth;
			bitmapInfo.bmiHeader.biHeight = -bitmapHeight;
			bitmapInfo.bmiHeader.biPlanes = 1;
			bitmapInfo.bmiHeader.biBitCount = 32;
			bitmapInfo.bmiHeader.biCompression = BI_RGB;
			void* bitmapPixels = nullptr;
			HDC deviceContext = CreateCompatibleDC(nullptr);
			HBITMAP bitmap = deviceContext ? CreateDIBSection(deviceContext, &bitmapInfo, DIB_RGB_COLORS, &bitmapPixels, nullptr, 0) : nullptr;
			HFONT font = CreateFontW(-fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
				OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
			if (deviceContext && bitmap && bitmapPixels && font)
			{
				std::memset(bitmapPixels, 0, static_cast<std::size_t>(textWidth) * bitmapHeight * 4);
				HGDIOBJ previousBitmap = SelectObject(deviceContext, bitmap);
				HGDIOBJ previousFont = SelectObject(deviceContext, font);
				SetBkMode(deviceContext, TRANSPARENT);
				SetTextColor(deviceContext, RGB(255, 255, 255));
				SIZE naturalTextSize{};
				GetTextExtentPoint32A(deviceContext, text.data(), static_cast<int>(text.size()), &naturalTextSize);
				int targetAdvance = static_cast<int>(std::lround(characterAdvance * cellSize));
				int characterExtra = text.size() > 1
					? (targetAdvance * static_cast<int>(text.size() - 1) + static_cast<int>(std::lround(3.0f * cellSize)) - naturalTextSize.cx) / static_cast<int>(text.size() - 1)
					: 0;
				SetTextCharacterExtra(deviceContext, characterExtra);
				TextOutA(deviceContext, renderPadding, renderPadding, text.data(), static_cast<int>(text.size()));
				const auto* rgba = static_cast<const std::uint8_t*>(bitmapPixels);
				for (int y = 0; y < bitmapHeight; ++y)
				{
					for (int x = 0; x < textWidth; ++x)
					{
						std::size_t pixelOffset = (static_cast<std::size_t>(y) * textWidth + x) * 4;
						if (rgba[pixelOffset] > 48 || rgba[pixelOffset + 1] > 48 || rgba[pixelOffset + 2] > 48)
							pixels.emplace_back(static_cast<float>(x - renderPadding), static_cast<float>(y - renderPadding));
					}
				}
				SelectObject(deviceContext, previousFont);
				SelectObject(deviceContext, previousBitmap);
			}
			if (font)
				DeleteObject(font);
			if (bitmap)
				DeleteObject(bitmap);
			if (deviceContext)
				DeleteDC(deviceContext);
#else
			static FT_Library fontLibrary = nullptr;
			static FT_Face fontFace = nullptr;
			static bool fontLibraryInitialized = false;
			if (!fontLibraryInitialized && FT_Init_FreeType(&fontLibrary) == 0)
			{
				fontLibraryInitialized = true;
				const std::array<const char*, 9> fontPaths = {
					"/System/Library/Fonts/Supplemental/Arial.ttf",
					"/System/Library/Fonts/Supplemental/Helvetica.ttf",
					"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
					"/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
					"/usr/share/fonts/truetype/freefont/FreeSans.ttf",
					"/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
					"/System/Library/Fonts/SFNS.ttf",
					"/system/fonts/Roboto-Regular.ttf",
					"/system/fonts/DroidSans.ttf"
				};
				for (const char* fontPath : fontPaths)
				{
					if (FT_New_Face(fontLibrary, fontPath, 0, &fontFace) == 0)
						break;
				}
			}
			if (fontFace && FT_Set_Pixel_Sizes(fontFace, 0, static_cast<FT_UInt>(fontHeight)) == 0)
			{
				int targetAdvance = static_cast<int>(std::lround(characterAdvance * cellSize));
				int baseline = renderPadding + static_cast<int>(fontFace->size->metrics.ascender >> 6);
				for (std::size_t characterIndex = 0; characterIndex < text.size(); ++characterIndex)
				{
					FT_ULong codepoint = static_cast<unsigned char>(text[characterIndex]);
					if (FT_Load_Char(fontFace, codepoint, FT_LOAD_RENDER) != 0)
						continue;
					const FT_Bitmap& bitmap = fontFace->glyph->bitmap;
					int glyphLeft = renderPadding + static_cast<int>(characterIndex) * targetAdvance + fontFace->glyph->bitmap_left;
					int glyphTop = baseline - fontFace->glyph->bitmap_top;
					for (unsigned int y = 0; y < bitmap.rows; ++y)
					{
						for (unsigned int x = 0; x < bitmap.width; ++x)
						{
							int pitch = bitmap.pitch;
							const unsigned char* row = pitch >= 0
								? bitmap.buffer + static_cast<std::size_t>(y) * pitch
								: bitmap.buffer + static_cast<std::size_t>(bitmap.rows - 1 - y) * -pitch;
							if (row[x] > 48)
								pixels.emplace_back(static_cast<float>(glyphLeft + static_cast<int>(x) - renderPadding),
									static_cast<float>(glyphTop + static_cast<int>(y) - renderPadding));
						}
					}
				}
			}
#endif
		}
		cachedText = renderedTextCache.emplace(std::move(cacheKey), std::move(pixels)).first;
	}
	float desiredWidth = text.empty() ? 0.0f :
		(static_cast<float>(text.size() - 1) * characterAdvance + 3.0f) * cellSize;
	float actualMinX = 0.0f;
	float actualMaxX = 0.0f;
	if (!cachedText->second.empty())
	{
		actualMinX = cachedText->second.front().x;
		actualMaxX = actualMinX;
		for (const glm::vec2& pixel : cachedText->second)
		{
			actualMinX = std::min(actualMinX, pixel.x);
			actualMaxX = std::max(actualMaxX, pixel.x);
		}
	}
	float horizontalAdjustment = (desiredWidth - (actualMaxX - actualMinX)) * 0.5f - actualMinX;
	for (const glm::vec2& pixel : cachedText->second)
	{
		float pixelX = leftOffset + pixel.x + horizontalAdjustment;
		float pixelY = topOffset + pixel.y;
		vertices.push_back(2.0f * pixelX / static_cast<float>(width) - 1.0f);
		vertices.push_back(1.0f - 2.0f * pixelY / static_cast<float>(height));
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

int runApplication();

#ifdef __ANDROID__
#include <android_native_app_glue.h>

namespace platform
{
void setAndroidApp(android_app* app);
}

extern "C" void android_main(android_app* app)
{
	platform::setAndroidApp(app);
	runApplication();
}
#else
int main()
{
	return runApplication();
}
#endif

int runApplication()
{
	int windowWidth = defaultWidth;
	int windowHeight = defaultHeight;
	if (!platform::createWindow(windowWidth, windowHeight))
	{
		std::cerr << "Failed to create window" << std::endl;
		return -1;
	}
	platform::setTextCallback(signUpCharCallback);
	platform::setKeyCallback(signUpKeyCallback);
	platform::setFramebufferCallback(framebuffer_size_callback);

#ifdef _WIN32
	gladLoadGL();
#else
	if (!gladLoadGLES2(reinterpret_cast<GLADloadfunc>(platform::getProcAddress)))
	{
		std::cerr << "Failed to load OpenGL functions" << std::endl;
		platform::destroyWindow();
		return -1;
	}
#endif
	int framebufferWidth = 0;
	int framebufferHeight = 0;
	platform::getFramebufferSize(framebufferWidth, framebufferHeight);
	glViewport(0, 0, framebufferWidth, framebufferHeight);
	glEnable(GL_PROGRAM_POINT_SIZE);

	const char* crosshairVertexSource = "#version " AIMTRAINER_GLSL_VERSION "\nlayout(location = 0) in vec2 position;\nuniform float aspectRatio;\nuniform float pointSize;\nvoid main() { gl_Position = vec4(position.x / aspectRatio, position.y, 0.0, 1.0); gl_PointSize = pointSize; }";
	const char* crosshairFragmentSource = "#version " AIMTRAINER_GLSL_VERSION "\nprecision mediump float;\nout vec4 FragColor;\nuniform vec4 crosshairColor;\nvoid main() { FragColor = crosshairColor; }";
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

	const char* skyboxVertexSource = "#version " AIMTRAINER_GLSL_VERSION "\nlayout(location = 0) in vec3 position;\nuniform mat4 skyboxMatrix;\nout vec3 skyDirection;\nvoid main() { skyDirection = position; vec4 clip = skyboxMatrix * vec4(position, 1.0); gl_Position = clip.xyww; }";
	const char* skyboxFragmentSource = "#version " AIMTRAINER_GLSL_VERSION "\nprecision mediump float;\n" R"GLSL(
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

	const char* shadowVertexSource = "#version " AIMTRAINER_GLSL_VERSION "\nlayout(location = 0) in vec2 position;\nuniform mat4 camMatrix;\nuniform vec2 shadowCenter;\nuniform vec2 shadowRadius;\nuniform float shadowHeight;\nout vec2 shadowCoord;\nvoid main() { shadowCoord = position; vec3 worldPos = vec3(shadowCenter.x + position.x * shadowRadius.x, shadowHeight, shadowCenter.y + position.y * shadowRadius.y); gl_Position = camMatrix * vec4(worldPos, 1.0); }";
	const char* shadowFragmentSource = "#version " AIMTRAINER_GLSL_VERSION "\nprecision mediump float;\nin vec2 shadowCoord;\nuniform float shadowOpacity;\nout vec4 FragColor;\nvoid main() { float r = length(shadowCoord); float alpha = (1.0 - smoothstep(0.35, 1.0, r)) * shadowOpacity; if (alpha < 0.01) discard; FragColor = vec4(0.015, 0.025, 0.035, alpha); }";
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

	std::vector<GLfloat> crosshairVertices = {
		-0.012f, 0.0f, 0.012f, 0.0f,
		0.0f, -0.012f, 0.0f, 0.012f,
		0.0f, 0.0f
	};
	for (int i = 0; i < 24; ++i)
	{
		const float angle = 2.0f * 3.14159265f * static_cast<float>(i) / 24.0f;
		crosshairVertices.push_back(0.010f * std::cos(angle));
		crosshairVertices.push_back(0.010f * std::sin(angle));
	}
	crosshairVertices.insert(crosshairVertices.end(), {
		-0.012f, 0.0f, -0.004f, 0.0f,
		0.004f, 0.0f, 0.012f, 0.0f,
		0.0f, -0.012f, 0.0f, -0.004f,
		0.0f, 0.004f, 0.0f, 0.012f
	});
	GLuint crosshairVAO = 0;
	GLuint crosshairVBO = 0;
	glGenVertexArrays(1, &crosshairVAO);
	glGenBuffers(1, &crosshairVBO);
	glBindVertexArray(crosshairVAO);
	glBindBuffer(GL_ARRAY_BUFFER, crosshairVBO);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(crosshairVertices.size() * sizeof(GLfloat)), crosshairVertices.data(), GL_STATIC_DRAW);
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

	fs::path executablePath;
#ifdef _WIN32
	std::vector<wchar_t> executablePathBuffer(32768);
	DWORD executablePathLength = GetModuleFileNameW(nullptr, executablePathBuffer.data(), static_cast<DWORD>(executablePathBuffer.size()));
	if (executablePathLength > 0)
		executablePath = fs::path(std::wstring(executablePathBuffer.data(), executablePathLength));
#elif defined(__APPLE__)
	uint32_t executablePathLength = 0;
	_NSGetExecutablePath(nullptr, &executablePathLength);
	std::vector<char> executablePathBuffer(executablePathLength + 1);
	if (_NSGetExecutablePath(executablePathBuffer.data(), &executablePathLength) == 0)
		executablePath = fs::weakly_canonical(executablePathBuffer.data());
#else
	std::vector<char> executablePathBuffer(4096);
	ssize_t executablePathLength = readlink("/proc/self/exe", executablePathBuffer.data(), executablePathBuffer.size() - 1);
	if (executablePathLength > 0)
	{
		executablePathBuffer[static_cast<std::size_t>(executablePathLength)] = '\0';
		executablePath = executablePathBuffer.data();
	}
#endif
	fs::path executableDirectory = executablePath.empty() ? fs::current_path() : executablePath.parent_path();

	auto hasRequiredAssets = [](const fs::path& root)
	{
		return fs::exists(root / "default.vert") &&
			fs::exists(root / "default.frag") &&
			fs::exists(root / "Resources" / "models" / "plane" / "plane.gltf") &&
			fs::exists(root / "Resources" / "models" / "orb" / "redorb.gltf");
	};

#ifdef _WIN32
	struct EmbeddedAssetFile
	{
		int resourceId;
		const wchar_t* relativePath;
	};
	const std::array<EmbeddedAssetFile, 10> embeddedAssetFiles =
	{{
		{ 101, L"default.vert" },
		{ 102, L"default.frag" },
		{ 201, L"Resources/models/plane/plane.gltf" },
		{ 202, L"Resources/models/plane/plane.bin" },
		{ 203, L"Resources/models/plane/baseColor.png" },
		{ 301, L"Resources/models/orb/redorb.gltf" },
		{ 302, L"Resources/models/orb/redorb.bin" },
		{ 303, L"Resources/models/orb/basecolor.png" },
		{ 401, L"Resources/models/prism/prism.gltf" },
		{ 402, L"Resources/models/prism/prism.bin" }
	}};

	#endif
	fs::path assetRoot;
	TemporaryAssetDirectory temporaryAssetDirectory;
	#ifdef _WIN32
	std::error_code tempDirectoryError;
	fs::path tempAssetParent = fs::temp_directory_path(tempDirectoryError);
	bool embeddedAssetsExtracted = !tempDirectoryError;
	if (embeddedAssetsExtracted)
	{
		assetRoot = tempAssetParent / "AimTrainer" / std::to_string(GetCurrentProcessId());
		temporaryAssetDirectory.path = assetRoot;
		HMODULE applicationModule = GetModuleHandleW(nullptr);
		for (const EmbeddedAssetFile& asset : embeddedAssetFiles)
		{
			if (!extractEmbeddedResource(applicationModule, asset.resourceId, assetRoot / asset.relativePath))
			{
				embeddedAssetsExtracted = false;
				break;
			}
		}
	}
	if (!embeddedAssetsExtracted)
		assetRoot = fs::current_path();
	#elif defined(__ANDROID__)
	assetRoot = platform::extractAndroidAssets();
	if (assetRoot.empty())
	{
		return -1;
	}
#else
	assetRoot = executableDirectory;
#endif

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
				std::error_code executableDirectoryError;
				fs::path installDirectory = fs::weakly_canonical(executableDirectory, executableDirectoryError);
				if (executableDirectoryError)
					installDirectory = executableDirectory;
				const std::array<fs::path, 3> installCandidates = {
					installDirectory / ".." / "share" / "AimTrainer",
					installDirectory / ".." / "Resources",
					installDirectory / "Resources"
				};
				bool foundInstalledAssets = false;
				for (const fs::path& candidate : installCandidates)
				{
					if (hasRequiredAssets(candidate))
					{
						assetRoot = candidate;
						foundInstalledAssets = true;
						break;
					}
				}
				if (!foundInstalledAssets)
				{
					std::cerr << "Game shaders or model resources were not found." << std::endl;
					glDeleteBuffers(1, &fpsVBO);
					glDeleteVertexArrays(1, &fpsVAO);
					glDeleteBuffers(1, &crosshairVBO);
					glDeleteVertexArrays(1, &crosshairVAO);
					glDeleteProgram(crosshairProgram);
					platform::destroyWindow();
					return -1;
				}
			}
		}
	}

			std::string vertexShaderPath = (assetRoot / "default.vert").string();
	std::string fragmentShaderPath = (assetRoot / "default.frag").string();
		Shader shaderProgram(vertexShaderPath.c_str(), fragmentShaderPath.c_str());
#ifdef __ANDROID__
	GLint shaderLinked = GL_FALSE;
	glGetProgramiv(shaderProgram.ID, GL_LINK_STATUS, &shaderLinked);
	if (shaderLinked != GL_TRUE)
	{
		return -1;
	}
#endif

	glm::vec4 lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	glm::vec3 lightPos = glm::vec3(0.5f, 0.5f, 0.5f);
	glm::mat4 lightModel = glm::mat4(1.0f);
	lightModel = glm::translate(lightModel, lightPos);

	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

	glEnable(GL_DEPTH_TEST);
	double dayNightStartTime = platform::getTime();
	constexpr float dayNightCycleSeconds = 180.0f;

	Camera camera(framebufferWidth, framebufferHeight, glm::vec3(0.0f, 2.5f, 6.0f));
	const fs::path mouseSensitivityPath = getMouseSensitivityPath();
	camera.sensitivity = loadMouseSensitivity(mouseSensitivityPath);
	const fs::path audioVolumePath = getAudioVolumePath();
	float audioVolume = loadAudioVolume(audioVolumePath);
	const fs::path aimTargetSizePath = getAimTargetSizePath();
	float aimTargetSize = loadAimTargetSize(aimTargetSizePath);
	cameraForFramebufferCallback = &camera;

	fs::path modelPath = assetRoot / "Resources" / "models" / "plane" / "plane.gltf";
	std::string modelFile = modelPath.string();

	if (!fs::exists(modelPath))
	{
		std::cerr << "Model file not found: " << modelPath << std::endl;
		platform::destroyWindow();
		return -1;
	}

	std::unique_ptr<Model> modelPtr;
	try
	{
		modelPtr = std::make_unique<Model>(modelFile.c_str());
	}
	catch (const std::exception& exception)
	{
		std::cerr << "Plane model load failed: " << exception.what() << std::endl;
		platform::destroyWindow();
		return -1;
	}
	Model& model = *modelPtr;

	fs::path orbPath = assetRoot / "Resources" / "models" / "orb" / "redorb.gltf";
	std::string orbFile = orbPath.string();
	if (!fs::exists(orbPath))
	{
		std::cerr << "Orb model not found: " << orbPath << std::endl;
		platform::destroyWindow();
		return -1;
	}
	Model orbModel(orbFile.c_str());

	fs::path prismPath = assetRoot / "Resources" / "models" / "prism" / "prism.gltf";
	if (!fs::exists(prismPath))
	{
		std::cerr << "Prism model not found: " << prismPath << std::endl;
		platform::destroyWindow();
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
	std::array<glm::vec3, 2> twoBallTargets =
	{
		glm::vec3(-4.0f, ballGroundY + 2.5f, -26.0f),
		glm::vec3(4.0f, ballGroundY + 6.0f, -26.0f)
	};
	std::array<glm::vec3, 6> sixBallTargets =
	{
		glm::vec3(-5.0f, ballGroundY + 2.0f, -26.0f),
		glm::vec3(0.0f, ballGroundY + 2.0f, -26.0f),
		glm::vec3(5.0f, ballGroundY + 2.0f, -26.0f),
		glm::vec3(-5.0f, ballGroundY + 6.0f, -26.0f),
		glm::vec3(0.0f, ballGroundY + 6.0f, -26.0f),
		glm::vec3(5.0f, ballGroundY + 6.0f, -26.0f)
	};
	std::array<bool, 6> sixBallTargetVisible = { true, true, true, true, true, true };
	constexpr float sixBallTargetSizeMultiplier = 0.55f;
	std::array<float, 2> twoBallTargetDirections = { 1.0f, -1.0f };
	constexpr float twoBallTargetMinX = -5.0f;
	constexpr float twoBallTargetMaxX = 5.0f;
	constexpr float twoBallTargetSpeed = 4.0f;
	std::mt19937 aimTargetRandomEngine(std::random_device{}());
	std::uniform_real_distribution<float> twoBallTargetSpawnX(twoBallTargetMinX, twoBallTargetMaxX);
	for (std::size_t targetIndex = 0; targetIndex < twoBallTargets.size(); ++targetIndex)
	{
		glm::vec3& target = twoBallTargets[targetIndex];
		target.x = twoBallTargetSpawnX(aimTargetRandomEngine);
		twoBallTargetDirections[targetIndex] = target.x < (twoBallTargetMinX + twoBallTargetMaxX) * 0.5f ? 1.0f : -1.0f;
	}
	std::uniform_real_distribution<float> aimTargetX(aimTrainerMinX, aimTrainerMaxX);
	std::uniform_real_distribution<float> aimTargetY(ballGroundY + 2.0f, ballGroundY + 8.0f);
	std::uniform_real_distribution<float> aimTargetZ(-30.0f, -23.0f);
	std::uniform_real_distribution<float> sixBallTargetX(-6.0f, 6.0f);
	std::uniform_real_distribution<float> sixBallTargetY(ballGroundY + 2.0f, ballGroundY + 8.0f);
	std::uniform_real_distribution<float> sixBallTargetZ(-29.0f, -23.0f);
	auto randomizeSixBallTargets = [&]()
	{
		for (std::size_t targetIndex = 0; targetIndex < sixBallTargets.size(); ++targetIndex)
		{
			sixBallTargetVisible[targetIndex] = true;
			bool placed = false;
			for (int attempt = 0; attempt < 100; ++attempt)
			{
				glm::vec3 candidate(sixBallTargetX(aimTargetRandomEngine),
					sixBallTargetY(aimTargetRandomEngine), sixBallTargetZ(aimTargetRandomEngine));
				bool overlapsAnotherTarget = false;
				for (std::size_t otherIndex = 0; otherIndex < sixBallTargets.size(); ++otherIndex)
				{
					if (otherIndex != targetIndex && sixBallTargetVisible[otherIndex] &&
						glm::length(candidate - sixBallTargets[otherIndex]) < 2.5f)
					{
						overlapsAnotherTarget = true;
						break;
					}
				}
				if (!overlapsAnotherTarget)
				{
					sixBallTargets[targetIndex] = candidate;
					placed = true;
					break;
				}
			}
				if (!placed)
				{
					sixBallTargets[targetIndex] = glm::vec3(sixBallTargetX(aimTargetRandomEngine),
						sixBallTargetY(aimTargetRandomEngine), sixBallTargetZ(aimTargetRandomEngine));
				}
		}
	};
	auto respawnSixBallTarget = [&](std::size_t targetIndex)
	{
		glm::vec3 previousPosition = sixBallTargets[targetIndex];
		sixBallTargetVisible[targetIndex] = false;
		for (int attempt = 0; attempt < 100; ++attempt)
		{
			glm::vec3 candidate(sixBallTargetX(aimTargetRandomEngine),
				sixBallTargetY(aimTargetRandomEngine), sixBallTargetZ(aimTargetRandomEngine));
			if (glm::length(candidate - previousPosition) < 2.5f)
				continue;

			bool overlapsAnotherTarget = false;
			for (std::size_t otherIndex = 0; otherIndex < sixBallTargets.size(); ++otherIndex)
			{
				if (otherIndex != targetIndex && sixBallTargetVisible[otherIndex] &&
					glm::length(candidate - sixBallTargets[otherIndex]) < 2.5f)
				{
					overlapsAnotherTarget = true;
					break;
				}
			}
			if (!overlapsAnotherTarget)
			{
				sixBallTargets[targetIndex] = candidate;
				sixBallTargetVisible[targetIndex] = true;
				return;
			}
		}
		sixBallTargets[targetIndex] = glm::vec3(sixBallTargetX(aimTargetRandomEngine),
			sixBallTargetY(aimTargetRandomEngine), sixBallTargetZ(aimTargetRandomEngine));
		sixBallTargetVisible[targetIndex] = true;
	};
	randomizeSixBallTargets();
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
	auto respawnTwoBallTarget = [&](std::size_t targetIndex)
	{
		glm::vec3& target = twoBallTargets[targetIndex];
		target.x = twoBallTargetSpawnX(aimTargetRandomEngine);
		target.y = ballGroundY + (targetIndex == 0 ? 2.5f : 6.0f);
		target.z = -26.0f;
		twoBallTargetDirections[targetIndex] = target.x < (twoBallTargetMinX + twoBallTargetMaxX) * 0.5f ? 1.0f : -1.0f;
	};
	constexpr double aimTaskDurationSeconds = 30.0;
	const fs::path aimHighScorePath = getAimHighScorePath();
	const fs::path twoBallsHighScorePath = getTwoBallsHighScorePath();
	const fs::path sixBallsHighScorePath = getSixBallsHighScorePath();
	const fs::path sessionTokenPath = getSessionTokenPath();
	const fs::path activationMarkerPath = getActivationMarkerPath();
	constexpr double aimTaskRetryCooldownSeconds = 1.0;
	int aimTaskScore = 0;
	int aimHighScore = loadAimHighScore(aimHighScorePath);
	int twoBallsHighScore = loadAimHighScore(twoBallsHighScorePath);
	int sixBallsHighScore = loadAimHighScore(sixBallsHighScorePath);
	std::string authenticatedSessionToken = loadSessionToken(sessionTokenPath);
	const std::string activatedEmail = loadSessionToken(activationMarkerPath);
	const std::string accountEmail = loadSessionToken(getApplicationDataDirectory() / "account.email");
	bool locallyActivated = !activatedEmail.empty() && activatedEmail == accountEmail;
	std::string cloudAccountName;
	std::string localPlayerName = loadSessionToken(getPlayerNamePath());
	bool restoreAuthenticatedSession = false;
	if (!authenticatedSessionToken.empty())
	{
		int remoteAimHighScore = 0;
		int remoteTwoBallsHighScore = 0;
		int remoteSixBallsHighScore = 0;
		std::string remoteAccountName;
		if (loadRankScores(authenticatedSessionToken, remoteAimHighScore, remoteTwoBallsHighScore, remoteSixBallsHighScore, remoteAccountName))
		{
			cloudAccountName = remoteAccountName;
			aimHighScore = std::max(aimHighScore, remoteAimHighScore);
			twoBallsHighScore = std::max(twoBallsHighScore, remoteTwoBallsHighScore);
			sixBallsHighScore = std::max(sixBallsHighScore, remoteSixBallsHighScore);
			restoreAuthenticatedSession = true;
		}
		else
		{
			deleteSessionToken(sessionTokenPath);
			authenticatedSessionToken.clear();
		}
	}
	std::vector<std::thread> rankSyncThreads;
	bool twoBallsScenario = false;
	bool sixBallsScenario = false;
	struct AimRank
	{
		const char* name;
		int minimumScore;
		glm::vec4 color;
	};
	const std::array<AimRank, 12> aimRanks = {{
		{ "WOOD", 0, glm::vec4(0.48f, 0.27f, 0.12f, 1.0f) },
		{ "BRONZE", 15, glm::vec4(0.72f, 0.38f, 0.20f, 1.0f) },
		{ "SILVER", 30, glm::vec4(0.78f, 0.81f, 0.84f, 1.0f) },
		{ "GOLD", 45, glm::vec4(1.0f, 0.74f, 0.10f, 1.0f) },
		{ "PLATINUM", 60, glm::vec4(0.83f, 0.88f, 0.90f, 1.0f) },
		{ "DIAMOND", 75, glm::vec4(0.48f, 0.86f, 1.0f, 1.0f) },
		{ "EMERALD", 90, glm::vec4(0.08f, 0.62f, 0.28f, 1.0f) },
		{ "AMETHYST", 105, glm::vec4(0.58f, 0.26f, 0.82f, 1.0f) },
		{ "REIDITE", 120, glm::vec4(0.55f, 0.24f, 0.20f, 1.0f) },
		{ "OBSIDIAN", 135, glm::vec4(0.24f, 0.19f, 0.31f, 1.0f) },
		{ "TITANIUM", 150, glm::vec4(0.62f, 0.66f, 0.70f, 1.0f) },
		{ "COLOSSEUM", 165, glm::vec4(0.72f, 0.60f, 0.40f, 1.0f) }
	}};
	auto getCurrentAimRank = [&]() -> const AimRank&
	{
		int scenarioHighScore = sixBallsScenario ? sixBallsHighScore :
			(twoBallsScenario ? twoBallsHighScore : aimHighScore);
		const AimRank* rank = &aimRanks.front();
		for (const AimRank& candidate : aimRanks)
		{
			if (scenarioHighScore < candidate.minimumScore)
				break;
			rank = &candidate;
		}
		return *rank;
	};
	bool aimTaskRunning = false;
	bool aimTaskFinished = false;
	bool wasAimTrainerActive = false;
	bool completedThreeBallsOnce = false;
	bool completedTwoBallsOnce = false;
	bool completedSixBallsOnce = false;
	bool previousThreeBallsKey = false;
	bool previousTwoBallsKey = false;
	bool previousSixBallsKey = false;
	bool previousScenarioTeleportKey = false;
	double aimTaskStartTime = 0.0;
	double aimTaskFinishedTime = 0.0;
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
	std::array<Ball, 1> balls =
	{
		Ball{ glm::vec3(0.0f, ballGroundY, 6.0f), glm::vec3(0.0f), true }
	};
	camera.Position = balls[playerBallIndex].position + glm::vec3(0.0f, cameraToPlayerBallOffset, 0.0f);
	int activeBall = 0;
	bool previousX = false;
	bool previousZ = false;
	bool previousLeftMouse = false;
	bool gamePaused = true;
	if (restoreAuthenticatedSession && locallyActivated)
	{
		signUpForm.active = false;
		signUpForm.complete = true;
		gamePaused = false;
		platform::setCursorCaptured(true);
	}
	else if (restoreAuthenticatedSession)
	{
		signUpForm.active = true;
		signUpForm.activationMode = true;
		signUpForm.focusedField = SignUpField::ActivationKey;
		signUpForm.email = loadSessionToken(getApplicationDataDirectory() / "account.email");
		gamePaused = true;
	}
	#ifdef AIMTRAINER_OFFLINE_BYPASS
		signUpForm.active = false;
		signUpForm.complete = true;
		gamePaused = false;
		platform::setCursorCaptured(true);
	#endif
		if (!isAuthApiConfigured())
			signUpForm.status = "SET API URL";
	bool showingControls = false;
	bool showingRanks = false;
	bool showingScenarioRanks = false;
	int crosshairStyle = 0;
	bool previousCrosshairKey = false;
	bool previousRanksKey = false;
	bool previousEscape = false;
	bool previousPauseMouse = false;
	bool draggingSensitivity = false;
	bool draggingAudioVolume = false;
	bool draggingAimTargetSize = false;
	double pauseStartedAt = 0.0;
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
	double previousTime = platform::getTime();
	float physicsAccumulator = 0.0f;
	double fpsWindowStart = previousTime;
	double hudWindowStart = previousTime;
	unsigned int fpsFrameCount = 0;
	int displayedFps = 0;
	glm::vec3 previousCameraPosition = camera.Position;
	std::vector<std::uint8_t> jumpSound = makeConcreteJumpWav();
	std::vector<std::uint8_t> aimTargetHitSound = makeAimTargetHitWav();
	std::vector<std::uint8_t> adjustedJumpSound;
	std::vector<std::uint8_t> adjustedAimTargetHitSound;
	setWavVolume(jumpSound, adjustedJumpSound, audioVolume);
	setWavVolume(aimTargetHitSound, adjustedAimTargetHitSound, audioVolume);
#ifndef AIMTRAINER_ANDROID
#ifndef _WIN32
	if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
	{
		std::cerr << "Failed to initialize HTTPS support" << std::endl;
		platform::destroyWindow();
		return -1;
	}
	setPortableAudioVolume(audioVolume);
#endif
#endif
	float fpsCellSize = std::max(2.0f, static_cast<float>(framebufferHeight) / 300.0f);
	std::string fpsLabel = "FPS: 0";
	std::vector<GLfloat> fpsVertices = buildFpsVertices(fpsLabel, framebufferWidth, framebufferHeight, fpsCellSize);
	std::string speedLabel = "SPD: 0";
	std::vector<GLfloat> speedVertices = buildFpsVertices(speedLabel, framebufferWidth, framebufferHeight, fpsCellSize, 16.0f + 9.0f * fpsCellSize);
	std::string aimTaskLabel = "T:30 H:0 B:" + std::to_string(aimHighScore);
	std::vector<GLfloat> aimTaskVertices = buildFpsVertices(aimTaskLabel, framebufferWidth, framebufferHeight, fpsCellSize, 16.0f + 18.0f * fpsCellSize);
	fpsVertices.insert(fpsVertices.end(), speedVertices.begin(), speedVertices.end());
	fpsVertices.insert(fpsVertices.end(), aimTaskVertices.begin(), aimTaskVertices.end());
	GLint rankVertexStart = 0;
	GLsizei rankVertexCount = 0;
	std::array<GLint, 12> rankLadderVertexStarts{};
	std::array<GLsizei, 12> rankLadderVertexCounts{};
	std::array<glm::vec4, 12> rankLadderColors{};
	glm::vec4 currentRankColor = getCurrentAimRank().color;
	bool rankLadderVisible = false;
	glBindBuffer(GL_ARRAY_BUFFER, fpsVBO);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(fpsVertices.size() * sizeof(GLfloat)), fpsVertices.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	while (!platform::shouldClose())
	{
		processInput();
		SignUpResult completedSignUpResult;
		bool hasCompletedSignUpRequest = false;
		{
			std::lock_guard<std::mutex> resultLock(signUpResultMutex);
			if (signUpResultReady)
			{
				completedSignUpResult = signUpResult;
				signUpResultReady = false;
				hasCompletedSignUpRequest = true;
			}
		}
		if (hasCompletedSignUpRequest)
		{
			if (signUpAuthThread.joinable())
				signUpAuthThread.join();
			signUpForm.submitting = false;
			signUpForm.status = completedSignUpResult.message;
			if (signUpForm.activationMode)
			{
				if (completedSignUpResult.activationAccepted)
				{
					signUpForm.activationMode = false;
					signUpForm.activationKey.clear();
					saveSessionToken(activationMarkerPath, signUpForm.email);
					signUpForm.active = false;
					signUpForm.complete = true;
					gamePaused = false;
					platform::setCursorCaptured(true);
				}
			}
			else if (completedSignUpResult.confirmationPending)
				signUpForm.emailConfirmationPending = true;
			if (!signUpForm.activationMode && completedSignUpResult.message == "EMAIL NOT CONFIRMED")
				signUpForm.emailConfirmationPending = true;
			if (completedSignUpResult.authenticated)
			{
				authenticatedSessionToken = completedSignUpResult.sessionToken;
				saveSessionToken(sessionTokenPath, authenticatedSessionToken);
				saveSessionToken(getApplicationDataDirectory() / "account.email", signUpForm.email);
				if (completedSignUpResult.ranksSynchronized)
				{
					if (completedSignUpResult.aimBestScore > aimHighScore)
					{
						aimHighScore = completedSignUpResult.aimBestScore;
						saveAimHighScore(aimHighScorePath, aimHighScore);
					}
					if (completedSignUpResult.twoBallsBestScore > twoBallsHighScore)
					{
						twoBallsHighScore = completedSignUpResult.twoBallsBestScore;
						saveAimHighScore(twoBallsHighScorePath, twoBallsHighScore);
					}
					if (completedSignUpResult.sixBallsBestScore > sixBallsHighScore)
					{
						sixBallsHighScore = completedSignUpResult.sixBallsBestScore;
						saveAimHighScore(sixBallsHighScorePath, sixBallsHighScore);
					}
				}
				if (signUpForm.email == website_api_config::ownerEmail)
				{
					signUpForm.active = false;
					signUpForm.complete = true;
					gamePaused = false;
					camera.mouseInitialized = false;
					platform::setCursorCaptured(true);
				}
				else
				{
					signUpForm.activationMode = true;
					signUpForm.focusedField = SignUpField::ActivationKey;
					signUpForm.status = "ENTER ACTIVATION KEY";
				}
				signUpForm.password.clear();
				signUpForm.confirmPassword.clear();
			}
			if (!signUpForm.activationMode && completedSignUpResult.message == "EMAIL ALREADY USED")
			{
				signUpForm.signInMode = true;
				signUpForm.focusedField = SignUpField::Email;
				signUpForm.password.clear();
				signUpForm.confirmPassword.clear();
			}
		}

		double currentTime = platform::getTime();
		bool escapePressed = platform::isKeyDown(platform::Key::Escape);
		bool pauseStateChanged = false;
		bool rankMenuPage = gamePaused && (showingRanks || showingScenarioRanks);
		bool controlsMenuPage = gamePaused && showingControls;
		if (nameEditor.submitRequested)
		{
			nameEditor.submitRequested = false;
			localPlayerName = nameEditor.text;
			if (localPlayerName.empty())
				deleteSessionToken(getPlayerNamePath());
			else
				saveSessionToken(getPlayerNamePath(), localPlayerName);
		}
		if (escapePressed && !previousEscape && nameEditor.active)
		{
			nameEditor.active = false;
		}
		else if (escapePressed && !previousEscape && !signUpForm.active)
		{
			pauseStateChanged = true;
			if (rankMenuPage || controlsMenuPage)
			{
				showingControls = false;
				showingRanks = false;
				showingScenarioRanks = false;
			}
			else if (gamePaused)
			{
				gamePaused = false;
				showingControls = false;
				showingRanks = false;
				showingScenarioRanks = false;
				camera.mouseInitialized = false;
				platform::setCursorCaptured(true);				platform::setCursorCaptured(true);

					aimTaskStartTime += currentTime - pauseStartedAt;
			}
			else
			{
				gamePaused = true;
				showingControls = false;
				showingRanks = false;
				showingScenarioRanks = false;
				platform::setCursorCaptured(false);
				pauseStartedAt = currentTime;
				previousCameraPosition = camera.Position;
			}
		}
		previousEscape = escapePressed;
		rankMenuPage = gamePaused && (showingRanks || showingScenarioRanks);
		controlsMenuPage = gamePaused && showingControls;
		bool pauseMousePressed = platform::isLeftMouseDown();
		if (controlsMenuPage && !draggingSensitivity && !draggingAudioVolume && !draggingAimTargetSize &&
			pauseMousePressed && !previousPauseMouse)
		{
			int sliderFramebufferWidth = 0;
			int sliderFramebufferHeight = 0;
			platform::getFramebufferSize(sliderFramebufferWidth, sliderFramebufferHeight);
			double cursorX = 0.0;
			double cursorY = 0.0;
			platform::getCursorPosition(cursorX, cursorY);
			if (sliderFramebufferWidth > 0 && sliderFramebufferHeight > 0)
			{
				float clickX = static_cast<float>(cursorX);
				float clickY = static_cast<float>(cursorY);
				float cellSize = std::max(2.0f, static_cast<float>(sliderFramebufferHeight) / 300.0f);
				float sliderLeft = static_cast<float>(sliderFramebufferWidth) * 0.5f - 55.0f * cellSize;
				float sliderRight = static_cast<float>(sliderFramebufferWidth) * 0.5f + 55.0f * cellSize;
				float sensitivityLabelTop = static_cast<float>(sliderFramebufferHeight) * 0.60f;
				float sensitivitySliderY = sensitivityLabelTop + 7.0f * cellSize;
				float volumeLabelTop = sensitivityLabelTop + 14.0f * cellSize;
				float volumeSliderY = volumeLabelTop + 7.0f * cellSize;
				float targetSizeLabelTop = sensitivityLabelTop + 25.0f * cellSize;
				float targetSizeSliderY = targetSizeLabelTop + 7.0f * cellSize;
				if (clickX >= sliderLeft - 8.0f * cellSize && clickX <= sliderRight + 8.0f * cellSize &&
					clickY >= sensitivitySliderY - 5.0f * cellSize && clickY <= sensitivitySliderY + 5.0f * cellSize)
					draggingSensitivity = true;
				else if (clickX >= sliderLeft - 8.0f * cellSize && clickX <= sliderRight + 8.0f * cellSize &&
					clickY >= volumeSliderY - 5.0f * cellSize && clickY <= volumeSliderY + 5.0f * cellSize)
					draggingAudioVolume = true;
				else if (clickX >= sliderLeft - 8.0f * cellSize && clickX <= sliderRight + 8.0f * cellSize &&
					clickY >= targetSizeSliderY - 5.0f * cellSize && clickY <= targetSizeSliderY + 5.0f * cellSize)
					draggingAimTargetSize = true;
			}
		}
		if (!pauseMousePressed)
		{
			draggingSensitivity = false;
			draggingAudioVolume = false;
			draggingAimTargetSize = false;
		}
		if (controlsMenuPage &&
			(draggingSensitivity || draggingAudioVolume || draggingAimTargetSize) && pauseMousePressed)
		{
			int sliderFramebufferWidth = 0;
			int sliderFramebufferHeight = 0;
			platform::getFramebufferSize(sliderFramebufferWidth, sliderFramebufferHeight);
			double cursorX = 0.0;
			double cursorY = 0.0;
			platform::getCursorPosition(cursorX, cursorY);
			if (sliderFramebufferWidth > 0 && sliderFramebufferHeight > 0)
			{
				float clickX = static_cast<float>(cursorX);
				float cellSize = std::max(2.0f, static_cast<float>(sliderFramebufferHeight) / 300.0f);
				float sliderLeft = static_cast<float>(sliderFramebufferWidth) * 0.5f - 55.0f * cellSize;
				float sliderWidth = 110.0f * cellSize;
				if (draggingSensitivity)
				{
					float amount = glm::clamp((clickX - sliderLeft) / sliderWidth, 0.0f, 1.0f);
					camera.sensitivity = 0.02f + amount * 0.28f;
					saveMouseSensitivity(mouseSensitivityPath, camera.sensitivity);
					pauseStateChanged = true;
				}
				if (draggingAimTargetSize)
				{
					float amount = glm::clamp((clickX - sliderLeft) / sliderWidth, 0.0f, 1.0f);
					aimTargetSize = 0.25f + amount * 0.75f;
					saveAimTargetSize(aimTargetSizePath, aimTargetSize);
					pauseStateChanged = true;
				}
				if (draggingAudioVolume)
				{
					float amount = glm::clamp((clickX - sliderLeft) / sliderWidth, 0.0f, 1.0f);
					audioVolume = amount;
					saveAudioVolume(audioVolumePath, audioVolume);
					setWavVolume(jumpSound, adjustedJumpSound, audioVolume);
					setWavVolume(aimTargetHitSound, adjustedAimTargetHitSound, audioVolume);
#ifndef _WIN32
					setPortableAudioVolume(audioVolume);
#endif
					pauseStateChanged = true;
				}
			}
		}
		if (gamePaused && pauseMousePressed && !previousPauseMouse)
		{
			int clickFramebufferWidth = 0;
			int clickFramebufferHeight = 0;
			platform::getFramebufferSize(clickFramebufferWidth, clickFramebufferHeight);
			double cursorX = 0.0;
			double cursorY = 0.0;
			platform::getCursorPosition(cursorX, cursorY);
			if (clickFramebufferWidth > 0 && clickFramebufferHeight > 0)
			{
				float clickX = static_cast<float>(cursorX);
				float clickY = static_cast<float>(cursorY);
				float menuCellSize = std::max(2.0f, static_cast<float>(clickFramebufferHeight) / 300.0f);
				if (signUpForm.active)
				{
					if (!signUpForm.submitting)
					{
					if (signUpForm.activationMode)
					{
						float activationTop = static_cast<float>(clickFramebufferHeight) * 0.40f;
						float formCenterX = static_cast<float>(clickFramebufferWidth) * 0.5f;
						if (clickX >= formCenterX - 100.0f * menuCellSize && clickX <= formCenterX + 100.0f * menuCellSize &&
							clickY >= activationTop && clickY <= activationTop + 14.0f * menuCellSize)
							signUpForm.focusedField = SignUpField::ActivationKey;
						else if (clickX >= formCenterX - 40.0f * menuCellSize && clickX <= formCenterX + 40.0f * menuCellSize &&
							clickY >= static_cast<float>(clickFramebufferHeight) * 0.67f - 5.0f * menuCellSize &&
							clickY <= static_cast<float>(clickFramebufferHeight) * 0.67f + 12.0f * menuCellSize)
							signUpForm.submitRequested = true;
					}
					else
					{
					float emailLabelTop = static_cast<float>(clickFramebufferHeight) * 0.28f;
					float passwordLabelTop = static_cast<float>(clickFramebufferHeight) * 0.40f;
					float confirmLabelTop = static_cast<float>(clickFramebufferHeight) * 0.52f;
					float formCenterX = static_cast<float>(clickFramebufferWidth) * 0.5f;
					if (clickX >= formCenterX - 100.0f * menuCellSize && clickX <= formCenterX + 100.0f * menuCellSize)
					{
						if (clickY >= emailLabelTop + 3.0f * menuCellSize && clickY <= emailLabelTop + 12.0f * menuCellSize)
							signUpForm.focusedField = SignUpField::Email;
						else if (clickY >= passwordLabelTop + 3.0f * menuCellSize && clickY <= passwordLabelTop + 12.0f * menuCellSize)
							signUpForm.focusedField = SignUpField::Password;
						else if (!signUpForm.signInMode && clickY >= confirmLabelTop + 3.0f * menuCellSize && clickY <= confirmLabelTop + 12.0f * menuCellSize)
							signUpForm.focusedField = SignUpField::ConfirmPassword;
						else if (clickX >= formCenterX - 40.0f * menuCellSize && clickX <= formCenterX + 40.0f * menuCellSize &&
							clickY >= passwordLabelTop + 13.0f * menuCellSize && clickY <= passwordLabelTop + 20.0f * menuCellSize)
							signUpForm.showPassword = !signUpForm.showPassword;
						else if (clickX >= formCenterX - 40.0f * menuCellSize && clickX <= formCenterX + 40.0f * menuCellSize &&
							clickY >= static_cast<float>(clickFramebufferHeight) * 0.67f - 5.0f * menuCellSize &&
							clickY <= static_cast<float>(clickFramebufferHeight) * 0.67f + 12.0f * menuCellSize)
							signUpForm.submitRequested = true;
						else if (clickX >= formCenterX - 75.0f * menuCellSize && clickX <= formCenterX + 75.0f * menuCellSize &&
							clickY >= static_cast<float>(clickFramebufferHeight) * 0.75f - 5.0f * menuCellSize &&
							clickY <= static_cast<float>(clickFramebufferHeight) * 0.75f + 12.0f * menuCellSize)
						{
							signUpForm.signInMode = !signUpForm.signInMode;
							if (signUpForm.signInMode && signUpForm.focusedField == SignUpField::ConfirmPassword)
								signUpForm.focusedField = SignUpField::Email;
					}
					}
					}
					}
				}
				else if (controlsMenuPage)
				{
					float backTop = static_cast<float>(clickFramebufferHeight) - 48.0f * menuCellSize;
					float backCenterX = static_cast<float>(clickFramebufferWidth) * 0.5f;
					if (clickX >= backCenterX - 20.0f * menuCellSize && clickX <= backCenterX + 20.0f * menuCellSize &&
						clickY >= backTop - 5.0f * menuCellSize && clickY <= backTop + 12.0f * menuCellSize)
					{
						showingControls = false;
						controlsMenuPage = false;
						pauseStateChanged = true;
					}
				}
				else if (showingRanks || showingScenarioRanks)
				{
					float backTop = static_cast<float>(clickFramebufferHeight) - 48.0f * menuCellSize;
					float backCenterX = static_cast<float>(clickFramebufferWidth) * 0.5f;
					if (clickX >= backCenterX - 20.0f * menuCellSize && clickX <= backCenterX + 20.0f * menuCellSize &&
						clickY >= backTop - 5.0f * menuCellSize && clickY <= backTop + 12.0f * menuCellSize)
					{
						showingRanks = false;
						showingScenarioRanks = false;
						pauseStateChanged = true;
					}
				}
				else
				{
					auto buttonHit = [&](const std::string& label, float top, float scale)
					{
						float width = static_cast<float>(label.size() * 4 - 1) * menuCellSize * scale;
						return clickX >= (clickFramebufferWidth - width) * 0.5f - 8.0f * menuCellSize &&
							clickX <= (clickFramebufferWidth + width) * 0.5f + 8.0f * menuCellSize &&
							clickY >= top - 5.0f * menuCellSize && clickY <= top + 12.0f * menuCellSize;
					};
					float pauseTop = static_cast<float>(clickFramebufferHeight) * 0.30f;
					if (buttonHit("RESUME", pauseTop + 18.0f * menuCellSize, 1.5f))
					{
						gamePaused = false;
						showingControls = false;
						showingRanks = false;
						showingScenarioRanks = false;
						camera.mouseInitialized = false;
						platform::setCursorCaptured(true);
						if (aimTaskRunning)
							aimTaskStartTime += currentTime - pauseStartedAt;
						pauseStateChanged = true;
					}
					else if (buttonHit("SCENARIO RANKS", pauseTop + 84.0f * menuCellSize, 1.5f))
					{
						showingScenarioRanks = true;
						showingRanks = showingControls = false;
						pauseStateChanged = true;
					}
					else if (buttonHit("RANKS", pauseTop + 62.0f * menuCellSize, 1.5f))
					{
						showingRanks = true;
						showingControls = showingScenarioRanks = false;
						pauseStateChanged = true;
					}
					else if (buttonHit("CONTROLS", pauseTop + 40.0f * menuCellSize, 1.5f))
					{
						showingControls = !showingControls;
						showingRanks = showingScenarioRanks = false;
						pauseStateChanged = true;
					}
					else if (buttonHit("NAME", pauseTop + 104.0f * menuCellSize, 1.5f))
					{
						if (nameEditor.active)
						{
							nameEditor.active = false;
						}
						else
						{
							nameEditor.text = localPlayerName;
							nameEditor.active = true;
						}
						pauseStateChanged = true;
					}
					else if (buttonHit("EXIT", pauseTop + 124.0f * menuCellSize, 1.5f))
					{
						platform::requestClose();
					}
				}
			}
		}
		if (signUpForm.active && signUpForm.submitRequested && !signUpForm.submitting)
		{
			signUpForm.submitRequested = false;
			if (signUpForm.activationMode)
			{
				bool validFormat = signUpForm.activationKey.size() == 43 && signUpForm.activationKey.starts_with("AIM-");
				if (!validFormat)
					signUpForm.status = "INVALID KEY FORMAT";
				else
				{
					std::string email = signUpForm.email;
					std::string activationKey = signUpForm.activationKey;
					signUpForm.status.clear();
					signUpForm.submitting = true;
					{
						std::lock_guard<std::mutex> resultLock(signUpResultMutex);
						signUpResultReady = false;
					}
					signUpAuthThread = std::thread([email = std::move(email), activationKey = std::move(activationKey), sessionToken = authenticatedSessionToken]()
					{
						SignUpResult requestResult = sendActivationApiRequest(email, activationKey, sessionToken);
						std::lock_guard<std::mutex> resultLock(signUpResultMutex);
						signUpResult = std::move(requestResult);
						signUpResultReady = true;
					});
				}
			}
			else if (!isAuthApiConfigured())
				signUpForm.status = "SET API URL";
			else
			{
				std::size_t atPosition = signUpForm.email.find('@');
				std::size_t dotPosition = signUpForm.email.find('.', atPosition == std::string::npos ? 0 : atPosition);
				if (atPosition == std::string::npos || atPosition == 0 || dotPosition == std::string::npos ||
					dotPosition <= atPosition + 1 || dotPosition + 1 >= signUpForm.email.size())
					signUpForm.status = "INVALID EMAIL";
				else if (signUpForm.password.size() < 6)
					signUpForm.status = "PASSWORD TOO SHORT";
				else if (!signUpForm.signInMode && signUpForm.password != signUpForm.confirmPassword)
					signUpForm.status = "PASSWORDS DO NOT MATCH";
				else
				{
					std::string email = signUpForm.email;
					std::string password = signUpForm.password;
					bool signIn = signUpForm.signInMode;
					signUpForm.status.clear();
					signUpForm.submitting = true;
					{
						std::lock_guard<std::mutex> resultLock(signUpResultMutex);
						signUpResultReady = false;
					}
					try
					{
						 signUpAuthThread = std::thread([email = std::move(email), password = std::move(password), signIn,
							 localAimBest = aimHighScore, localTwoBallsBest = twoBallsHighScore, localSixBallsBest = sixBallsHighScore]()
						{
							SignUpResult requestResult = sendAuthApiRequest(email, password, signIn);
							if (requestResult.authenticated && !requestResult.sessionToken.empty())
							{
								requestResult.aimBestScore = localAimBest;
								requestResult.twoBallsBestScore = localTwoBallsBest;
								requestResult.sixBallsBestScore = localSixBallsBest;
								requestResult.ranksSynchronized = sendRankScores(requestResult.sessionToken,
									requestResult.aimBestScore, requestResult.twoBallsBestScore, requestResult.sixBallsBestScore);
							}
							std::lock_guard<std::mutex> resultLock(signUpResultMutex);
							signUpResult = std::move(requestResult);
							signUpResultReady = true;
						});
					}
					catch (const std::system_error&)
					{
						signUpForm.submitting = false;
						signUpForm.status = "NETWORK ERROR";
					}
				}
			}
		}
		rankMenuPage = gamePaused && (showingRanks || showingScenarioRanks);
		controlsMenuPage = gamePaused && showingControls;
		previousPauseMouse = pauseMousePressed;
		double aimTaskClockTime = gamePaused ? pauseStartedAt : currentTime;
		double elapsedSincePreviousFrame = currentTime - previousTime;
		float deltaTime = gamePaused ? 0.0f : static_cast<float>(std::min(elapsedSincePreviousFrame, 0.05));
		previousTime = currentTime;
		if (gamePaused)
			physicsAccumulator = 0.0f;
		else
			physicsAccumulator += deltaTime;
		if (gamePaused)
			dayNightStartTime += elapsedSincePreviousFrame;
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
		if (currentTime - hudWindowStart >= 0.1 || fpsTextChanged || pauseStateChanged)
		{
			rankVertexStart = 0;
			rankVertexCount = 0;
			rankLadderVertexStarts.fill(0);
			rankLadderVertexCounts.fill(0);
			rankLadderVisible = rankMenuPage && showingRanks;
			currentRankColor = getCurrentAimRank().color;
			hudWindowStart = currentTime;
			float horizontalSpeed = glm::length(glm::vec2(camera.horizontalVelocity.x, camera.horizontalVelocity.z));
			speedLabel = "SPD: " + std::to_string(static_cast<int>(std::round(horizontalSpeed)));
			platform::getFramebufferSize(framebufferWidth, framebufferHeight);
			fpsCellSize = std::max(2.0f, static_cast<float>(framebufferHeight) / 300.0f);
			fpsVertices = (signUpForm.active || rankMenuPage || controlsMenuPage)
				? std::vector<GLfloat>()
				: buildFpsVertices(fpsLabel, framebufferWidth, framebufferHeight, fpsCellSize);
			speedVertices = buildFpsVertices(speedLabel, framebufferWidth, framebufferHeight, fpsCellSize, 16.0f + 9.0f * fpsCellSize);
			int aimTaskSecondsRemaining = 30;
			if (aimTaskFinished)
				aimTaskSecondsRemaining = 0;
			else if (aimTaskRunning)
				aimTaskSecondsRemaining = static_cast<int>(std::ceil(std::max(0.0, aimTaskDurationSeconds - (aimTaskClockTime - aimTaskStartTime))));
			aimTaskLabel = "T:" + std::to_string(aimTaskSecondsRemaining) +
				" H:" + std::to_string(aimTaskScore) +
			" B:" + std::to_string(sixBallsScenario ? sixBallsHighScore :
				(twoBallsScenario ? twoBallsHighScore : aimHighScore));
			aimTaskVertices = buildFpsVertices(aimTaskLabel, framebufferWidth, framebufferHeight, fpsCellSize, 16.0f + 18.0f * fpsCellSize);
			if (!signUpForm.active && !rankMenuPage && !controlsMenuPage)
			{
				fpsVertices.insert(fpsVertices.end(), speedVertices.begin(), speedVertices.end());
				fpsVertices.insert(fpsVertices.end(), aimTaskVertices.begin(), aimTaskVertices.end());
				std::vector<GLfloat> rankVertices = buildFpsVertices(
					"R: " + std::string(getCurrentAimRank().name),
					framebufferWidth, framebufferHeight, fpsCellSize, 16.0f + 27.0f * fpsCellSize);
				rankVertexStart = static_cast<GLint>(fpsVertices.size() / 2);
				rankVertexCount = static_cast<GLsizei>(rankVertices.size() / 2);
				fpsVertices.insert(fpsVertices.end(), rankVertices.begin(), rankVertices.end());
#ifdef __ANDROID__
				const std::array<std::string, 3> bestScoreLabels = {
					"A: " + std::to_string(aimHighScore),
					"2B: " + std::to_string(twoBallsHighScore),
					"6B: " + std::to_string(sixBallsHighScore)
				};
				for (std::size_t labelIndex = 0; labelIndex < bestScoreLabels.size(); ++labelIndex)
				{
					std::vector<GLfloat> bestScoreVertices = buildFpsVertices(bestScoreLabels[labelIndex], framebufferWidth, framebufferHeight, fpsCellSize, 16.0f + (36.0f + 9.0f * static_cast<float>(labelIndex)) * fpsCellSize);
					fpsVertices.insert(fpsVertices.end(), bestScoreVertices.begin(), bestScoreVertices.end());
				}
#endif
			}
			int scenarioHighScore = sixBallsScenario ? sixBallsHighScore :
				(twoBallsScenario ? twoBallsHighScore : aimHighScore);
			int currentRankMinimumScore = getCurrentAimRank().minimumScore;
			int nextRankMinimumScore = aimRanks.back().minimumScore;
			if (currentRankMinimumScore < aimRanks.back().minimumScore)
			{
				for (const AimRank& rank : aimRanks)
				{
					if (rank.minimumScore > scenarioHighScore)
					{
						nextRankMinimumScore = rank.minimumScore;
						break;
					}
				}
			}
			if (rankLadderVisible)
			{
				auto appendCenteredRankText = [&](const std::string& text, float cellSize, float top)
				{
					float width = static_cast<float>(text.size() * 4 - 1) * cellSize;
					std::vector<GLfloat> vertices = buildFpsVertices(text, framebufferWidth, framebufferHeight,
						cellSize, top, (static_cast<float>(framebufferWidth) - width) * 0.5f);
					fpsVertices.insert(fpsVertices.end(), vertices.begin(), vertices.end());
				};
				appendCenteredRankText("RANKS", fpsCellSize * 1.5f, static_cast<float>(framebufferHeight) * 0.12f);
				appendCenteredRankText("CURRENT " + std::string(getCurrentAimRank().name), fpsCellSize,
					static_cast<float>(framebufferHeight) * 0.19f);
				std::size_t currentRankIndex = static_cast<std::size_t>(&getCurrentAimRank() - aimRanks.data());
				for (std::size_t rankIndex = 0; rankIndex < aimRanks.size(); ++rankIndex)
				{
					float rowTop = static_cast<float>(framebufferHeight) * 0.25f + static_cast<float>(rankIndex) * 4.5f * fpsCellSize;
					const AimRank& rank = aimRanks[rankIndex];
					std::string rankLine = std::string(rank.name) + " " + std::to_string(rank.minimumScore);
					float rankCellSize = fpsCellSize * 0.8f;
					float rankLineWidth = static_cast<float>(rankLine.size() * 4 - 1) * rankCellSize;
					std::vector<GLfloat> nameVertices = buildFpsVertices(rankLine,
						framebufferWidth, framebufferHeight, rankCellSize, rowTop,
						(static_cast<float>(framebufferWidth) - rankLineWidth) * 0.5f);
					std::size_t startVertex = fpsVertices.size() / 2;
					rankLadderColors[rankIndex] = rank.color;
					fpsVertices.insert(fpsVertices.end(), nameVertices.begin(), nameVertices.end());
					if (rankIndex == currentRankIndex)
					{
						std::vector<GLfloat> checkVertices = buildFpsVertices(">", framebufferWidth, framebufferHeight,
							rankCellSize, rowTop, (static_cast<float>(framebufferWidth) - rankLineWidth) * 0.5f - 6.0f * rankCellSize);
						fpsVertices.insert(fpsVertices.end(), checkVertices.begin(), checkVertices.end());
					}
					rankLadderVertexStarts[rankIndex] = static_cast<GLint>(startVertex);
					rankLadderVertexCounts[rankIndex] = static_cast<GLsizei>(fpsVertices.size() / 2 - startVertex);
				}
				std::string progressText = scenarioHighScore >= aimRanks.back().minimumScore
					? "MAX RANK"
					: "NEXT " + std::to_string(std::max(0, nextRankMinimumScore - scenarioHighScore));
				appendCenteredRankText(progressText, fpsCellSize, static_cast<float>(framebufferHeight) * 0.25f + 12.0f * 4.5f * fpsCellSize);
				const std::string displayedName = localPlayerName.empty() ? cloudAccountName : localPlayerName;
				if (!displayedName.empty())
					appendCenteredRankText(displayedName, fpsCellSize, static_cast<float>(framebufferHeight) * 0.09f);
			}
			if (signUpForm.active)
			{
				auto getVisiblePasswordText = [](const std::string& password)
				{
					return password.size() > 34 ? password.substr(password.size() - 34) : password;
				};
				auto appendCenteredSignUpText = [&](const std::string& text, float top, float scale = 1.0f, float characterAdvance = 4.0f)
				{
					float cellSize = fpsCellSize * scale;
					float textUnits = text.empty() ? 0.0f : static_cast<float>(text.size() - 1) * characterAdvance + 3.0f;
					cellSize = std::min(cellSize, (static_cast<float>(framebufferWidth) - 24.0f * fpsCellSize) / std::max(1.0f, textUnits));
					cellSize = std::max(1.0f, cellSize);
					float width = textUnits * cellSize;
					std::vector<GLfloat> textVertices = buildFpsVertices(text, framebufferWidth, framebufferHeight,
						cellSize, top, (static_cast<float>(framebufferWidth) - width) * 0.5f, characterAdvance);
					if (!textVertices.empty())
					{
						GLfloat minimumX = textVertices.front();
						GLfloat maximumX = minimumX;
						for (std::size_t vertexIndex = 0; vertexIndex < textVertices.size(); vertexIndex += 2)
						{
							minimumX = std::min(minimumX, textVertices[vertexIndex]);
							maximumX = std::max(maximumX, textVertices[vertexIndex]);
						}
						float actualCenter = (minimumX + maximumX) * 0.5f;
						float centerCorrection = -actualCenter;
						for (std::size_t vertexIndex = 0; vertexIndex < textVertices.size(); vertexIndex += 2)
							textVertices[vertexIndex] += centerCorrection;
					}
					fpsVertices.insert(fpsVertices.end(), textVertices.begin(), textVertices.end());
				};
				if (signUpForm.activationMode)
				{
					float activationTop = static_cast<float>(framebufferHeight) * 0.40f;
					appendCenteredSignUpText("ACTIVATE GAME", static_cast<float>(framebufferHeight) * 0.20f, 1.6f, 4.5f);
					appendCenteredSignUpText("ACTIVATION KEY", activationTop);
					std::string visibleKey = signUpForm.activationKey.size() > 43
						? signUpForm.activationKey.substr(signUpForm.activationKey.size() - 43) : signUpForm.activationKey;
					appendCenteredSignUpText((signUpForm.focusedField == SignUpField::ActivationKey ? ">" : " ") +
						(visibleKey.empty() ? "CLICK TO TYPE" : visibleKey), activationTop + 6.0f * fpsCellSize);
					appendCenteredSignUpText(signUpForm.submitting ? "CONNECTING" : "ACTIVATE",
						static_cast<float>(framebufferHeight) * 0.67f, 1.4f, 4.5f);
					appendCenteredSignUpText(signUpForm.status.empty() ? "ENTER THE KEY FROM YOUR PURCHASE" : signUpForm.status,
						static_cast<float>(framebufferHeight) * 0.86f);
				}
				else
				{
				float emailTop = static_cast<float>(framebufferHeight) * 0.28f;
				float passwordTop = static_cast<float>(framebufferHeight) * 0.40f;
				float confirmTop = static_cast<float>(framebufferHeight) * 0.52f;
				appendCenteredSignUpText(signUpForm.signInMode ? "SIGN IN" : "CREATE ACCOUNT",
					static_cast<float>(framebufferHeight) * 0.16f, 1.6f, 4.5f);
				appendCenteredSignUpText("EMAIL", emailTop);
				std::string visibleEmail = signUpForm.email.size() > 34 ? signUpForm.email.substr(signUpForm.email.size() - 34) : signUpForm.email;
				appendCenteredSignUpText((signUpForm.focusedField == SignUpField::Email ? ">" : " ") +
					(visibleEmail.empty() ? "CLICK TO TYPE" : visibleEmail), emailTop + 6.0f * fpsCellSize);
				appendCenteredSignUpText("PASSWORD", passwordTop);
				appendCenteredSignUpText((signUpForm.focusedField == SignUpField::Password ? ">" : " ") +
					(signUpForm.password.empty() ? "CLICK TO TYPE" : (signUpForm.showPassword ? getVisiblePasswordText(signUpForm.password) : std::string(std::min<std::size_t>(signUpForm.password.size(), 34), '*'))),
					passwordTop + 6.0f * fpsCellSize);
				appendCenteredSignUpText(signUpForm.showPassword ? "HIDE PASSWORD" : "SHOW PASSWORD",
					passwordTop + 14.0f * fpsCellSize);
				if (!signUpForm.signInMode)
				{
					appendCenteredSignUpText("CONFIRM", confirmTop);
					appendCenteredSignUpText((signUpForm.focusedField == SignUpField::ConfirmPassword ? ">" : " ") +
						(signUpForm.confirmPassword.empty() ? "CLICK TO TYPE" : (signUpForm.showPassword ? getVisiblePasswordText(signUpForm.confirmPassword) : std::string(std::min<std::size_t>(signUpForm.confirmPassword.size(), 34), '*'))),
						confirmTop + 6.0f * fpsCellSize);
				}
				appendCenteredSignUpText(signUpForm.submitting ? "CONNECTING" : (signUpForm.signInMode ? "SIGN IN" : "REGISTER"),
					static_cast<float>(framebufferHeight) * 0.67f, 1.4f, 4.5f);
				appendCenteredSignUpText(signUpForm.signInMode ? "CREATE ACCOUNT" : "SWITCH TO SIGN IN",
					static_cast<float>(framebufferHeight) * 0.75f);
				if (signUpForm.emailConfirmationPending)
				{
					if (!signUpForm.status.empty())
						appendCenteredSignUpText(signUpForm.status, static_cast<float>(framebufferHeight) * 0.89f);
				}
				else
				{
				appendCenteredSignUpText(signUpForm.status.empty() ? "TAB SWITCH ENTER SUBMIT" : signUpForm.status,
						static_cast<float>(framebufferHeight) * 0.96f);
				}
				}
			}
			if (gamePaused && !signUpForm.active && !rankMenuPage && !controlsMenuPage)
			{
				const std::string pauseLabel = "PAUSED";
				float pauseCellSize = fpsCellSize * 2.0f;
				float pauseTop = static_cast<float>(framebufferHeight) * 0.30f;
				auto appendCenteredPauseText = [&](const std::string& text, float cellSize, float top)
				{
					float textWidth = static_cast<float>(text.size() * 4 - 1) * cellSize;
					std::vector<GLfloat> textVertices = buildFpsVertices(
						text, framebufferWidth, framebufferHeight, cellSize, top,
						(static_cast<float>(framebufferWidth) - textWidth) * 0.5f);
					fpsVertices.insert(fpsVertices.end(), textVertices.begin(), textVertices.end());
				};
				appendCenteredPauseText(pauseLabel, pauseCellSize, pauseTop);
				appendCenteredPauseText("RESUME", fpsCellSize * 1.5f, pauseTop + 18.0f * fpsCellSize);
				appendCenteredPauseText("CONTROLS", fpsCellSize * 1.5f, pauseTop + 40.0f * fpsCellSize);
				appendCenteredPauseText("RANKS", fpsCellSize * 1.5f, pauseTop + 62.0f * fpsCellSize);
				appendCenteredPauseText("SCENARIO RANKS", fpsCellSize * 1.5f, pauseTop + 84.0f * fpsCellSize);
				appendCenteredPauseText(nameEditor.active ? "NAME " + nameEditor.text + "_" : std::string("NAME"), fpsCellSize * 1.5f, pauseTop + 104.0f * fpsCellSize);
				appendCenteredPauseText("EXIT", fpsCellSize * 1.5f, pauseTop + 124.0f * fpsCellSize);
			}
			if (controlsMenuPage)
			{
				float settingsCellSize = std::max(2.0f, static_cast<float>(framebufferHeight) / 300.0f);
				auto appendCenteredSettingsText = [&](const std::string& text, float cellSize, float top)
				{
					float width = static_cast<float>(text.size() * 4 - 1) * cellSize;
					std::vector<GLfloat> vertices = buildFpsVertices(text, framebufferWidth, framebufferHeight,
						cellSize, top, (static_cast<float>(framebufferWidth) - width) * 0.5f);
					fpsVertices.insert(fpsVertices.end(), vertices.begin(), vertices.end());
				};
				float settingsTitleSize = settingsCellSize * 1.5f;
				appendCenteredSettingsText("CONTROLS", settingsTitleSize, static_cast<float>(framebufferHeight) * 0.10f);
				appendCenteredSettingsText("BACK", settingsTitleSize, static_cast<float>(framebufferHeight) - 48.0f * settingsCellSize);
				const std::array<std::string, 7> controlsText = {
					"WASD MOVE", "MOUSE LOOK", "SPACE JUMP", "LMB SHOOT",
					"X/Z BALL", "F FULLSCREEN", "ESC RESUME"
				};
				float controlRowTop = static_cast<float>(framebufferHeight) * 0.23f;
				float controlRowSpacing = settingsCellSize * 5.5f;
				for (std::size_t controlIndex = 0; controlIndex < controlsText.size(); ++controlIndex)
					appendCenteredSettingsText(controlsText[controlIndex], settingsCellSize,
						controlRowTop + static_cast<float>(controlIndex) * controlRowSpacing);

				float sliderLeft = static_cast<float>(framebufferWidth) * 0.5f - 55.0f * settingsCellSize;
				float sliderWidth = 110.0f * settingsCellSize;
				float sensitivityLabelTop = static_cast<float>(framebufferHeight) * 0.60f;
				appendCenteredSettingsText("SENSITIVITY: " + std::to_string(static_cast<int>(std::round((camera.sensitivity - 0.02f) / 0.28f * 100.0f))),
					settingsCellSize, sensitivityLabelTop);
				float sensitivitySliderY = sensitivityLabelTop + 7.0f * settingsCellSize;
				float volumeLabelTop = sensitivityLabelTop + 14.0f * settingsCellSize;
				appendCenteredSettingsText("VOLUME: " + std::to_string(static_cast<int>(std::round(audioVolume * 100.0f))),
					settingsCellSize, volumeLabelTop);
				float volumeSliderY = volumeLabelTop + 7.0f * settingsCellSize;
				float targetSizeLabelTop = volumeLabelTop + 11.0f * settingsCellSize;
				appendCenteredSettingsText("BALL SIZE: " + std::to_string(static_cast<int>(std::round(aimTargetSize * 100.0f))),
					settingsCellSize, targetSizeLabelTop);
				float targetSizeSliderY = targetSizeLabelTop + 7.0f * settingsCellSize;
				for (int tick = 0; tick <= 40; ++tick)
				{
					float pixelX = sliderLeft + sliderWidth * static_cast<float>(tick) / 40.0f;
					fpsVertices.push_back(2.0f * pixelX / static_cast<float>(framebufferWidth) - 1.0f);
					fpsVertices.push_back(1.0f - 2.0f * sensitivitySliderY / static_cast<float>(framebufferHeight));
					fpsVertices.push_back(2.0f * pixelX / static_cast<float>(framebufferWidth) - 1.0f);
					fpsVertices.push_back(1.0f - 2.0f * volumeSliderY / static_cast<float>(framebufferHeight));
					fpsVertices.push_back(2.0f * pixelX / static_cast<float>(framebufferWidth) - 1.0f);
					fpsVertices.push_back(1.0f - 2.0f * targetSizeSliderY / static_cast<float>(framebufferHeight));
				}
				auto appendSliderKnob = [&](float value, float sliderY)
				{
					float knobX = sliderLeft + sliderWidth * value;
					std::vector<GLfloat> vertices = buildFpsVertices("I", framebufferWidth, framebufferHeight,
						settingsCellSize * 2.0f, sliderY - 5.0f * settingsCellSize, knobX - 3.0f * settingsCellSize);
					fpsVertices.insert(fpsVertices.end(), vertices.begin(), vertices.end());
				};
				appendSliderKnob((camera.sensitivity - 0.02f) / 0.28f, sensitivitySliderY);
				appendSliderKnob(audioVolume, volumeSliderY);
				appendSliderKnob((aimTargetSize - 0.25f) / 0.75f, targetSizeSliderY);
			}
			glm::vec3 hudPlayerBallPosition = camera.Position - glm::vec3(0.0f, cameraToPlayerBallOffset, 0.0f);
			bool trainerHudVisible = !gamePaused &&
				hudPlayerBallPosition.x >= aimTrainerMinX && hudPlayerBallPosition.x <= aimTrainerMaxX &&
				hudPlayerBallPosition.z >= aimTrainerMinZ && hudPlayerBallPosition.z <= aimTrainerMaxZ;
			if (trainerHudVisible)
			{
				const std::array<std::string, 3> scenarioLabels = {
					!sixBallsScenario && !twoBallsScenario ? "> 1 THREE BALLS" : "  1 THREE BALLS",
					twoBallsScenario ? "> 2 TWO BALLS" : "  2 TWO BALLS",
					sixBallsScenario ? "> 4 SIX BALLS" : "  4 SIX BALLS"
				};
				float selectorCellSize = fpsCellSize;
				float selectorTop = 16.0f + 24.0f * fpsCellSize;
				float selectorRowSpacing = 6.0f * selectorCellSize;
				for (std::size_t selectorIndex = 0; selectorIndex < scenarioLabels.size(); ++selectorIndex)
				{
					float labelWidth = static_cast<float>(scenarioLabels[selectorIndex].size() * 4 - 1) * selectorCellSize;
					std::vector<GLfloat> vertices = buildFpsVertices(scenarioLabels[selectorIndex],
						framebufferWidth, framebufferHeight, selectorCellSize,
						selectorTop + static_cast<float>(selectorIndex) * selectorRowSpacing,
						static_cast<float>(framebufferWidth) - labelWidth - 16.0f);
					fpsVertices.insert(fpsVertices.end(), vertices.begin(), vertices.end());
				}
			}
			if (rankMenuPage)
			{
				float menuCellSize = std::max(2.0f, static_cast<float>(framebufferHeight) / 300.0f);
				std::vector<GLfloat> backVertices = buildFpsVertices("BACK", framebufferWidth, framebufferHeight,
					menuCellSize * 1.5f, static_cast<float>(framebufferHeight) - 48.0f * menuCellSize,
					(static_cast<float>(framebufferWidth) - 17.0f * menuCellSize) * 0.5f);
				fpsVertices.insert(fpsVertices.end(), backVertices.begin(), backVertices.end());
				if (showingScenarioRanks)
				{
					auto appendCenteredScenarioText = [&](const std::string& text, float cellSize, float top)
					{
						float width = static_cast<float>(text.size() * 4 - 1) * cellSize;
						std::vector<GLfloat> vertices = buildFpsVertices(text, framebufferWidth, framebufferHeight,
							cellSize, top, (static_cast<float>(framebufferWidth) - width) * 0.5f);
						fpsVertices.insert(fpsVertices.end(), vertices.begin(), vertices.end());
					};
					appendCenteredScenarioText(sixBallsScenario ? "SIX BALLS" :
						(twoBallsScenario ? "TWO BALLS" : "THREE BALLS"), menuCellSize * 2.0f, static_cast<float>(framebufferHeight) * 0.28f);
					appendCenteredScenarioText("HIGH SCORE", menuCellSize, static_cast<float>(framebufferHeight) * 0.38f);
					std::string scenarioRankName = getCurrentAimRank().name;
					float rankTextWidth = static_cast<float>(scenarioRankName.size() * 4 - 1) * menuCellSize * 2.0f;
					std::vector<GLfloat> scenarioRankVertices = buildFpsVertices(scenarioRankName, framebufferWidth, framebufferHeight,
						menuCellSize * 2.0f, static_cast<float>(framebufferHeight) * 0.48f,
						(static_cast<float>(framebufferWidth) - rankTextWidth) * 0.5f);
					rankVertexStart = static_cast<GLint>(fpsVertices.size() / 2);
					rankVertexCount = static_cast<GLsizei>(scenarioRankVertices.size() / 2);
					currentRankColor = getCurrentAimRank().color;
					fpsVertices.insert(fpsVertices.end(), scenarioRankVertices.begin(), scenarioRankVertices.end());
					appendCenteredScenarioText("SCORE " + std::to_string(sixBallsScenario ? sixBallsHighScore :
						(twoBallsScenario ? twoBallsHighScore : aimHighScore)), menuCellSize, static_cast<float>(framebufferHeight) * 0.60f);
				}
			}
			glBindBuffer(GL_ARRAY_BUFFER, fpsVBO);
			glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(fpsVertices.size() * sizeof(GLfloat)), fpsVertices.data(), GL_DYNAMIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		if (!gamePaused)
			camera.Inputs(true);

		bool cameraJumpPressed = !gamePaused && platform::isKeyDown(platform::Key::Space);
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
#ifdef _WIN32
				PlaySoundA(reinterpret_cast<LPCSTR>(adjustedJumpSound.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#else
				playPortableSound(false);
#endif
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
			bool forwardPressed = platform::isKeyDown(platform::Key::W);
			bool backwardPressed = platform::isKeyDown(platform::Key::S);
			bool leftPressed = platform::isKeyDown(platform::Key::A);
			bool rightPressed = platform::isKeyDown(platform::Key::D);
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

		bool scenarioTeleportPressed = !gamePaused && platform::isKeyDown(platform::Key::J);
		if (scenarioTeleportPressed && !previousScenarioTeleportKey)
		{
			const glm::vec3 scenarioCenter(0.0f, ballGroundY, (aimTrainerMinZ + aimTrainerMaxZ) * 0.5f);
			balls[playerBallIndex].position = scenarioCenter;
			balls[playerBallIndex].velocity = glm::vec3(0.0f);
			camera.Position = scenarioCenter + glm::vec3(0.0f, cameraToPlayerBallOffset, 0.0f);
			previousCameraPosition = camera.Position;
			camera.horizontalVelocity = glm::vec3(0.0f);
			cameraVerticalVelocity = 0.0f;
			aimTaskRunning = false;
			aimTaskFinished = false;
			aimTaskScore = 0;
		}
		previousScenarioTeleportKey = scenarioTeleportPressed;

		bool xPressed = !gamePaused && platform::isKeyDown(platform::Key::X);
		if (xPressed && !previousX)
			activeBall = (activeBall + 1) % static_cast<int>(balls.size());
		previousX = xPressed;
		bool zPressed = !gamePaused && platform::isKeyDown(platform::Key::Z);
		if (zPressed && !previousZ)
			activeBall = (activeBall + static_cast<int>(balls.size()) - 1) % static_cast<int>(balls.size());
		previousZ = zPressed;
		bool twoBallsPressed = !gamePaused && platform::isKeyDown(platform::Key::Num2);
		bool threeBallsPressed = !gamePaused && platform::isKeyDown(platform::Key::Num1);
		bool sixBallsPressed = !gamePaused && platform::isKeyDown(platform::Key::Num4);
		if (twoBallsPressed && !previousTwoBallsKey)
		{
			twoBallsScenario = true;
			sixBallsScenario = false;
			aimTaskRunning = false;
			aimTaskFinished = false;
			aimTaskScore = 0;
		}
		else if (threeBallsPressed && !previousThreeBallsKey)
		{
			twoBallsScenario = false;
			sixBallsScenario = false;
			aimTaskRunning = false;
			aimTaskFinished = false;
			aimTaskScore = 0;
		}
		else if (sixBallsPressed && !previousSixBallsKey)
		{
			twoBallsScenario = false;
			sixBallsScenario = true;
			aimTaskRunning = false;
			aimTaskFinished = false;
			aimTaskScore = 0;
		}
		previousTwoBallsKey = twoBallsPressed;
		previousThreeBallsKey = threeBallsPressed;
		previousSixBallsKey = sixBallsPressed;

		bool crosshairKeyPressed = platform::isKeyDown(platform::Key::C);
		if (crosshairKeyPressed && !previousCrosshairKey)
			crosshairStyle = (crosshairStyle + 1) % 4;
		previousCrosshairKey = crosshairKeyPressed;
		bool ranksKeyPressed = platform::isKeyDown(platform::Key::R);
		if (ranksKeyPressed && !previousRanksKey && !gamePaused)
		{
			gamePaused = true;
			showingControls = false;
			showingRanks = true;
			showingScenarioRanks = false;
			platform::setCursorCaptured(false);
			pauseStartedAt = platform::getTime();
			previousCameraPosition = camera.Position;
		}
		previousRanksKey = ranksKeyPressed;

		for (std::size_t ballIndex = 0; !gamePaused && ballIndex < balls.size(); ++ballIndex)
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
		if (!gamePaused)
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
		if (!gamePaused && aimTrainerActive && !aimTaskRunning &&
			(!aimTaskFinished || currentTime - aimTaskFinishedTime >= aimTaskRetryCooldownSeconds))
		{
			aimTaskScore = 0;
			aimTaskFinished = false;
			aimTaskRunning = true;
			aimTaskStartTime = currentTime;
			if (sixBallsScenario)
				randomizeSixBallTargets();
		}
		if (!gamePaused)
			wasAimTrainerActive = aimTrainerActive;
		if (!gamePaused && aimTaskRunning && twoBallsScenario)
		{
			for (std::size_t targetIndex = 0; targetIndex < twoBallTargets.size(); ++targetIndex)
			{
				glm::vec3& target = twoBallTargets[targetIndex];
				target.x += twoBallTargetDirections[targetIndex] * twoBallTargetSpeed * deltaTime;
				if (target.x >= twoBallTargetMaxX)
				{
					target.x = twoBallTargetMaxX;
					twoBallTargetDirections[targetIndex] = -1.0f;
				}
				else if (target.x <= twoBallTargetMinX)
				{
					target.x = twoBallTargetMinX;
					twoBallTargetDirections[targetIndex] = 1.0f;
				}
			}
		}
		if (!gamePaused && aimTaskRunning && aimTaskClockTime - aimTaskStartTime >= aimTaskDurationSeconds)
		{
			aimTaskRunning = false;
			aimTaskFinished = true;
			aimTaskFinishedTime = currentTime;
			if (sixBallsScenario)
				completedSixBallsOnce = true;
			else if (twoBallsScenario)
				completedTwoBallsOnce = true;
			else
				completedThreeBallsOnce = true;
			int& scenarioHighScore = sixBallsScenario ? sixBallsHighScore :
				(twoBallsScenario ? twoBallsHighScore : aimHighScore);
			const fs::path& scenarioHighScorePath = sixBallsScenario ? sixBallsHighScorePath :
				(twoBallsScenario ? twoBallsHighScorePath : aimHighScorePath);
			if (aimTaskScore > scenarioHighScore)
			{
				scenarioHighScore = aimTaskScore;
				saveAimHighScore(scenarioHighScorePath, scenarioHighScore);
				if (!authenticatedSessionToken.empty())
				{
					std::string sessionToken = authenticatedSessionToken;
					int aimBestScore = aimHighScore;
					int twoBallsBestScore = twoBallsHighScore;
					int sixBallsBestScore = sixBallsHighScore;
					try
					{
						rankSyncThreads.emplace_back([sessionToken = std::move(sessionToken), aimBestScore,
							twoBallsBestScore, sixBallsBestScore]() mutable
						{
							sendRankScores(sessionToken, aimBestScore, twoBallsBestScore, sixBallsBestScore);
						});
					}
					catch (const std::system_error&)
					{
					}
				}
			}
		}
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

		bool leftMousePressed = platform::isLeftMouseDown();
		bool scenarioSelectorClicked = false;
		if (!gamePaused && aimTrainerActive && leftMousePressed && !previousLeftMouse)
		{
			int selectorFramebufferWidth = 0;
			int selectorFramebufferHeight = 0;
			platform::getFramebufferSize(selectorFramebufferWidth, selectorFramebufferHeight);
			double cursorX = 0.0;
			double cursorY = 0.0;
			platform::getCursorPosition(cursorX, cursorY);
			if (selectorFramebufferWidth > 0 && selectorFramebufferHeight > 0)
			{
				float clickX = static_cast<float>(cursorX);
				float clickY = static_cast<float>(cursorY);
				float selectorCellSize = fpsCellSize;
				float selectorTop = 16.0f + 24.0f * selectorCellSize;
				float selectorRowSpacing = 6.0f * selectorCellSize;
				int clickedRow = static_cast<int>(std::round((clickY - selectorTop) / selectorRowSpacing));
				float clickedRowTop = selectorTop + static_cast<float>(clickedRow) * selectorRowSpacing;
				const std::array<std::string, 3> selectorLabels = {
					"  1 THREE BALLS", "  2 TWO BALLS", "  4 SIX BALLS"
				};
				float rowLabelWidth = clickedRow >= 0 && clickedRow < 3
					? static_cast<float>(selectorLabels[static_cast<std::size_t>(clickedRow)].size() * 4 - 1) * selectorCellSize
					: 0.0f;
				float rowLabelLeft = (static_cast<float>(selectorFramebufferWidth) - rowLabelWidth) * 0.5f;
				if (clickedRow >= 0 && clickedRow < 3 &&
					clickY >= clickedRowTop - 5.0f * selectorCellSize &&
					clickY <= clickedRowTop + selectorCellSize &&
					clickX >= rowLabelLeft - 2.0f * selectorCellSize &&
					clickX <= rowLabelLeft + rowLabelWidth + 2.0f * selectorCellSize)
				{
					if (clickedRow == 0 && (twoBallsScenario || sixBallsScenario))
					{
						twoBallsScenario = false;
						sixBallsScenario = false;
						scenarioSelectorClicked = true;
					}
					else if (clickedRow == 1 && !twoBallsScenario)
					{
						twoBallsScenario = true;
						sixBallsScenario = false;
						scenarioSelectorClicked = true;
					}
					else if (clickedRow == 2 && !sixBallsScenario)
					{
						twoBallsScenario = false;
						sixBallsScenario = true;
						scenarioSelectorClicked = true;
					}
					if (scenarioSelectorClicked)
					{
						aimTaskRunning = false;
						aimTaskFinished = false;
						aimTaskScore = 0;
					}
				}
			}
		}
		if (!gamePaused && aimTrainerActive && aimTaskRunning && leftMousePressed && !previousLeftMouse)
		{
			if (!scenarioSelectorClicked)
			{
				glm::vec3 shotDirection = glm::normalize(camera.Orientation);
				float closestHitDistance = std::numeric_limits<float>::max();
				int hitTargetIndex = -1;
				std::size_t activeTargetCount = sixBallsScenario ? sixBallTargets.size() :
					(twoBallsScenario ? twoBallTargets.size() : aimTargets.size());
				for (std::size_t targetIndex = 0; targetIndex < activeTargetCount; ++targetIndex)
				{
					if (sixBallsScenario && !sixBallTargetVisible[targetIndex])
						continue;
					glm::vec3 targetPosition = sixBallsScenario ? sixBallTargets[targetIndex] :
						(twoBallsScenario ? twoBallTargets[targetIndex] : aimTargets[targetIndex]);
					glm::vec3 toTarget = targetPosition - camera.Position;
					float alongRay = glm::dot(toTarget, shotDirection);
					if (alongRay <= 0.0f || alongRay >= closestHitDistance)
						continue;

					float perpendicularDistanceSquared = glm::dot(toTarget, toTarget) - alongRay * alongRay;
					float targetHitRadius = ballRadius * aimTargetSize *
						(sixBallsScenario ? sixBallTargetSizeMultiplier * 1.5f : 1.0f);
					if (perpendicularDistanceSquared <= targetHitRadius * targetHitRadius)
					{
						closestHitDistance = alongRay;
						hitTargetIndex = static_cast<int>(targetIndex);
					}
				}
				if (hitTargetIndex >= 0)
				{
					++aimTaskScore;
					if (sixBallsScenario)
						respawnSixBallTarget(static_cast<std::size_t>(hitTargetIndex));
					else
					{
						if (twoBallsScenario)
							respawnTwoBallTarget(static_cast<std::size_t>(hitTargetIndex));
						else
							respawnAimTarget(static_cast<std::size_t>(hitTargetIndex));
					}
#ifdef _WIN32
					PlaySoundA(reinterpret_cast<LPCSTR>(adjustedAimTargetHitSound.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#else
					playPortableSound(true);
#endif
				}
			}
		}
		previousLeftMouse = leftMousePressed;

		glm::mat4 skyboxProjection = glm::perspective(glm::radians(45.0f),
			static_cast<float>(camera.width) / static_cast<float>(camera.height), 0.1f, 500.0f);
		glm::mat4 skyboxView = glm::lookAt(glm::vec3(0.0f), camera.Orientation, camera.Up);
		float dayCyclePhase = static_cast<float>(std::fmod(platform::getTime() - dayNightStartTime, dayNightCycleSeconds) / dayNightCycleSeconds);
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
		glUniform1f(skyboxCloudTimeLocation, static_cast<float>(platform::getTime() - dayNightStartTime));
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
		if (sixBallsScenario)
		{
			for (std::size_t targetIndex = 0; targetIndex < sixBallTargets.size(); ++targetIndex)
			{
				if (sixBallTargetVisible[targetIndex])
					orbModel.DrawWithTint(shaderProgram, camera, sixBallTargets[targetIndex],
						glm::vec3(1.0f, 0.12f, 0.12f), glm::vec3(aimTargetSize * sixBallTargetSizeMultiplier));
			}
		}
		else if (twoBallsScenario)
		{
			for (const glm::vec3& targetPosition : twoBallTargets)
				orbModel.DrawWithTint(shaderProgram, camera, targetPosition, glm::vec3(1.0f, 0.12f, 0.12f), glm::vec3(aimTargetSize));
		}
		else
		{
			for (const glm::vec3& targetPosition : aimTargets)
				orbModel.DrawWithTint(shaderProgram, camera, targetPosition, glm::vec3(1.0f, 0.12f, 0.12f), glm::vec3(aimTargetSize));
		}
		camera.Position = simulationCameraPosition;
		camera.Orientation = simulationOrientation;

		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glUseProgram(crosshairProgram);
		int crosshairWidth = 0;
		int crosshairHeight = 0;
		platform::getFramebufferSize(crosshairWidth, crosshairHeight);
		float crosshairAspect = crosshairHeight > 0
			? static_cast<float>(crosshairWidth) / static_cast<float>(crosshairHeight)
			: 1.0f;
		if (!gamePaused)
		{
			glUniform1f(crosshairAspectLocation, crosshairAspect);
			glBindVertexArray(crosshairVAO);
			glUniform4f(crosshairColorLocation, 0.0f, 1.0f, 1.0f, 1.0f);
			glLineWidth(1.5f);
			if (crosshairStyle == 1)
			{
				glUniform1f(crosshairPointSizeLocation, 6.0f);
				glDrawArrays(GL_POINTS, 4, 1);
				glUniform1f(crosshairPointSizeLocation, 1.0f);
			}
			else if (crosshairStyle == 2)
				glDrawArrays(GL_LINE_LOOP, 5, 24);
			else if (crosshairStyle == 3)
				glDrawArrays(GL_LINES, 29, 8);
			else
				glDrawArrays(GL_LINES, 0, 4);
			glBindVertexArray(0);
		}
		glUniform1f(crosshairAspectLocation, 1.0f);
		glUniform1f(crosshairPointSizeLocation, 2.0f);
		glUniform4f(crosshairColorLocation, 0.0f, 0.0f, 0.0f, 0.85f);
		glBindVertexArray(fpsVAO);
		glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(fpsVertices.size() / 2));
		glUniform1f(crosshairPointSizeLocation, 1.0f);
		glUniform4f(crosshairColorLocation, 1.0f, 1.0f, 1.0f, 1.0f);
		glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(fpsVertices.size() / 2));
		glUniform1f(crosshairPointSizeLocation, 1.0f);
		for (std::size_t rankIndex = 0; rankIndex < rankLadderVertexStarts.size(); ++rankIndex)
		{
			if (rankLadderVertexCounts[rankIndex] == 0)
				continue;
			glUniform4fv(crosshairColorLocation, 1, glm::value_ptr(rankLadderColors[rankIndex]));
			glDrawArrays(GL_POINTS, rankLadderVertexStarts[rankIndex], rankLadderVertexCounts[rankIndex]);
		}
		if (rankVertexCount > 0)
		{
			glUniform4fv(crosshairColorLocation, 1, glm::value_ptr(currentRankColor));
			glDrawArrays(GL_POINTS, rankVertexStart, rankVertexCount);
		}
		glBindVertexArray(0);
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);

		platform::swapBuffers();
		platform::pollEvents();
	};

	if (signUpAuthThread.joinable())
		signUpAuthThread.join();
	for (std::thread& rankSyncThread : rankSyncThreads)
		if (rankSyncThread.joinable())
			rankSyncThread.join();
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
	platform::destroyWindow();
	return 0;
};

void processInput()
{
	static bool fullscreenWasPressed = false;
	static int windowedX = 100;
	static int windowedY = 100;
	static int windowedWidth = defaultWidth;
	static int windowedHeight = defaultHeight;

	bool fullscreenPressed = platform::isKeyDown(platform::Key::F);
	if (fullscreenPressed && !fullscreenWasPressed)
		platform::toggleFullscreen();
	fullscreenWasPressed = fullscreenPressed;
};

void framebuffer_size_callback(int width, int height)
{
	glViewport(0, 0, width, height);
	if (width > 0 && height > 0 && cameraForFramebufferCallback != nullptr)
	{
		cameraForFramebufferCallback->width = width;
		cameraForFramebufferCallback->height = height;
	}
}

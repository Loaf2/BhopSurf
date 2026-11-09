#ifdef __ANDROID__

#include "PlatformWindow.h"

#include <android/input.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace platform
{
namespace
{
android_app* g_app = nullptr;
EGLDisplay g_display = EGL_NO_DISPLAY;
EGLSurface g_surface = EGL_NO_SURFACE;
EGLContext g_context = EGL_NO_CONTEXT;
int g_width = 0;
int g_height = 0;
bool g_leftMouseDown = false;
double g_pointerX = 0.0;
double g_pointerY = 0.0;
double g_lookX = 0.0;
double g_lookY = 0.0;
std::array<bool, static_cast<size_t>(Key::Count)> g_keys{};
std::array<bool, static_cast<size_t>(Key::Count)> g_touchKeys{};
bool g_touchShoot = false;
int32_t g_lookPointerId = -1;
float g_lastLookX = 0.0f;
float g_lastLookY = 0.0f;
GLuint g_overlayProgram = 0;
GLuint g_overlayVao = 0;
GLuint g_overlayVbo = 0;
TextCallback g_textCallback = nullptr;
KeyCallback g_keyCallback = nullptr;
enum class GuideLanguage { English, Spanish };
bool g_guideOpen = true;
bool g_guideLanguageChosen = false;
bool g_scenarioMenuOpen = false;
GuideLanguage g_guideLanguage = GuideLanguage::English;

unsigned int mapTextCodepoint(int32_t keyCode)
{
	if (keyCode >= AKEYCODE_A && keyCode <= AKEYCODE_Z)
		return static_cast<unsigned int>('a' + (keyCode - AKEYCODE_A));
	if (keyCode >= AKEYCODE_0 && keyCode <= AKEYCODE_9)
		return static_cast<unsigned int>('0' + (keyCode - AKEYCODE_0));
	switch (keyCode)
	{
	case AKEYCODE_SPACE: return ' ';
	case AKEYCODE_MINUS: return '-';
	case AKEYCODE_PERIOD: return '.';
	case AKEYCODE_AT: return '@';
	case AKEYCODE_PLUS: return '+';
	default: return 0;
	}
}

Key mapKeyCode(int32_t keyCode, bool& mapped)
{
	mapped = true;
	switch (keyCode)
	{
	case AKEYCODE_ESCAPE: return Key::Escape;
	case AKEYCODE_SPACE: return Key::Space;
	case AKEYCODE_TAB: return Key::Tab;
	case AKEYCODE_ENTER: return Key::Enter;
	case AKEYCODE_DEL: return Key::Backspace;
	case AKEYCODE_SHIFT_LEFT: return Key::LeftShift;
	case AKEYCODE_SHIFT_RIGHT: return Key::RightShift;
	case AKEYCODE_CTRL_LEFT:
	case AKEYCODE_CTRL_RIGHT: return Key::Control;
	case AKEYCODE_V: return Key::V;
	case AKEYCODE_W: return Key::W;
	case AKEYCODE_A: return Key::A;
	case AKEYCODE_S: return Key::S;
	case AKEYCODE_D: return Key::D;
	case AKEYCODE_X: return Key::X;
	case AKEYCODE_Z: return Key::Z;
	case AKEYCODE_F: return Key::F;
	case AKEYCODE_J: return Key::J;
	case AKEYCODE_2: return Key::Num2;
	case AKEYCODE_1: return Key::Num1;
	case AKEYCODE_3: return Key::Num3;
	case AKEYCODE_4: return Key::Num4;
	case AKEYCODE_5: return Key::Num5;
	case AKEYCODE_C: return Key::C;
	case AKEYCODE_R: return Key::R;
	default: mapped = false; return Key::Count;
	}
}

struct VirtualButton
{
	float x;
	float y;
	float radius;
	Key key;
	bool shoot;
	char label;
};

std::array<VirtualButton, 7> virtualButtons()
{
	const float w = static_cast<float>(g_width);
	const float h = static_cast<float>(g_height);
	const float step = h * 0.12f;
	const float radius = step * 0.45f;
	const float dpadX = w * 0.14f;
	const float dpadY = h * 0.72f;
	return {{
		{ dpadX, dpadY - step, radius, Key::W, false, 'W' },
		{ dpadX - step, dpadY, radius, Key::A, false, 'A' },
		{ dpadX, dpadY + step, radius, Key::S, false, 'S' },
		{ dpadX + step, dpadY, radius, Key::D, false, 'D' },
		{ w * 0.78f, h * 0.72f, radius, Key::Space, false, 'O' },
		{ w * 0.90f, h * 0.72f, radius, Key::Count, true, 'F' },
		{ w * 0.90f, h * 0.50f, radius, Key::J, false, 'J' }
	}};
}

std::vector<std::array<int, 4>> labelSegments(char label)
{
	switch (label)
	{
	case 'W': return {{0, 0, 0, 4}, {2, 0, 2, 4}, {0, 4, 1, 2}, {1, 2, 2, 4}};
	case 'A': return {{0, 4, 0, 0}, {0, 0, 2, 0}, {2, 0, 2, 4}, {0, 2, 2, 2}};
	case 'S': return {{2, 0, 0, 0}, {0, 0, 0, 2}, {0, 2, 2, 2}, {2, 2, 2, 4}, {2, 4, 0, 4}};
	case 'D': return {{0, 0, 0, 4}, {0, 0, 1, 0}, {1, 0, 2, 1}, {2, 1, 2, 3}, {2, 3, 1, 4}, {1, 4, 0, 4}};
	case 'J': return {{2, 0, 2, 4}, {2, 4, 0, 4}, {0, 4, 0, 3}};
	case 'F': return {{0, 0, 0, 4}, {0, 0, 2, 0}, {0, 2, 2, 2}};
	case 'O': return {{0, 0, 2, 0}, {2, 0, 2, 4}, {2, 4, 0, 4}, {0, 4, 0, 0}};
	case 'B': return {{0, 0, 0, 4}, {0, 0, 2, 0}, {2, 0, 2, 2}, {0, 2, 2, 2}, {2, 2, 2, 4}, {2, 4, 0, 4}};
	case 'C': return {{2, 0, 0, 0}, {0, 0, 0, 4}, {0, 4, 2, 4}};
	case 'E': return {{2, 0, 0, 0}, {0, 0, 0, 4}, {0, 2, 2, 2}, {0, 4, 2, 4}};
	case 'G': return {{2, 0, 0, 0}, {0, 0, 0, 4}, {0, 4, 2, 4}, {2, 4, 2, 2}, {2, 2, 1, 2}};
	case 'H': return {{0, 0, 0, 4}, {2, 0, 2, 4}, {0, 2, 2, 2}};
	case 'I': return {{1, 0, 1, 4}, {0, 0, 2, 0}, {0, 4, 2, 4}};
	case 'K': return {{0, 0, 0, 4}, {2, 0, 0, 2}, {0, 2, 2, 4}};
	case 'L': return {{0, 0, 0, 4}, {0, 4, 2, 4}};
	case 'M': return {{0, 4, 0, 0}, {0, 0, 1, 2}, {1, 2, 2, 0}, {2, 0, 2, 4}};
	case 'N': return {{0, 4, 0, 0}, {0, 0, 2, 4}, {2, 4, 2, 0}};
	case 'P': return {{0, 4, 0, 0}, {0, 0, 2, 0}, {2, 0, 2, 2}, {2, 2, 0, 2}};
	case 'Q': return {{0, 0, 2, 0}, {2, 0, 2, 4}, {2, 4, 0, 4}, {0, 4, 0, 0}, {1, 3, 2, 4}};
	case 'R': return {{0, 4, 0, 0}, {0, 0, 2, 0}, {2, 0, 2, 2}, {2, 2, 0, 2}, {0, 2, 2, 4}};
	case 'T': return {{0, 0, 2, 0}, {1, 0, 1, 4}};
	case 'U': return {{0, 0, 0, 4}, {0, 4, 2, 4}, {2, 4, 2, 0}};
	case 'V': return {{0, 0, 1, 4}, {1, 4, 2, 0}};
	case 'X': return {{0, 0, 2, 4}, {2, 0, 0, 4}};
	case 'Y': return {{0, 0, 1, 2}, {2, 0, 1, 2}, {1, 2, 1, 4}};
	case 'Z': return {{0, 0, 2, 0}, {2, 0, 0, 4}, {0, 4, 2, 4}};
	case '0': return {{0, 0, 2, 0}, {2, 0, 2, 4}, {2, 4, 0, 4}, {0, 4, 0, 0}};
	case '1': return {{1, 0, 1, 4}};
	case '2': return {{0, 0, 2, 0}, {2, 0, 2, 2}, {2, 2, 0, 2}, {0, 2, 0, 4}, {0, 4, 2, 4}};
	case '3': return {{0, 0, 2, 0}, {2, 0, 2, 4}, {0, 2, 2, 2}, {0, 4, 2, 4}};
	case '4': return {{0, 0, 0, 2}, {0, 2, 2, 2}, {2, 0, 2, 4}};
	case '5': return {{2, 0, 0, 0}, {0, 0, 0, 2}, {0, 2, 2, 2}, {2, 2, 2, 4}, {2, 4, 0, 4}};
	case '6': return {{2, 0, 0, 0}, {0, 0, 0, 4}, {0, 4, 2, 4}, {2, 4, 2, 2}, {2, 2, 0, 2}};
	case '7': return {{0, 0, 2, 0}, {2, 0, 1, 4}};
	case '8': return {{0, 0, 2, 0}, {2, 0, 2, 4}, {2, 4, 0, 4}, {0, 4, 0, 0}, {0, 2, 2, 2}};
	case '9': return {{0, 2, 0, 0}, {0, 0, 2, 0}, {2, 0, 2, 4}, {0, 2, 2, 2}, {0, 4, 2, 4}};
	case '-': return {{0, 2, 2, 2}};
	case ':': return {{1, 1, 1, 1}, {1, 3, 1, 3}};
	case '.': return {{1, 4, 1, 4}};
	case '!': return {{1, 0, 1, 3}, {1, 4, 1, 4}};
	case '?': return {{0, 0, 2, 0}, {2, 0, 2, 2}, {2, 2, 1, 2}, {1, 2, 1, 3}, {1, 4, 1, 4}};
	case '+': return {{0, 2, 2, 2}, {1, 1, 1, 3}};
	case '/': return {{2, 0, 0, 4}};
	case '(': return {{1, 0, 0, 2}, {0, 2, 1, 4}};
	case ')': return {{1, 0, 2, 2}, {2, 2, 1, 4}};
	default: return {};
	}
}

GLuint compileOverlayShader(GLenum type, const char* source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
	return shader;
}

void initOverlay()
{
	if (g_overlayProgram != 0)
		return;
	const char* vertexSource = "#version 300 es\nlayout(location = 0) in vec2 position;\nvoid main() { gl_Position = vec4(position, 0.0, 1.0); }\n";
	const char* fragmentSource = "#version 300 es\nprecision mediump float;\nuniform vec4 color;\nout vec4 fragColor;\nvoid main() { fragColor = color; }\n";
	const GLuint vertexShader = compileOverlayShader(GL_VERTEX_SHADER, vertexSource);
	const GLuint fragmentShader = compileOverlayShader(GL_FRAGMENT_SHADER, fragmentSource);
	const GLuint program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	GLint linked = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (linked != GL_TRUE)
	{
		__android_log_print(ANDROID_LOG_ERROR, "AimTrainer", "Failed to link touch overlay program");
		glDeleteProgram(program);
		return;
	}
	g_overlayProgram = program;
	glGenVertexArrays(1, &g_overlayVao);
	glGenBuffers(1, &g_overlayVbo);
	glBindVertexArray(g_overlayVao);
	glBindBuffer(GL_ARRAY_BUFFER, g_overlayVbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindVertexArray(0);
}

void appendGuideText(std::vector<float>& lines, const std::string& text, float x, float y, float cell)
{
	float cursor = x;
	for (const char c : text)
	{
		for (const std::array<int, 4>& segment : labelSegments(c))
		{
			lines.push_back(cursor + static_cast<float>(segment[0]) * cell);
			lines.push_back(y + static_cast<float>(segment[1]) * cell);
			lines.push_back(cursor + static_cast<float>(segment[2]) * cell);
			lines.push_back(y + static_cast<float>(segment[3]) * cell);
		}
		cursor += 4.0f * cell;
	}
}

void drawPixelLines(const std::vector<float>& lines, GLint colorLocation, float red, float green, float blue, float alpha)
{
	if (lines.empty())
		return;
	const float width = static_cast<float>(g_width);
	const float height = static_cast<float>(g_height);
	std::vector<float> ndc(lines.size());
	for (std::size_t i = 0; i < lines.size(); i += 2)
	{
		ndc[i] = lines[i] / width * 2.0f - 1.0f;
		ndc[i + 1] = 1.0f - lines[i + 1] / height * 2.0f;
	}
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(ndc.size() * sizeof(float)), ndc.data(), GL_DYNAMIC_DRAW);
	glUniform4f(colorLocation, red, green, blue, alpha);
	glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(ndc.size() / 2));
}

void appendCenteredText(std::vector<float>& lines, float centerX, float y, float cell, const std::string& text)
{
	const float textWidth = static_cast<float>(text.size()) * 4.0f * cell;
	appendGuideText(lines, text, centerX - textWidth * 0.5f, y, cell);
}

struct GuideRect
{
	float x;
	float y;
	float w;
	float h;

	bool contains(float px, float py) const
	{
		return px >= x && px <= x + w && py >= y && py <= y + h;
	}
};

struct GuideLayout
{
	GuideRect help;
	GuideRect crosshair;
	GuideRect ranks;
	GuideRect scenario;
	GuideRect scenarioAim;
	GuideRect scenarioTwo;
	GuideRect scenarioSix;
	GuideRect english;
	GuideRect spanish;
	GuideRect close;
	GuideRect language;
	float cell;
};

GuideLayout guideLayout()
{
	const float w = static_cast<float>(g_width);
	const float h = static_cast<float>(g_height);
	const float cellByHeight = h * 0.016f;
	const float cellByWidth = w / 220.0f;
	GuideLayout layout{};
	layout.cell = cellByHeight < cellByWidth ? cellByHeight : cellByWidth;
	layout.help = { w * 0.86f, h * 0.04f, w * 0.12f, h * 0.12f };
	layout.crosshair = { w * 0.86f, h * 0.18f, w * 0.12f, h * 0.08f };
	layout.ranks = { w * 0.86f, h * 0.28f, w * 0.12f, h * 0.08f };
	layout.scenario = { w * 0.80f, h * 0.38f, w * 0.18f, h * 0.08f };
	layout.scenarioAim = { w * 0.60f, h * 0.38f, w * 0.18f, h * 0.07f };
	layout.scenarioTwo = { w * 0.60f, h * 0.47f, w * 0.18f, h * 0.07f };
	layout.scenarioSix = { w * 0.60f, h * 0.56f, w * 0.18f, h * 0.07f };
	layout.english = { w * 0.15f, h * 0.45f, w * 0.30f, h * 0.20f };
	layout.spanish = { w * 0.55f, h * 0.45f, w * 0.30f, h * 0.20f };
	layout.close = { w * 0.35f, h * 0.80f, w * 0.12f, h * 0.12f };
	layout.language = { w * 0.53f, h * 0.80f, w * 0.12f, h * 0.12f };
	return layout;
}

std::string guideTitle(GuideLanguage language)
{
	return language == GuideLanguage::English ? "MOBILE GUIDE" : "GUIA MOVIL";
}

std::vector<std::string> guideBody(GuideLanguage language)
{
	if (language == GuideLanguage::English)
		return {
			"LEFT SIDE: W A S D TO MOVE",
			"RIGHT SIDE: DRAG TO LOOK AROUND",
			"O: JUMP",
			"F: SHOOT",
			"J: TELEPORT TO THE AIM AREA",
			"TAP THE SCREEN TO SHOOT",
			"CLOSE THIS GUIDE WITH THE CLOSE BUTTON",
			"TAP HELP ANYTIME TO OPEN IT AGAIN"
		};
	return {
		"LADO IZQUIERDO: W A S D PARA MOVER",
		"LADO DERECHO: ARRASTRA PARA MIRAR",
		"O: SALTAR",
		"F: DISPARAR",
		"J: TELETRANSPORTE A LA ZONA DE TIRO",
		"TOCA LA PANTALLA PARA DISPARAR",
		"CIERRA ESTA GUIA CON EL BOTON CERRAR",
		"TOCA AYUDA EN CUALQUIER MOMENTO"
	};
}

void drawPixelRect(float x, float y, float rectWidth, float rectHeight, GLint colorLocation, float red, float green, float blue, float alpha)
{
	const float width = static_cast<float>(g_width);
	const float height = static_cast<float>(g_height);
	const std::array<float, 8> corners = {
		x / width * 2.0f - 1.0f, 1.0f - y / height * 2.0f,
		(x + rectWidth) / width * 2.0f - 1.0f, 1.0f - y / height * 2.0f,
		(x + rectWidth) / width * 2.0f - 1.0f, 1.0f - (y + rectHeight) / height * 2.0f,
		x / width * 2.0f - 1.0f, 1.0f - (y + rectHeight) / height * 2.0f
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners.data(), GL_DYNAMIC_DRAW);
	glUniform4f(colorLocation, red, green, blue, alpha);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void drawGuide(GLint colorLocation)
{
	const GuideLayout layout = guideLayout();
	const float w = static_cast<float>(g_width);
	const float h = static_cast<float>(g_height);
	const float cell = layout.cell;
	std::vector<float> text;

	if (!g_guideOpen)
	{
		drawPixelRect(layout.help.x, layout.help.y, layout.help.w, layout.help.h, colorLocation, 1.0f, 1.0f, 1.0f, 0.3f);
		appendCenteredText(text, layout.help.x + layout.help.w * 0.5f, layout.help.y + layout.help.h * 0.5f - 2.0f * cell, cell, "HELP");
		drawPixelRect(layout.crosshair.x, layout.crosshair.y, layout.crosshair.w, layout.crosshair.h, colorLocation, 1.0f, 1.0f, 1.0f, 0.3f);
		appendCenteredText(text, layout.crosshair.x + layout.crosshair.w * 0.5f, layout.crosshair.y + layout.crosshair.h * 0.5f - 2.0f * cell, cell, "CROSS");
		drawPixelRect(layout.ranks.x, layout.ranks.y, layout.ranks.w, layout.ranks.h, colorLocation, 1.0f, 1.0f, 1.0f, 0.3f);
		appendCenteredText(text, layout.ranks.x + layout.ranks.w * 0.5f, layout.ranks.y + layout.ranks.h * 0.5f - 2.0f * cell, cell, "RANKS");
		drawPixelRect(layout.scenario.x, layout.scenario.y, layout.scenario.w, layout.scenario.h, colorLocation, 1.0f, 1.0f, 1.0f, 0.3f);
		appendCenteredText(text, layout.scenario.x + layout.scenario.w * 0.5f, layout.scenario.y + layout.scenario.h * 0.5f - 2.0f * cell, cell, "SCENARIO");
		if (g_scenarioMenuOpen)
		{
			drawPixelRect(layout.scenarioAim.x, layout.scenarioAim.y, layout.scenarioAim.w, layout.scenarioAim.h, colorLocation, 1.0f, 1.0f, 1.0f, 0.3f);
			appendCenteredText(text, layout.scenarioAim.x + layout.scenarioAim.w * 0.5f, layout.scenarioAim.y + layout.scenarioAim.h * 0.5f - 2.0f * cell, cell, "AIM");
			drawPixelRect(layout.scenarioTwo.x, layout.scenarioTwo.y, layout.scenarioTwo.w, layout.scenarioTwo.h, colorLocation, 1.0f, 1.0f, 1.0f, 0.3f);
			appendCenteredText(text, layout.scenarioTwo.x + layout.scenarioTwo.w * 0.5f, layout.scenarioTwo.y + layout.scenarioTwo.h * 0.5f - 2.0f * cell, cell, "2 BALLS");
			drawPixelRect(layout.scenarioSix.x, layout.scenarioSix.y, layout.scenarioSix.w, layout.scenarioSix.h, colorLocation, 1.0f, 1.0f, 1.0f, 0.3f);
			appendCenteredText(text, layout.scenarioSix.x + layout.scenarioSix.w * 0.5f, layout.scenarioSix.y + layout.scenarioSix.h * 0.5f - 2.0f * cell, cell, "6 BALLS");
		}
		drawPixelLines(text, colorLocation, 1.0f, 1.0f, 1.0f, 1.0f);
		return;
	}

	drawPixelRect(0.0f, 0.0f, w, h, colorLocation, 0.0f, 0.0f, 0.0f, 0.85f);
	if (!g_guideLanguageChosen)
	{
		drawPixelRect(layout.english.x, layout.english.y, layout.english.w, layout.english.h, colorLocation, 1.0f, 1.0f, 1.0f, 0.25f);
		drawPixelRect(layout.spanish.x, layout.spanish.y, layout.spanish.w, layout.spanish.h, colorLocation, 1.0f, 1.0f, 1.0f, 0.25f);
		appendCenteredText(text, w * 0.5f, h * 0.20f, cell, "CHOOSE LANGUAGE");
		appendCenteredText(text, w * 0.5f, h * 0.30f, cell, "ELIJE IDIOMA");
		appendCenteredText(text, layout.english.x + layout.english.w * 0.5f, layout.english.y + layout.english.h * 0.5f - 2.0f * cell, cell, "ENGLISH");
		appendCenteredText(text, layout.spanish.x + layout.spanish.w * 0.5f, layout.spanish.y + layout.spanish.h * 0.5f - 2.0f * cell, cell, "ESPANOL");
	}
	else
	{
		drawPixelRect(layout.close.x, layout.close.y, layout.close.w, layout.close.h, colorLocation, 1.0f, 1.0f, 1.0f, 0.25f);
		drawPixelRect(layout.language.x, layout.language.y, layout.language.w, layout.language.h, colorLocation, 1.0f, 1.0f, 1.0f, 0.25f);
		appendCenteredText(text, w * 0.5f, h * 0.08f, cell, guideTitle(g_guideLanguage));
		const std::vector<std::string> lines = guideBody(g_guideLanguage);
		for (std::size_t i = 0; i < lines.size(); ++i)
			appendGuideText(text, lines[i], w * 0.08f, h * 0.20f + static_cast<float>(i) * cell * 7.0f, cell);
		appendCenteredText(text, layout.close.x + layout.close.w * 0.5f, layout.close.y + layout.close.h * 0.5f - 2.0f * cell, cell, "CLOSE");
		appendCenteredText(text, layout.language.x + layout.language.w * 0.5f, layout.language.y + layout.language.h * 0.5f - 2.0f * cell, cell, "LANG");
	}
	drawPixelLines(text, colorLocation, 1.0f, 1.0f, 1.0f, 1.0f);
}

bool handleGuideTap(float x, float y)
{
	const GuideLayout layout = guideLayout();
	if (!g_guideOpen)
	{
		if (!layout.help.contains(x, y))
			return false;
		g_guideOpen = true;
		return true;
	}

	if (!g_guideLanguageChosen)
	{
		if (layout.english.contains(x, y))
		{
			g_guideLanguage = GuideLanguage::English;
			g_guideLanguageChosen = true;
		}
		else if (layout.spanish.contains(x, y))
		{
			g_guideLanguage = GuideLanguage::Spanish;
			g_guideLanguageChosen = true;
		}
	}
	else if (layout.close.contains(x, y))
	{
		g_guideOpen = false;
	}
	else if (layout.language.contains(x, y))
	{
		g_guideLanguageChosen = false;
	}
	return true;
}

void drawTouchControls()
{
	if (g_width <= 0 || g_height <= 0)
		return;
	initOverlay();
	if (g_overlayProgram == 0)
		return;

	GLint previousProgram = 0;
	GLint previousVao = 0;
	GLint previousBuffer = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousBuffer);
	const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
	const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	const float width = static_cast<float>(g_width);
	const float height = static_cast<float>(g_height);
	auto toNdcX = [width](float x) { return x / width * 2.0f - 1.0f; };
	auto toNdcY = [height](float y) { return 1.0f - y / height * 2.0f; };

	glUseProgram(g_overlayProgram);
	glBindVertexArray(g_overlayVao);
	glBindBuffer(GL_ARRAY_BUFFER, g_overlayVbo);
	const GLint colorLocation = glGetUniformLocation(g_overlayProgram, "color");
	drawGuide(colorLocation);

	constexpr int segments = 32;
	constexpr float pi = 3.14159265f;
	std::array<float, (segments + 2) * 2> vertices{};
	for (const VirtualButton& button : virtualButtons())
	{
		if (g_guideOpen)
			break;
		const bool pressed = button.shoot ? g_touchShoot : g_touchKeys[static_cast<size_t>(button.key)];
		vertices[0] = toNdcX(button.x);
		vertices[1] = toNdcY(button.y);
		for (int i = 0; i <= segments; ++i)
		{
			const float angle = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
			vertices[(i + 1) * 2] = toNdcX(button.x + std::cos(angle) * button.radius);
			vertices[(i + 1) * 2 + 1] = toNdcY(button.y + std::sin(angle) * button.radius);
		}
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_DYNAMIC_DRAW);
		if (pressed)
			glUniform4f(colorLocation, 1.0f, 1.0f, 1.0f, 0.6f);
		else
			glUniform4f(colorLocation, 1.0f, 1.0f, 1.0f, 0.25f);
		glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 2);

		const float cell = button.radius * 0.2f;
		std::vector<float> labelVertices;
		for (const std::array<int, 4>& segment : labelSegments(button.label))
		{
			labelVertices.push_back(button.x + static_cast<float>(segment[0] - 1) * cell);
			labelVertices.push_back(button.y + static_cast<float>(segment[1] - 2) * cell);
			labelVertices.push_back(button.x + static_cast<float>(segment[2] - 1) * cell);
			labelVertices.push_back(button.y + static_cast<float>(segment[3] - 2) * cell);
		}
		for (std::size_t i = 0; i < labelVertices.size(); i += 2)
		{
			labelVertices[i] = toNdcX(labelVertices[i]);
			labelVertices[i + 1] = toNdcY(labelVertices[i + 1]);
		}
		if (!labelVertices.empty())
		{
			glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(labelVertices.size() * sizeof(float)), labelVertices.data(), GL_DYNAMIC_DRAW);
			glUniform4f(colorLocation, 1.0f, 1.0f, 1.0f, pressed ? 1.0f : 0.8f);
			glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(labelVertices.size() / 2));
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousBuffer));
	glBindVertexArray(static_cast<GLuint>(previousVao));
	glUseProgram(static_cast<GLuint>(previousProgram));
	if (depthWasEnabled)
		glEnable(GL_DEPTH_TEST);
	if (!blendWasEnabled)
		glDisable(GL_BLEND);
}

bool initEgl(ANativeWindow* window)
{
	g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (g_display == EGL_NO_DISPLAY || !eglInitialize(g_display, nullptr, nullptr))
		return false;

	const EGLint configAttributes[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_NONE
	};
	EGLConfig config = nullptr;
	EGLint configCount = 0;
	if (!eglChooseConfig(g_display, configAttributes, &config, 1, &configCount) || configCount == 0)
		return false;

	EGLint format = 0;
	eglGetConfigAttrib(g_display, config, EGL_NATIVE_VISUAL_ID, &format);
	ANativeWindow_setBuffersGeometry(window, 0, 0, format);

	g_surface = eglCreateWindowSurface(g_display, config, window, nullptr);
	if (g_surface == EGL_NO_SURFACE)
		return false;

	const EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	g_context = eglCreateContext(g_display, config, EGL_NO_CONTEXT, contextAttributes);
	if (g_context == EGL_NO_CONTEXT)
		return false;

	if (!eglMakeCurrent(g_display, g_surface, g_surface, g_context))
		return false;

	eglQuerySurface(g_display, g_surface, EGL_WIDTH, &g_width);
	eglQuerySurface(g_display, g_surface, EGL_HEIGHT, &g_height);
	return true;
}

void termEgl()
{
	if (g_display != EGL_NO_DISPLAY)
	{
		eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (g_context != EGL_NO_CONTEXT)
			eglDestroyContext(g_display, g_context);
		if (g_surface != EGL_NO_SURFACE)
			eglDestroySurface(g_display, g_surface);
		eglTerminate(g_display);
	}
	g_display = EGL_NO_DISPLAY;
	g_surface = EGL_NO_SURFACE;
	g_context = EGL_NO_CONTEXT;
}

void handleAppCommand(android_app* app, int32_t command)
{
	switch (command)
	{
	case APP_CMD_INIT_WINDOW:
		if (app->window != nullptr && !initEgl(app->window))
			__android_log_print(ANDROID_LOG_ERROR, "AimTrainer", "Failed to initialize EGL");
		break;
	case APP_CMD_TERM_WINDOW:
		termEgl();
		break;
	default:
		break;
	}
}

int32_t handleInputEvent(android_app*, AInputEvent* event)
{
	const int32_t type = AInputEvent_getType(event);
	if (type == AINPUT_EVENT_TYPE_KEY)
	{
		const bool down = AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_DOWN;
		const bool up = AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_UP;
		bool mapped = false;
		const Key key = mapKeyCode(AKeyEvent_getKeyCode(event), mapped);
		if (!mapped || (!down && !up))
			return 0;
		g_keys[static_cast<size_t>(key)] = down;
		if (g_keyCallback != nullptr && down)
			g_keyCallback(key, KeyAction::Press);
		else if (g_keyCallback != nullptr && up)
			g_keyCallback(key, KeyAction::Release);
		if (down && g_textCallback != nullptr)
		{
			const unsigned int codepoint = mapTextCodepoint(AKeyEvent_getKeyCode(event));
			if (codepoint != 0)
				g_textCallback(codepoint);
		}
		return 1;
	}
	if (type == AINPUT_EVENT_TYPE_MOTION)
	{
		const int32_t source = AInputEvent_getSource(event);
		if ((source & AINPUT_SOURCE_TOUCHSCREEN) != AINPUT_SOURCE_TOUCHSCREEN)
			return 0;
		const int32_t rawAction = AMotionEvent_getAction(event);
		const int32_t action = rawAction & AMOTION_EVENT_ACTION_MASK;
		const size_t liftedIndex = static_cast<size_t>((rawAction & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
		const bool allReleased = action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL;
		const bool isDown = action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_POINTER_DOWN;
		if (allReleased || (action == AMOTION_EVENT_ACTION_POINTER_UP && AMotionEvent_getPointerId(event, liftedIndex) == g_lookPointerId))
			g_lookPointerId = -1;

		if (isDown && handleGuideTap(AMotionEvent_getX(event, liftedIndex), AMotionEvent_getY(event, liftedIndex)))
			return 1;
		if (g_guideOpen)
		{
			g_touchKeys = {};
			g_touchShoot = false;
			g_leftMouseDown = false;
			g_lookPointerId = -1;
			return 1;
		}

		std::array<bool, static_cast<size_t>(Key::Count)> touchKeys{};
		bool shootDown = false;
		bool tapDown = false;
		const auto buttons = virtualButtons();
		const size_t pointerCount = AMotionEvent_getPointerCount(event);
		const GuideLayout layout = guideLayout();
		for (size_t i = 0; i < pointerCount && !allReleased; ++i)
		{
			if (action == AMOTION_EVENT_ACTION_POINTER_UP && i == liftedIndex)
				continue;
			const float x = AMotionEvent_getX(event, i);
			const float y = AMotionEvent_getY(event, i);
			if (layout.crosshair.contains(x, y))
			{
				touchKeys[static_cast<size_t>(Key::C)] = true;
				continue;
			}
			if (layout.ranks.contains(x, y))
			{
				touchKeys[static_cast<size_t>(Key::R)] = true;
				continue;
			}
			if (g_scenarioMenuOpen && layout.scenarioAim.contains(x, y))
			{
				touchKeys[static_cast<size_t>(Key::Num1)] = true;
				g_scenarioMenuOpen = false;
				continue;
			}
			if (g_scenarioMenuOpen && layout.scenarioTwo.contains(x, y))
			{
				touchKeys[static_cast<size_t>(Key::Num2)] = true;
				g_scenarioMenuOpen = false;
				continue;
			}
			if (g_scenarioMenuOpen && layout.scenarioSix.contains(x, y))
			{
				touchKeys[static_cast<size_t>(Key::Num3)] = true;
				g_scenarioMenuOpen = false;
				continue;
			}
			if (layout.scenario.contains(x, y))
			{
				if (isDown && i == liftedIndex)
					g_scenarioMenuOpen = !g_scenarioMenuOpen;
				continue;
			}
			bool hitButton = false;
			for (const VirtualButton& button : buttons)
			{
				const float dx = x - button.x;
				const float dy = y - button.y;
				if (dx * dx + dy * dy > button.radius * button.radius)
					continue;
				hitButton = true;
				if (button.shoot)
					shootDown = true;
				else
					touchKeys[static_cast<size_t>(button.key)] = true;
			}
			if (hitButton)
				continue;
			const int32_t pointerId = AMotionEvent_getPointerId(event, i);
			if (pointerId == g_lookPointerId)
			{
				g_lookX += x - g_lastLookX;
				g_lookY += y - g_lastLookY;
				g_lastLookX = x;
				g_lastLookY = y;
			}
			else if (g_lookPointerId == -1 && (action == AMOTION_EVENT_ACTION_DOWN ? i == 0 : i == liftedIndex) && isDown && x >= static_cast<float>(g_width) * 0.5f)
			{
				g_lookPointerId = pointerId;
				g_lastLookX = x;
				g_lastLookY = y;
			}
			else
			{
				tapDown = true;
				g_pointerX = x;
				g_pointerY = y;
			}
		}

		g_touchKeys = touchKeys;
		g_touchShoot = shootDown;
		g_leftMouseDown = shootDown || tapDown;
		return 1;
	}
	return 0;
}
}

void setAndroidApp(android_app* app)
{
	g_app = app;
	g_app->onAppCmd = handleAppCommand;
	g_app->onInputEvent = handleInputEvent;
}

void showExtractionFailureColor(float red, float green, float blue)
{
	glClearColor(red, green, blue, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	eglSwapBuffers(g_display, g_surface);
}

std::string extractAndroidAssets()
{
	if (g_app == nullptr || g_app->activity == nullptr || g_app->activity->internalDataPath == nullptr || g_app->activity->assetManager == nullptr)
		return {};

	namespace fs = std::filesystem;
	const fs::path root = fs::path(g_app->activity->internalDataPath) / "assets";
	const std::array<const char*, 10> relativePaths = {
		"default.vert", "default.frag",
		"Resources/models/plane/plane.gltf", "Resources/models/plane/plane.bin", "Resources/models/plane/baseColor.png",
		"Resources/models/orb/redorb.gltf", "Resources/models/orb/redorb.bin", "Resources/models/orb/baseColor.png",
		"Resources/models/prism/prism.gltf", "Resources/models/prism/prism.bin"
	};
	for (const char* relativePath : relativePaths)
	{
		AAsset* asset = AAssetManager_open(g_app->activity->assetManager, relativePath, AASSET_MODE_BUFFER);
		if (asset == nullptr)
		{
			__android_log_print(ANDROID_LOG_ERROR, "AimTrainer", "Asset not found in APK: %s", relativePath);
			showExtractionFailureColor(1.0f, 0.5f, 0.0f);
			return {};
		}

		const fs::path destination = root / relativePath;
		std::error_code error;
		fs::create_directories(destination.parent_path(), error);
		FILE* output = std::fopen(destination.string().c_str(), "wb");
		bool written = output != nullptr;
		std::array<char, 8192> chunk{};
		int bytesRead = 0;
		while (written && (bytesRead = AAsset_read(asset, chunk.data(), chunk.size())) > 0)
			written = std::fwrite(chunk.data(), 1, static_cast<size_t>(bytesRead), output) == static_cast<size_t>(bytesRead);
		written = written && bytesRead == 0;
		if (output != nullptr)
			std::fclose(output);
		AAsset_close(asset);
		if (!written)
		{
			__android_log_print(ANDROID_LOG_ERROR, "AimTrainer", "Failed to extract asset: %s", relativePath);
			showExtractionFailureColor(1.0f, 0.0f, 1.0f);
			return {};
		}
	}
	return root.string();
}

bool createWindow(int& width, int& height)
{
	if (g_app == nullptr)
		return false;
	while (g_app->window == nullptr && !g_app->destroyRequested)
		pollEvents();
	width = g_width;
	height = g_height;
	return g_surface != EGL_NO_SURFACE;
}

void destroyWindow()
{
	termEgl();
}

bool shouldClose()
{
	return g_app == nullptr || g_app->destroyRequested != 0;
}

void requestClose()
{
	if (g_app != nullptr)
		ANativeActivity_finish(g_app->activity);
}

void swapBuffers()
{
	if (g_display != EGL_NO_DISPLAY && g_surface != EGL_NO_SURFACE)
	{
		drawTouchControls();
		eglSwapBuffers(g_display, g_surface);
	}
}

void pollEvents()
{
	int events = 0;
	android_poll_source* source = nullptr;
	while (ALooper_pollOnce(0, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0)
	{
		if (source != nullptr)
			source->process(g_app, source);
		if (g_app != nullptr && g_app->destroyRequested)
			break;
	}
}

bool isKeyDown(Key key)
{
	return g_keys[static_cast<size_t>(key)] || g_touchKeys[static_cast<size_t>(key)];
}

bool isLeftMouseDown()
{
	return g_leftMouseDown;
}

void getCursorPosition(double& x, double& y)
{
	x = g_pointerX;
	y = g_pointerY;
}

void getLookPosition(double& x, double& y)
{
	x = g_lookX;
	y = g_lookY;
}

void getFramebufferSize(int& width, int& height)
{
	width = g_width;
	height = g_height;
}

void setCursorCaptured(bool)
{
}

void setTextCallback(TextCallback callback)
{
	g_textCallback = callback;
}

void setKeyCallback(KeyCallback callback)
{
	g_keyCallback = callback;
}

void setFramebufferCallback(FramebufferCallback)
{
}

void toggleFullscreen()
{
}

const char* getClipboardString()
{
	return nullptr;
}

void* getProcAddress(const char* name)
{
	return reinterpret_cast<void*>(eglGetProcAddress(name));
}

double getTime()
{
	timespec now{};
	clock_gettime(CLOCK_MONOTONIC, &now);
	return static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_nsec) * 1e-9;
}
}

#endif

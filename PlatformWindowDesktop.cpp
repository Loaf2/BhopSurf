#ifndef __ANDROID__

#include "PlatformWindow.h"

#include <GLFW/glfw3.h>

#include <array>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace platform
{
namespace
{
GLFWwindow* g_window = nullptr;
GLFWmonitor* g_primaryMonitor = nullptr;
TextCallback g_textCallback = nullptr;
KeyCallback g_keyCallback = nullptr;
FramebufferCallback g_framebufferCallback = nullptr;
int g_windowedX = 100;
int g_windowedY = 100;
int g_windowedWidth = 1280;
int g_windowedHeight = 720;

int toGlfwKey(Key key)
{
	switch (key)
	{
	case Key::Escape: return GLFW_KEY_ESCAPE;
	case Key::Space: return GLFW_KEY_SPACE;
	case Key::Tab: return GLFW_KEY_TAB;
	case Key::Enter: return GLFW_KEY_ENTER;
	case Key::Backspace: return GLFW_KEY_BACKSPACE;
	case Key::LeftShift: return GLFW_KEY_LEFT_SHIFT;
	case Key::RightShift: return GLFW_KEY_RIGHT_SHIFT;
	case Key::Control: return GLFW_KEY_LEFT_CONTROL;
	case Key::V: return GLFW_KEY_V;
	case Key::W: return GLFW_KEY_W;
	case Key::A: return GLFW_KEY_A;
	case Key::S: return GLFW_KEY_S;
	case Key::D: return GLFW_KEY_D;
	case Key::X: return GLFW_KEY_X;
	case Key::Z: return GLFW_KEY_Z;
	case Key::F: return GLFW_KEY_F;
	case Key::J: return GLFW_KEY_J;
	case Key::C: return GLFW_KEY_C;
	case Key::R: return GLFW_KEY_R;
	case Key::Num2: return GLFW_KEY_2;
	case Key::Num1: return GLFW_KEY_1;
	case Key::Num4: return GLFW_KEY_4;
	default: return GLFW_KEY_UNKNOWN;
	}
}

Key fromGlfwKey(int key, bool& recognized)
{
	recognized = true;
	switch (key)
	{
	case GLFW_KEY_ESCAPE: return Key::Escape;
	case GLFW_KEY_SPACE: return Key::Space;
	case GLFW_KEY_TAB: return Key::Tab;
	case GLFW_KEY_ENTER: return Key::Enter;
	case GLFW_KEY_BACKSPACE: return Key::Backspace;
	case GLFW_KEY_LEFT_SHIFT: return Key::LeftShift;
	case GLFW_KEY_RIGHT_SHIFT: return Key::RightShift;
	case GLFW_KEY_LEFT_CONTROL:
	case GLFW_KEY_RIGHT_CONTROL: return Key::Control;
	case GLFW_KEY_V: return Key::V;
	case GLFW_KEY_W: return Key::W;
	case GLFW_KEY_A: return Key::A;
	case GLFW_KEY_S: return Key::S;
	case GLFW_KEY_D: return Key::D;
	case GLFW_KEY_X: return Key::X;
	case GLFW_KEY_Z: return Key::Z;
	case GLFW_KEY_F: return Key::F;
	case GLFW_KEY_J: return Key::J;
	case GLFW_KEY_C: return Key::C;
	case GLFW_KEY_R: return Key::R;
	case GLFW_KEY_2: return Key::Num2;
	case GLFW_KEY_1: return Key::Num1;
	case GLFW_KEY_4: return Key::Num4;
	default:
		recognized = false;
		return Key::Count;
	}
}

void glfwCharCallback(GLFWwindow*, unsigned int codepoint)
{
	if (g_textCallback != nullptr)
		g_textCallback(codepoint);
}

void glfwKeyCallback(GLFWwindow*, int key, int, int action, int)
{
	if (g_keyCallback == nullptr)
		return;
	bool recognized = false;
	const Key mapped = fromGlfwKey(key, recognized);
	if (!recognized)
		return;
	if (action == GLFW_PRESS)
		g_keyCallback(mapped, KeyAction::Press);
	else if (action == GLFW_RELEASE)
		g_keyCallback(mapped, KeyAction::Release);
	else if (action == GLFW_REPEAT)
		g_keyCallback(mapped, KeyAction::Repeat);
}

void glfwFramebufferSizeCallback(GLFWwindow*, int width, int height)
{
	if (g_framebufferCallback != nullptr)
		g_framebufferCallback(width, height);
}

void glfwMouseButtonCallback(GLFWwindow*, int button, int action, int)
{
	if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS && g_window != nullptr)
		glfwSetWindowShouldClose(g_window, GLFW_TRUE);
}
}

void setTextCallback(TextCallback callback)
{
	g_textCallback = callback;
}

void setKeyCallback(KeyCallback callback)
{
	g_keyCallback = callback;
}

void setFramebufferCallback(FramebufferCallback callback)
{
	g_framebufferCallback = callback;
}

void toggleFullscreen()
{
	if (g_window == nullptr)
		return;
	if (glfwGetWindowMonitor(g_window) != nullptr)
	{
		glfwSetWindowMonitor(g_window, nullptr, g_windowedX, g_windowedY, g_windowedWidth, g_windowedHeight, 0);
		return;
	}
	glfwGetWindowPos(g_window, &g_windowedX, &g_windowedY);
	glfwGetWindowSize(g_window, &g_windowedWidth, &g_windowedHeight);
	const GLFWvidmode* mode = g_primaryMonitor ? glfwGetVideoMode(g_primaryMonitor) : nullptr;
	if (mode != nullptr)
		glfwSetWindowMonitor(g_window, g_primaryMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
}

const char* getClipboardString()
{
	return g_window != nullptr ? glfwGetClipboardString(g_window) : nullptr;
}

bool createWindow(int& width, int& height)
{
	if (!glfwInit())
		return false;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	g_primaryMonitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* nativeMode = g_primaryMonitor ? glfwGetVideoMode(g_primaryMonitor) : nullptr;
	const int windowWidth = nativeMode ? nativeMode->width : width;
	const int windowHeight = nativeMode ? nativeMode->height : height;
	g_window = glfwCreateWindow(windowWidth, windowHeight, "Aim Trainer", nativeMode ? g_primaryMonitor : nullptr, nullptr);
	if (g_window == nullptr)
	{
		glfwTerminate();
		return false;
	}

	glfwMakeContextCurrent(g_window);
#ifdef _WIN32
	HICON icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(501), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
	if (icon != nullptr)
	{
		HWND nativeWindow = glfwGetWin32Window(g_window);
		SendMessageW(nativeWindow, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
		SendMessageW(nativeWindow, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
	}
#endif
	glfwSetFramebufferSizeCallback(g_window, glfwFramebufferSizeCallback);
	glfwSetCharCallback(g_window, glfwCharCallback);
	glfwSetKeyCallback(g_window, glfwKeyCallback);
	glfwSetMouseButtonCallback(g_window, glfwMouseButtonCallback);
	glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	glfwSwapInterval(0);

	width = windowWidth;
	height = windowHeight;
	return true;
}

void destroyWindow()
{
	if (g_window != nullptr)
	{
		glfwDestroyWindow(g_window);
		g_window = nullptr;
	}
	glfwTerminate();
}

bool shouldClose()
{
	return g_window != nullptr && glfwWindowShouldClose(g_window);
}

void requestClose()
{
	if (g_window != nullptr)
		glfwSetWindowShouldClose(g_window, GLFW_TRUE);
}

void swapBuffers()
{
	if (g_window != nullptr)
		glfwSwapBuffers(g_window);
}

void pollEvents()
{
	glfwPollEvents();
}

bool isKeyDown(Key key)
{
	return g_window != nullptr && glfwGetKey(g_window, toGlfwKey(key)) == GLFW_PRESS;
}

bool isLeftMouseDown()
{
	return g_window != nullptr && glfwGetMouseButton(g_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
}

void getCursorPosition(double& x, double& y)
{
	if (g_window != nullptr)
		glfwGetCursorPos(g_window, &x, &y);
}

void getLookPosition(double& x, double& y)
{
	getCursorPosition(x, y);
}

void getFramebufferSize(int& width, int& height)
{
	if (g_window != nullptr)
		glfwGetFramebufferSize(g_window, &width, &height);
}

void setCursorCaptured(bool captured)
{
	if (g_window != nullptr)
		glfwSetInputMode(g_window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

double getTime()
{
	return glfwGetTime();
}

void* getProcAddress(const char* name)
{
	return reinterpret_cast<void*>(glfwGetProcAddress(name));
}
}

#endif

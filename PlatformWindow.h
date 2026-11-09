#pragma once

#include <string>

#ifdef __ANDROID__
struct android_app;
#endif

namespace platform
{
enum class Key
{
	Escape,
	Space,
	Tab,
	Enter,
	Backspace,
	LeftShift,
	RightShift,
	Control,
	V,
	W,
	A,
	S,
	D,
	X,
	Z,
	F,
	J,
	Num1,
	Num2,
	Num3,
	Num4,
	Num5,
	Num6,
	C,
	R,
	Count
};

#ifdef __ANDROID__
void setAndroidApp(android_app* app);
std::string extractAndroidAssets();
#endif

enum class KeyAction
{
	Release,
	Press,
	Repeat
};

using TextCallback = void (*)(unsigned int codepoint);
using KeyCallback = void (*)(Key key, KeyAction action);
using FramebufferCallback = void (*)(int width, int height);

void setTextCallback(TextCallback callback);
void setKeyCallback(KeyCallback callback);
void setFramebufferCallback(FramebufferCallback callback);
void toggleFullscreen();
const char* getClipboardString();

bool createWindow(int& width, int& height);
void destroyWindow();
bool shouldClose();
void requestClose();
void swapBuffers();
void pollEvents();
bool isKeyDown(Key key);
bool isLeftMouseDown();
void getCursorPosition(double& x, double& y);
void getLookPosition(double& x, double& y);
void getFramebufferSize(int& width, int& height);
void setCursorCaptured(bool captured);
double getTime();
void* getProcAddress(const char* name);
}

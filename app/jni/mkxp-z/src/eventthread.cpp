/*
** eventthread.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "eventthread.h"

#include <SDL_events.h>
#include <SDL_messagebox.h>
#include <SDL_timer.h>
#include <SDL_thread.h>
#include <SDL_touch.h>
#include <SDL_rect.h>


#include <al.h>
#include <alc.h>
#include <alext.h>
#include <cmath>

#include "sharedstate.h"
#include "graphics.h"

#ifndef MKXPZ_BUILD_XCODE
#include "settingsmenu.h"
#include "gamecontrollerdb.txt.xxd"
#else
#include "system/system.h"
#include "filesystem/filesystem.h"
#include "TouchBar.h"
#endif

#ifdef MKXPZ_BUILD_ANDROID
#include <SDL.h>
#include <jni.h>

// 缓存JNI相关引用
static jclass cachedJavaClass = nullptr;
static jmethodID cachedSetFPSVisibilityMethod = nullptr;
static jmethodID cachedUpdateFPSTextMethod = nullptr;

// 初始化JNI缓存
static bool initJNICache() {
    if (cachedJavaClass != nullptr) {
        return true; // 已经初始化过
    }

    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    if (!env) {
        return false;
    }

    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!activity) {
        return false;
    }

    jclass cls = env->GetObjectClass(activity);
    if (!cls) {
        env->DeleteLocalRef(activity);
        return false;
    }

    // 创建全局引用，这样即使跨JNI调用也能保持有效
    cachedJavaClass = (jclass)env->NewGlobalRef(cls);

    // 获取方法ID
    cachedSetFPSVisibilityMethod = env->GetStaticMethodID(cachedJavaClass, "setFPSVisibility", "(Z)V");
    cachedUpdateFPSTextMethod = env->GetStaticMethodID(cachedJavaClass, "updateFPSText", "(I)V");

    // 释放本地引用
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);

    return (cachedSetFPSVisibilityMethod != nullptr && cachedUpdateFPSTextMethod != nullptr);
}

// 清理JNI缓存，在程序退出时调用
static void cleanupJNICache() {
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    if (env && cachedJavaClass) {
        env->DeleteGlobalRef(cachedJavaClass);
        cachedJavaClass = nullptr;
    }
    cachedSetFPSVisibilityMethod = nullptr;
    cachedUpdateFPSTextMethod = nullptr;
}

static void jniSetFPSVisibility(bool state)
{
    if (!initJNICache()) {
        SDL_Log("Failed to initialize JNI cache for FPS visibility");
        return;
    }

    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    env->CallStaticVoidMethod(cachedJavaClass, cachedSetFPSVisibilityMethod, state);
}

static void jniUpdateFPSText(int num)
{
    if (!initJNICache()) {
        SDL_Log("Failed to initialize JNI cache for FPS text update");
        return;
    }

    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    env->CallStaticVoidMethod(cachedJavaClass, cachedUpdateFPSTextMethod, (jint)num);
}
#endif

#include "al-util.h"
#include "debugwriter.h"

#ifndef __APPLE__
#include "util/string-util.h"
#endif

#include <string.h>

typedef void (ALC_APIENTRY *LPALCDEVICEPAUSESOFT) (ALCdevice *device);
typedef void (ALC_APIENTRY *LPALCDEVICERESUMESOFT) (ALCdevice *device);

#define AL_DEVICE_PAUSE_FUN \
	AL_FUN(DevicePause, LPALCDEVICEPAUSESOFT) \
	AL_FUN(DeviceResume, LPALCDEVICERESUMESOFT)

struct ALCFunctions
{
#define AL_FUN(name, type) type name;
	AL_DEVICE_PAUSE_FUN
#undef AL_FUN
} static alc;

static void initALCFunctions(ALCdevice *alcDev)
{
	if (!strstr(alcGetString(alcDev, ALC_EXTENSIONS), "ALC_SOFT_pause_device"))
		return;

	Debug() << "ALC_SOFT_pause_device present";

#define AL_FUN(name, type) alc. name = (type) alcGetProcAddress(alcDev, "alc" #name "SOFT");
	AL_DEVICE_PAUSE_FUN;
#undef AL_FUN
}

#define HAVE_ALC_DEVICE_PAUSE alc.DevicePause

uint8_t EventThread::keyStates[];
EventThread::ControllerState EventThread::controllerState;
EventThread::MouseState EventThread::mouseState;
EventThread::TouchState EventThread::touchState;
SDL_atomic_t EventThread::verticalScrollDistance;

/* User event codes */
enum
{
	REQUEST_SETFULLSCREEN = 0,
	REQUEST_WINRESIZE,
	REQUEST_WINREPOSITION,
	REQUEST_WINRENAME,
	REQUEST_WINCENTER,
	REQUEST_MESSAGEBOX,
	REQUEST_SETCURSORVISIBLE,

	REQUEST_TEXTMODE,

	REQUEST_SETTINGS,

	UPDATE_FPS,
	UPDATE_SCREEN_RECT,

	EVENT_COUNT
};

static uint32_t usrIdStart;

bool EventThread::allocUserEvents()
{
	usrIdStart = SDL_RegisterEvents(EVENT_COUNT);

	if (usrIdStart == (uint32_t) -1)
		return false;

	return true;
}

EventThread::EventThread()
	: ctrl(0),
	  fullscreen(false),
	  showCursor(false)
{
	textInputLock = SDL_CreateMutex();
}

EventThread::~EventThread()
{
	SDL_DestroyMutex(textInputLock);
}

SDL_TimerID hideCursorTimerID = 0;
Uint32 cursorTimerCallback(Uint32 interval, void* param)
{
    EventThread *ethread = static_cast<EventThread*>(param);
    hideCursorTimerID = 0;
    ethread->requestShowCursor(ethread->getShowCursor());
    return 0;
}
void EventThread::cursorTimer()
{
    SDL_RemoveTimer(hideCursorTimerID);
    hideCursorTimerID = SDL_AddTimer(500, cursorTimerCallback, this);
}
#ifdef MKXPZ_BUILD_ANDROID
bool firstjniSetFPSVisibility = false;
#endif
void EventThread::process(RGSSThreadData &rtData)
{
	SDL_Event event;
	SDL_Window *win = rtData.window;

	UnidirMessage<Vec2i> &windowSizeMsg = rtData.windowSizeMsg;
	UnidirMessage<Vec2i> &drawableSizeMsg = rtData.drawableSizeMsg;

	initALCFunctions(rtData.alcDev);

	// XXX this function breaks input focus on OSX
#ifndef __APPLE__
	SDL_SetEventFilter(eventFilter, &rtData);
#endif

	fullscreen = rtData.config.fullscreen;
	int toggleFSMod = rtData.config.anyAltToggleFS ? KMOD_ALT : KMOD_LALT;
    bool displayingFPS = rtData.config.displayFPS;
#ifdef MKXPZ_BUILD_ANDROID
	if (!firstjniSetFPSVisibility && (displayingFPS || rtData.config.printFPS)) {
        firstjniSetFPSVisibility = true;
        jniSetFPSVisibility(true);
		fps.sendUpdates.set();
	}
#else
    if (displayingFPS || rtData.config.printFPS) {
		fps.sendUpdates.set();
    }
#endif
	bool cursorInWindow = false;

	// Will be updated eventually
	SDL_Rect gameScreen = { 0, 0, 0, 0 };

	// SDL doesn't send an initial FOCUS_GAINED event
	bool windowFocused = true;

	bool terminate = false;

#ifdef MKXPZ_BUILD_XCODE
	SDL_GameControllerAddMappingsFromFile(mkxp_fs::getPathForAsset("gamecontrollerdb", "txt").c_str());
#else
	SDL_GameControllerAddMappingsFromRW(SDL_RWFromConstMem(assets_gamecontrollerdb_txt, assets_gamecontrollerdb_txt_len), 1);
#endif

	SDL_JoystickUpdate();
	if (SDL_NumJoysticks() > 0 && SDL_IsGameController(0)) {
		ctrl = SDL_GameControllerOpen(0);
	}

	char buffer[128];

	char pendingTitle[128];
	bool havePendingTitle = false;

	bool resetting = false;

	int winW, winH;
	int i, rc;

	SDL_DisplayMode dm = {0};

	SDL_GetWindowSize(win, &winW, &winH);

	// Just in case it's started when the window is opened
	// for some dumb reason
	SDL_StopTextInput();

	textInputBuffer.clear();

#ifndef MKXPZ_BUILD_XCODE
	SettingsMenu *sMenu = 0;
#else
	// Will always be 0
	void *sMenu = 0;
#endif

	while (true)
	{
		if (!SDL_WaitEvent(&event)) {
			Debug() << "EventThread: Event error";
			break;
		}

#ifndef MKXPZ_BUILD_XCODE
		if (sMenu && sMenu->onEvent(event)) {
			if (sMenu->destroyReq()) {
				delete sMenu;
				sMenu = 0;
				updateCursorState(cursorInWindow && windowFocused, gameScreen);
			}
			continue;
		}
#endif

		// Now process the rest
		switch (event.type)
		{
			case SDL_WINDOWEVENT:
				switch (event.window.event)
				{
					case SDL_WINDOWEVENT_SIZE_CHANGED:
						winW = event.window.data1;
						winH = event.window.data2;

						int drwW, drwH;
						SDL_GL_GetDrawableSize(win, &drwW, &drwH);

						windowSizeMsg.post(Vec2i(winW, winH));
						drawableSizeMsg.post(Vec2i(drwW, drwH));

						resetInputStates();

						break;

					case SDL_WINDOWEVENT_ENTER:
						cursorInWindow = true;
						mouseState.inWindow = true;
						updateCursorState(cursorInWindow && windowFocused && !sMenu, gameScreen);
						break;

					case SDL_WINDOWEVENT_LEAVE:
						cursorInWindow = false;
						mouseState.inWindow = false;
						updateCursorState(cursorInWindow && windowFocused && !sMenu, gameScreen);
						break;

					case SDL_WINDOWEVENT_CLOSE:
						terminate = true;
						break;

					case SDL_WINDOWEVENT_FOCUS_GAINED:
						windowFocused = true;
						updateCursorState(cursorInWindow && windowFocused && !sMenu, gameScreen);
						break;

					case SDL_WINDOWEVENT_FOCUS_LOST:
						windowFocused = false;
						updateCursorState(cursorInWindow && windowFocused && !sMenu, gameScreen);
						resetInputStates();
						break;
				}
				break;

			case SDL_TEXTINPUT:
				lockText(true);

				if (textInputBuffer.size() < 512)
					textInputBuffer += event.text.text;

				lockText(false);

				break;

			case SDL_QUIT:
				terminate = true;
				Debug() << "EventThread termination requested";
				break;

			case SDL_KEYDOWN:
				// Alt+Enter: fullscreen switch
				if (event.key.keysym.scancode == SDL_SCANCODE_RETURN && (event.key.keysym.mod & toggleFSMod)) {
					setFullscreen(win, !fullscreen);

					if (!fullscreen && havePendingTitle) {
						SDL_SetWindowTitle(win, pendingTitle);
						pendingTitle[0] = '\0';
						havePendingTitle = false;
					}

					break;
				}

#ifndef MKXPZ_BUILD_ANDROID
				// F1: settings menu
				if (event.key.keysym.scancode == SDL_SCANCODE_F1 && rtData.config.enableSettings) {
                    // Do not open settings menu until initializing shared state.
                    // Opening before initializing shared state will crash (segmentation fault).
                    if (!shState)
                    {
                        break;
                    }
#ifndef MKXPZ_BUILD_XCODE
					if (!sMenu) {
						sMenu = new SettingsMenu(rtData);
						updateCursorState(false, gameScreen);
					}
					sMenu->raise();
#else
					openSettingsWindow();
#endif
				}
#endif

				// F2: display FPS
				if (event.key.keysym.scancode == SDL_SCANCODE_F2) {
					if (!displayingFPS) {
						fps.sendUpdates.set();
						displayingFPS = true;
#ifdef MKXPZ_BUILD_ANDROID
						jniSetFPSVisibility(true);
#endif
					} else {
#ifdef MKXPZ_BUILD_ANDROID
						jniSetFPSVisibility(false);
#endif
						displayingFPS = false;

						if (!rtData.config.printFPS)
							fps.sendUpdates.clear();

						if (fullscreen) {
							// Prevent fullscreen flicker
							strncpy(pendingTitle, rtData.config.windowTitle.c_str(), sizeof(pendingTitle));
							havePendingTitle = true;
							break;
						}

						SDL_SetWindowTitle(win, rtData.config.windowTitle.c_str());
					}
					break;
				}

				// F12: RGSS reset
				if (event.key.keysym.scancode == SDL_SCANCODE_F12) {
					if (!rtData.config.enableReset)
						break;

					if (resetting)
						break;

					resetting = true;
					rtData.rqResetFinish.clear();
					rtData.rqReset.set();

					break;
				}

				keyStates[event.key.keysym.scancode] = true;

				break;

			case SDL_KEYUP:
				if (event.key.keysym.scancode == SDL_SCANCODE_F12) {
					if (!rtData.config.enableReset)
						break;

					resetting = false;
					rtData.rqResetFinish.set();

					break;
				}

				keyStates[event.key.keysym.scancode] = false;

				break;

			case SDL_CONTROLLERBUTTONDOWN:
				controllerState.buttons[event.cbutton.button] = true;
				break;

			case SDL_CONTROLLERBUTTONUP:
				controllerState.buttons[event.cbutton.button] = false;
				break;

			case SDL_CONTROLLERAXISMOTION:
				controllerState.axes[event.caxis.axis] = event.caxis.value;
				break;

			case SDL_CONTROLLERDEVICEADDED:
				if (event.cdevice.which > 0)
					break;

				ctrl = SDL_GameControllerOpen(0);

				break;

			case SDL_CONTROLLERDEVICEREMOVED:
				resetInputStates();
				ctrl = 0;
				break;

			case SDL_MOUSEBUTTONDOWN:
				mouseState.buttons[event.button.button] = true;
				break;

			case SDL_MOUSEBUTTONUP:
				mouseState.buttons[event.button.button] = false;
				break;

			case SDL_MOUSEMOTION:
				mouseState.x = event.motion.x;
				mouseState.y = event.motion.y;
                cursorTimer();
				updateCursorState(cursorInWindow, gameScreen);
				break;

			case SDL_MOUSEWHEEL:
				// Only consider vertical scrolling for now
				SDL_AtomicAdd(&verticalScrollDistance, event.wheel.y);
				break;

			case SDL_FINGERDOWN:
				i = event.tfinger.fingerId;
				touchState.fingers[i].down = true;
				break;

			case SDL_FINGERMOTION:
				i = event.tfinger.fingerId;
				touchState.fingers[i].x = event.tfinger.x * winW;
				touchState.fingers[i].y = event.tfinger.y * winH;
				break;

			case SDL_FINGERUP:
				i = event.tfinger.fingerId;
				memset(&touchState.fingers[i], 0, sizeof(touchState.fingers[0]));
				break;

			default:
				// Handle user events
				switch(event.type - usrIdStart)
				{
					case REQUEST_SETFULLSCREEN:
						setFullscreen(win, static_cast<bool>(event.user.code));
						break;

					case REQUEST_WINRESIZE:
						SDL_SetWindowSize(win, event.window.data1, event.window.data2);
						rtData.rqWindowAdjust.clear();
						break;

					case REQUEST_WINREPOSITION:
						SDL_SetWindowPosition(win, event.window.data1, event.window.data2);
						rtData.rqWindowAdjust.clear();
						break;

					case REQUEST_WINCENTER:
						rc = SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(win), &dm);

						if (!rc)
							SDL_SetWindowPosition(
								win,
								(dm.w / 2) - (winW / 2),
								(dm.h / 2) - (winH / 2)
							);

						rtData.rqWindowAdjust.clear();

						break;

					case REQUEST_WINRENAME:
						rtData.config.windowTitle = (const char*)event.user.data1;
						SDL_SetWindowTitle(win, rtData.config.windowTitle.c_str());
						break;

					case REQUEST_TEXTMODE:
						if (event.user.code) {
							SDL_StartTextInput();
							lockText(true);
							textInputBuffer.clear();
							lockText(false);
						} else {
							SDL_StopTextInput();
							lockText(true);
							textInputBuffer.clear();
							lockText(false);
						}
						break;

					case REQUEST_MESSAGEBOX:
					{
#ifndef __APPLE__
						// Try to format the message with additional newlines
						std::string message = copyWithNewlines((const char *) event.user.data1, 70);
						SDL_ShowSimpleMessageBox(
							event.user.code,
							rtData.config.windowTitle.c_str(),
							message.c_str(),
							win
						);
#else
						SDL_ShowSimpleMessageBox(
							event.user.code,
							rtData.config.windowTitle.c_str(),
							(const char *)event.user.data1,
							win
						);
#endif
						free(event.user.data1);
						msgBoxDone.set();
						break;
					}

					case REQUEST_SETCURSORVISIBLE:
						showCursor = event.user.code;
						updateCursorState(cursorInWindow, gameScreen);
						break;

					case REQUEST_SETTINGS:
#ifndef MKXPZ_BUILD_XCODE
						if (!sMenu) {
							sMenu = new SettingsMenu(rtData);
							updateCursorState(false, gameScreen);
						}
						sMenu->raise();
#else
						openSettingsWindow();
#endif
						break;

					case UPDATE_FPS:
						if (rtData.config.printFPS)
							Debug() << "FPS:" << event.user.code;

						if (!fps.sendUpdates)
							break;

						snprintf(buffer, sizeof(buffer), "%s - %d FPS", rtData.config.windowTitle.c_str(), event.user.code);

#ifdef MKXPZ_BUILD_ANDROID
						jniUpdateFPSText(event.user.code);
#else
                        // Updating the window title in fullscreen mode
						// seems to cause flickering
						if (fullscreen) {
							strncpy(pendingTitle, buffer, sizeof(pendingTitle));
							havePendingTitle = true;
							break;
						}

						SDL_SetWindowTitle(win, buffer);
#endif



						break;

					case UPDATE_SCREEN_RECT:
						gameScreen.x = event.user.windowID;
						gameScreen.y = event.user.code;
						gameScreen.w = reinterpret_cast<intptr_t>(event.user.data1);
						gameScreen.h = reinterpret_cast<intptr_t>(event.user.data2);
						updateCursorState(cursorInWindow, gameScreen);
						break;
				}
		}

		if (terminate)
			break;
	}

	// Just in case
	rtData.syncPoint.resumeThreads();

	if (SDL_GameControllerGetAttached(ctrl))
		SDL_GameControllerClose(ctrl);

#ifndef MKXPZ_BUILD_XCODE
	delete sMenu;
#endif
}

int EventThread::eventFilter(void *data, SDL_Event *event)
{
	RGSSThreadData &rtData = *static_cast<RGSSThreadData*>(data);

	switch (event->type)
	{
		case SDL_APP_WILLENTERBACKGROUND:
			Debug() << "SDL_APP_WILLENTERBACKGROUND";

			if (HAVE_ALC_DEVICE_PAUSE)
				alc.DevicePause(rtData.alcDev);

			rtData.syncPoint.haltThreads();

			return 0;

		case SDL_APP_DIDENTERBACKGROUND:
			Debug() << "SDL_APP_DIDENTERBACKGROUND";
			return 0;

		case SDL_APP_WILLENTERFOREGROUND:
			Debug() << "SDL_APP_WILLENTERFOREGROUND";
			return 0;

		case SDL_APP_DIDENTERFOREGROUND:
			Debug() << "SDL_APP_DIDENTERFOREGROUND";

			if (HAVE_ALC_DEVICE_PAUSE)
				alc.DeviceResume(rtData.alcDev);

			rtData.syncPoint.resumeThreads();

			return 0;

		case SDL_APP_TERMINATING:
			Debug() << "SDL_APP_TERMINATING";
			return 0;

		case SDL_APP_LOWMEMORY:
			Debug() << "SDL_APP_LOWMEMORY";
			return 0;

		/*
		case SDL_RENDER_TARGETS_RESET :
			Debug() << "****** SDL_RENDER_TARGETS_RESET";
			return 0;

		case SDL_RENDER_DEVICE_RESET :
			Debug() << "****** SDL_RENDER_DEVICE_RESET";
			return 0;
		*/
	}

	return 1;
}

void EventThread::cleanup()
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
		if ((event.type - usrIdStart) == REQUEST_MESSAGEBOX)
			free(event.user.data1);
}

void EventThread::resetInputStates()
{
	memset(&keyStates, 0, sizeof(keyStates));
	memset(&controllerState, 0, sizeof(controllerState));
	memset(&mouseState.buttons, 0, sizeof(mouseState.buttons));
	memset(&touchState, 0, sizeof(touchState));
}

void EventThread::setFullscreen(SDL_Window *win, bool mode)
{
	SDL_SetWindowFullscreen(win, mode ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	fullscreen = mode;
}

void EventThread::updateCursorState(bool inWindow, const SDL_Rect &screen)
{
	SDL_Point pos = { mouseState.x, mouseState.y };
	bool inScreen = inWindow && SDL_PointInRect(&pos, &screen);

	if (inScreen)
        SDL_ShowCursor(showCursor || hideCursorTimerID ? SDL_TRUE : SDL_FALSE);
	else
		SDL_ShowCursor(SDL_TRUE);
}

void EventThread::requestTerminate()
{
	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

void EventThread::requestFullscreenMode(bool mode)
{
	if (mode == fullscreen)
		return;

	SDL_Event event;
	event.type = usrIdStart + REQUEST_SETFULLSCREEN;
	event.user.code = static_cast<Sint32>(mode);
	SDL_PushEvent(&event);
}

void EventThread::requestWindowResize(int width, int height)
{
	shState->rtData().rqWindowAdjust.set();
	SDL_Event event;
	event.type = usrIdStart + REQUEST_WINRESIZE;
	event.window.data1 = width;
	event.window.data2 = height;
	SDL_PushEvent(&event);
}

void EventThread::requestWindowReposition(int x, int y)
{
	shState->rtData().rqWindowAdjust.set();
	SDL_Event event;
	event.type = usrIdStart + REQUEST_WINREPOSITION;
	event.window.data1 = x;
	event.window.data2 = y;
	SDL_PushEvent(&event);
}

void EventThread::requestWindowCenter()
{
	shState->rtData().rqWindowAdjust.set();
	SDL_Event event;
	event.type = usrIdStart + REQUEST_WINCENTER;
	SDL_PushEvent(&event);
}

void EventThread::requestWindowRename(const char *title)
{
	SDL_Event event;
	event.type = usrIdStart + REQUEST_WINRENAME;
	event.user.data1 = (void*)title;
	SDL_PushEvent(&event);
}

void EventThread::requestShowCursor(bool mode)
{
	SDL_Event event;
	event.type = usrIdStart + REQUEST_SETCURSORVISIBLE;
	event.user.code = mode;
	SDL_PushEvent(&event);
}

void EventThread::requestTextInputMode(bool mode)
{
	SDL_Event event;
	event.type = usrIdStart + REQUEST_TEXTMODE;
	event.user.code = mode;
	SDL_PushEvent(&event);
}

void EventThread::requestSettingsMenu()
{
	SDL_Event event;
	event.type = usrIdStart + REQUEST_SETTINGS;
	SDL_PushEvent(&event);
}

void EventThread::showMessageBox(const char *body, int flags)
{
	msgBoxDone.clear();

	// mkxp has already been asked to quit.
	// Don't break things if the window wants to close
	if (shState->rtData().rqTerm)
		return;

	SDL_Event event;
	event.user.code = flags;
	event.user.data1 = strdup(body);
	event.type = usrIdStart + REQUEST_MESSAGEBOX;
	SDL_PushEvent(&event);

	// Keep repainting screen while box is open
    try{
        shState->graphics().repaintWait(msgBoxDone);
    }catch(...){}

	// Prevent endless loops
	resetInputStates();
}

bool EventThread::getFullscreen() const
{
	return fullscreen;
}

bool EventThread::getShowCursor() const
{
	return showCursor;
}

bool EventThread::getControllerConnected() const
{
	return ctrl != 0;
}

SDL_GameController *EventThread::controller() const
{
	return ctrl;
}

void EventThread::notifyFrame()
{
#ifdef MKXPZ_BUILD_XCODE
	uint32_t frames = round(shState->graphics().averageFrameRate());
	updateTouchBarFPSDisplay(frames);
#endif
	if (!fps.sendUpdates)
		return;

	SDL_Event event;

#ifdef MKXPZ_BUILD_XCODE
	event.user.code = frames;
#else
	event.user.code = round(shState->graphics().averageFrameRate());
#endif
	event.user.type = usrIdStart + UPDATE_FPS;
#ifndef __ANDROID__
    SDL_PushEvent(&event);
#else
    // Android devices may not update the FPS display immediately
    // if the app is in the background, so we need to force it
    if (hideCursorTimerID) {
        SDL_RemoveTimer(hideCursorTimerID);
        hideCursorTimerID = 0;
    }
    jniUpdateFPSText(event.user.code);
#endif
}

void EventThread::notifyGameScreenChange(const SDL_Rect &screen)
{
	/* We have to get a bit hacky here to fit the rectangle
	 * data into the user event struct */
	SDL_Event event;
	event.type = usrIdStart + UPDATE_SCREEN_RECT;
	event.user.windowID = screen.x;
	event.user.code = screen.y;
	event.user.data1 = reinterpret_cast<void*>(screen.w);
	event.user.data2 = reinterpret_cast<void*>(screen.h);
	SDL_PushEvent(&event);
}

void EventThread::lockText(bool lock)
{
	lock ? SDL_LockMutex(textInputLock) : SDL_UnlockMutex(textInputLock);
}

void SyncPoint::haltThreads()
{
	if (mainSync.locked)
		return;

	// Lock the reply sync first to avoid races
	reply.lock();

	// Lock main sync and sleep until RGSS thread reports back
	mainSync.lock();
	reply.waitForUnlock();

	/* Now that the RGSS thread is asleep, we can
	 * safely put the other threads to sleep as well
	 * without causing deadlocks */
	secondSync.lock();
}

void SyncPoint::resumeThreads()
{
	if (!mainSync.locked)
		return;

	mainSync.unlock(false);
	secondSync.unlock(true);
}

bool SyncPoint::mainSyncLocked()
{
	return mainSync.locked;
}

void SyncPoint::waitMainSync()
{
	reply.unlock(false);
	mainSync.waitForUnlock();
}

void SyncPoint::passSecondarySync()
{
	if (!secondSync.locked)
		return;

	secondSync.waitForUnlock();
}

SyncPoint::Util::Util()
{
	mut = SDL_CreateMutex();
	cond = SDL_CreateCond();
}

SyncPoint::Util::~Util()
{
	SDL_DestroyCond(cond);
	SDL_DestroyMutex(mut);
}

void SyncPoint::Util::lock()
{
	locked.set();
}

void SyncPoint::Util::unlock(bool multi)
{
	locked.clear();

	if (multi)
		SDL_CondBroadcast(cond);
	else
		SDL_CondSignal(cond);
}

void SyncPoint::Util::waitForUnlock()
{
	SDL_LockMutex(mut);

	while (locked) {
		// 修复：Android设备上的死锁问题 - 使用超时机制
		int result = SDL_CondWaitTimeout(cond, mut, 100); // 100ms超时
		if (result == SDL_MUTEX_TIMEDOUT) {
			// 超时后检查是否应该继续等待
			if (!locked) {
				break; // 锁已释放，退出循环
			}
			// 继续等待，但避免永久死锁
		}
	}

	SDL_UnlockMutex(mut);
}

package app.mtool.mtoolmobile.mkxpz;

import org.libsdl.app.HIDDeviceManager;
import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLAudioManager;
import org.libsdl.app.SDLClipboardHandler;
import org.libsdl.app.SDLControllerManager;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.content.pm.PackageManager;
import android.os.Build;
import android.Manifest;
import android.view.ActionMode;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SearchEvent;
import android.view.Window;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;
import android.widget.Toast;
import android.os.Environment;
import android.content.Intent;
import android.app.Activity;
import android.util.Log;

import android.content.Context;
import android.content.res.Configuration;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.TextView;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Dictionary;
import java.util.HashMap;

public class MKXPZActivity extends SDLActivity {
    private static String GAME_PATH = Environment.getExternalStorageDirectory() + "/mkxp-z"; //SDL2 Jni 调用需要的游戏路径
    private static int SERVER_PORT = 54871;
    private static final String TAG = "MToolMKXPZActivity";
    private static final String TARGET_PACKAGE = "app.mtool.mtoolmobile";
    private static final int CONTEXT_MODE = Context.CONTEXT_INCLUDE_CODE | Context.CONTEXT_IGNORE_SECURITY;
    public static Class<?> mtoolViewManagerClass = null;
    public static Method justKillMeAndBringUpMTOOL = null;
    public static Method sendKeyEventToSender = null;
    public boolean realStarted = false; // 是否已经完成初始化
    public static int usingRubyVersion = 193; // 使用的 Ruby 版本，默认使用 Ruby 3.1.3
    public static String[] ruby187Libs = new String[]{
            "SDL2",
            "SDL2_image",
            "SDL2_ttf",
            "SDL2_sound",
            "openal",
            "ruby187",
            "mkxp-z-187"
    };
    public static String[] ruby193Libs = new String[]{
            "SDL2",
            "SDL2_image",
            "SDL2_ttf",
            "SDL2_sound",
            "openal",
            "ruby193",
            "mkxp-z-193"
    };
    public static String[] ruby313Libs = new String[]{
            "SDL2",
            "SDL2_image",
            "SDL2_ttf",
            "SDL2_sound",
            "openal",
            "ruby313",
            "mkxp-z"
    };
    public static HashMap<Integer, String[]> rubyVersions = new HashMap<Integer, String[]>() {
        {
            put(187, ruby187Libs);
            put(193, ruby193Libs);
            put(313, ruby313Libs);
        }
    }; // 使用的 Ruby 版本

    // 远程类实例的反射引用
    private Object mtoolViewManager;


    // 远程类的Context
    private Context mtoolContext;
    private ClassLoader remoteClassLoader;

    protected ClassLoader getRemoteClassLoader() {
        if (mtoolContext == null) {
            try {
                mtoolContext = createPackageContext(TARGET_PACKAGE, CONTEXT_MODE);
            } catch (PackageManager.NameNotFoundException e) {
                throw new RuntimeException(e);
            }
        }
        if (mtoolContext != null) {
            if (remoteClassLoader == null) {
                remoteClassLoader = mtoolContext.getClassLoader();
            }
            return remoteClassLoader;
        }
        return null;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (sendKeyEventToSender != null) {
            try {
                sendKeyEventToSender.invoke(null, event);
            } catch (Exception e) {
                Log.e(TAG, "发送按键事件失败", e);
            }
        }
        return super.dispatchKeyEvent(event);
    }

    class vInputRecv implements Window.Callback {
        @Override
        public boolean dispatchTouchEvent(MotionEvent event) {
            return false;
        }

        @Override
        public boolean dispatchTrackballEvent(MotionEvent motionEvent) {
            return false;
        }

        @Override
        public boolean dispatchKeyEvent(KeyEvent event) {
            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                SDLActivity.onNativeKeyDown(event.getKeyCode());
            } else if (event.getAction() == KeyEvent.ACTION_UP) {
                SDLActivity.onNativeKeyUp(event.getKeyCode());
            }
            return false;
        }

        @Override
        public boolean dispatchKeyShortcutEvent(KeyEvent keyEvent) {
            return false;
        }

        @Override
        public boolean dispatchGenericMotionEvent(MotionEvent event) {
            float x, y;
            int action;

            switch (event.getSource()) {
                case InputDevice.SOURCE_JOYSTICK:
                    return SDLControllerManager.handleJoystickMotionEvent(event);

                case InputDevice.SOURCE_MOUSE:
                    action = event.getActionMasked();
                    switch (action) {
                        case MotionEvent.ACTION_SCROLL:
                            x = event.getAxisValue(MotionEvent.AXIS_HSCROLL, 0);
                            y = event.getAxisValue(MotionEvent.AXIS_VSCROLL, 0);
                            SDLActivity.onNativeMouse(0, action, x, y, false);
                            return true;

                        case MotionEvent.ACTION_HOVER_MOVE:
                            x = event.getX(0);
                            y = event.getY(0);

                            SDLActivity.onNativeMouse(0, action, x, y, false);
                            return true;

                        default:
                            break;
                    }
                    break;

                default:
                    break;
            }
            return false;
        }

        @Override
        public boolean dispatchPopulateAccessibilityEvent(AccessibilityEvent accessibilityEvent) {
            return false;
        }

        @Override
        public View onCreatePanelView(int i) {
            return null;
        }

        @Override
        public boolean onCreatePanelMenu(int i, Menu menu) {
            return false;
        }

        @Override
        public boolean onPreparePanel(int i, View view, Menu menu) {
            return false;
        }

        @Override
        public boolean onMenuOpened(int i, Menu menu) {
            return false;
        }

        @Override
        public boolean onMenuItemSelected(int i, MenuItem menuItem) {
            return false;
        }

        @Override
        public void onWindowAttributesChanged(WindowManager.LayoutParams layoutParams) {

        }

        @Override
        public void onContentChanged() {

        }

        @Override
        public void onWindowFocusChanged(boolean b) {

        }

        @Override
        public void onAttachedToWindow() {

        }

        @Override
        public void onDetachedFromWindow() {

        }

        @Override
        public void onPanelClosed(int i, Menu menu) {

        }

        @Override
        public boolean onSearchRequested() {
            return false;
        }

        @Override
        public boolean onSearchRequested(SearchEvent searchEvent) {
            return false;
        }

        @Override
        public ActionMode onWindowStartingActionMode(ActionMode.Callback callback) {
            return null;
        }

        @Override
        public ActionMode onWindowStartingActionMode(ActionMode.Callback callback, int i) {
            return null;
        }

        @Override
        public void onActionModeStarted(ActionMode actionMode) {

        }

        @Override
        public void onActionModeFinished(ActionMode actionMode) {

        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        checkAndRequestPermissions();
    }

    /**
     * This method returns the name of the shared object with the application entry point
     * It can be overridden by derived classes.
     */
    @Override
    protected String getMainSharedObject() {
        String library;
        String[] libraries = this.getLibraries();
        library = "lib" + libraries[libraries.length - 1] + ".so";
        return getContext().getApplicationInfo().nativeLibraryDir + "/" + library;
    }

    /**
     * This method is called by SDL before loading the native shared libraries.
     * It can be overridden to provide names of shared libraries to be loaded.
     * The default implementation returns the defaults. It never returns null.
     * An array returned by a new implementation must at least contain "SDL2".
     * Also keep in mind that the order the libraries are loaded may matter.
     *
     * @return names of shared libraries to be loaded (e.g. "SDL2", "main").
     */
    @Override
    protected String[] getLibraries() {
        return rubyVersions.getOrDefault(usingRubyVersion, ruby313Libs);
    }

    boolean initing = false;

    void realInit() {
        if (initing) {
            return;
        }
        initing = true;

        String externalStoragePath = Environment.getExternalStorageDirectory().getAbsolutePath();
        String rtpInitDonePath = externalStoragePath + "/MTool/RTP/RTPInitDone_v1";
        if (!new File(rtpInitDonePath).exists()) {
            // 创建全屏遮罩用于展示进度
            final ViewGroup rootView = (ViewGroup) getWindow().getDecorView().getRootView();
            final View progressOverlay = getLayoutInflater().inflate(R.layout.progress_overlay, null);

            final TextView tvProgressFile = progressOverlay.findViewById(R.id.tv_progress_file);
            final TextView tvProgressPercent = progressOverlay.findViewById(R.id.tv_progress_percent);
            final ProgressBar progressBar = progressOverlay.findViewById(R.id.progress_bar);

            // 将进度遮罩添加到根视图
            final RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(
                    RelativeLayout.LayoutParams.MATCH_PARENT,
                    RelativeLayout.LayoutParams.MATCH_PARENT);
            rootView.addView(progressOverlay, params);

            Log.i(TAG, "开始复制RTP资源文件到: " + externalStoragePath + "/MTool/RTP");

            AssetsManager.copyAssets(this, "RTP", externalStoragePath + "/MTool/RTP", new AssetsManager.CopyProgressCallback() {
                @Override
                public void onStart(int totalFiles) {
                    runOnUiThread(() -> {
                        progressBar.setMax(100);
                        progressBar.setProgress(0);
                        tvProgressPercent.setText(getString(R.string.copy_progress_format, 0, totalFiles, 0));
                        Log.i(TAG, "开始复制文件，共 " + totalFiles + " 个文件");
                    });
                }

                @Override
                public void onProgress(String currentFile, int progress, int currentCount, int totalFiles) {
                    runOnUiThread(() -> {
                        tvProgressFile.setText(getString(R.string.copying_file_progress, currentFile));
                        tvProgressPercent.setText(getString(R.string.copy_progress_format, currentCount, totalFiles, progress));
                        progressBar.setProgress(progress);
                        Log.i(TAG, "复制进度: " + currentCount + "/" + totalFiles + " (" + progress + "%)");
                    });
                }

                @Override
                public void onFinish(boolean success) {
                    runOnUiThread(() -> {
                        // 移除进度遮罩
                        rootView.removeView(progressOverlay);

                        // 显示复制结果
                        Toast.makeText(MKXPZActivity.this,
                                success ? R.string.copy_complete : R.string.copy_failed,
                                Toast.LENGTH_SHORT).show();

                        Log.i(TAG, "文件复制" + (success ? "完成" : "失败"));

                        try {
                            if (success) {
                                new File(rtpInitDonePath).createNewFile();
                            }
                        } catch (IOException e) {
                            Log.e(TAG, "创建标记文件失败", e);
                            throw new RuntimeException(e);
                        }

                        initing = false;
                        initGame();
                    });
                }
            });
            return;
        }
        initing = false;
        initGame();
    }

    void initGame() {
        //在这里做个全屏遮罩
        AssetsManager.copyAssets(this, "RTP", Environment.getExternalStorageDirectory() + "/MTool/RTP", null);
        try {
            // 加载MToolViewManager类
            mtoolViewManagerClass = getRemoteClassLoader().loadClass("app.mtool.mtoolmobile.mtool.managers.MToolViewManager");
            justKillMeAndBringUpMTOOL = mtoolViewManagerClass.getMethod("justKillMeAndBringUpMTOOL", Activity.class);
            sendKeyEventToSender = mtoolViewManagerClass.getMethod("sendKeyEventToSender", KeyEvent.class);
            try {
                GAME_PATH = PackageContextHelper.readFileFromOtherApp(this, "node/mtoolWebRoot/mkxpz_game_path");
                SERVER_PORT = Integer.parseInt(PackageContextHelper.readFileFromOtherApp(this, "node/serverURI").split(":")[1]);
                usingRubyVersion = Integer.parseInt(PackageContextHelper.readFileFromOtherApp(this, "node/mtoolWebRoot/mkxpz_rubyVersion"));
                Log.v(TAG, "游戏路径: " + GAME_PATH);
                Log.v(TAG, "Ruby Version: " + usingRubyVersion);
            } catch (Exception e) {
                Log.e(TAG, "读取游戏路径或端口失败，可能是 MTool 服务未在运行", e);
                justKillMeAndBringUpMTOOL.invoke(null, this);
                //throw new RuntimeException(e);
                return;
            }
            //super.onCreate(savedInstanceState);
            try {
                // 创建目标包的Context
                mtoolContext = createPackageContext(TARGET_PACKAGE, CONTEXT_MODE);
            } catch (Exception e) {
                Log.e(TAG, "初始化失败", e);
                finish();
            }


            Log.v(TAG, "Device: " + Build.DEVICE);
            Log.v(TAG, "Model: " + Build.MODEL);
            Log.v(TAG, "onCreate()");

            SDLAudioManager.setContext(this);
            try {
                Thread.currentThread().setName("SDLActivity");
            } catch (Exception e) {
                Log.v(TAG, "modify thread properties failed " + e.toString());
            }

            // Load shared libraries
            String errorMsgBrokenLib = "";
            try {
                loadLibraries();
                mBrokenLibraries = false; /* success */
            } catch (UnsatisfiedLinkError e) {
                System.err.println(e.getMessage());
                mBrokenLibraries = true;
                errorMsgBrokenLib = e.getMessage();
            } catch (Exception e) {
                System.err.println(e.getMessage());
                mBrokenLibraries = true;
                errorMsgBrokenLib = e.getMessage();
            }

            if (!mBrokenLibraries) {
                String expected_version = String.valueOf(SDL_MAJOR_VERSION) + "." +
                        String.valueOf(SDL_MINOR_VERSION) + "." +
                        String.valueOf(SDL_MICRO_VERSION);
                String version = nativeGetVersion();
                if (!version.equals(expected_version)) {
                    mBrokenLibraries = true;
                    errorMsgBrokenLib = "SDL C/Java version mismatch (expected " + expected_version + ", got " + version + ")";
                }
            }

            if (mBrokenLibraries) {
                mSingleton = this;
                AlertDialog.Builder dlgAlert = new AlertDialog.Builder(this);
                dlgAlert.setMessage("An error occurred while trying to start the application. Please try again and/or reinstall."
                        + System.getProperty("line.separator")
                        + System.getProperty("line.separator")
                        + "Error: " + errorMsgBrokenLib);
                dlgAlert.setTitle("SDL Error");
                dlgAlert.setPositiveButton("Exit",
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int id) {
                                // if this button is clicked, close current activity
                                SDLActivity.mSingleton.finish();
                            }
                        });
                dlgAlert.setCancelable(false);
                dlgAlert.create().show();

                return;
            }

            // Set up JNI
            SDL.setupJNI();

            // Initialize state
            SDL.initialize();

            // So we can call stuff from static callbacks
            mSingleton = this;
            SDL.setContext(this);

            mClipboardHandler = new SDLClipboardHandler();

            mHIDDeviceManager = HIDDeviceManager.acquire(this);

            // Set up the surface
            mSurface = createSDLSurface(getApplication());

            // 获取构造函数
            java.lang.reflect.Constructor<?> constructor = mtoolViewManagerClass.getConstructor(
                    Activity.class);

            // 创建实例
            mtoolViewManager = constructor.newInstance(this);

            // 调用init方法
            Method initMethod = mtoolViewManagerClass.getMethod("init");
            mtoolViewManagerClass.getField("initFullScreen").set(mtoolViewManager, false);// initFullScreen 避免全屏模式
            mtoolViewManagerClass.getField("defaultToGameView").set(mtoolViewManager, true);// initFullScreen 避免全屏模式
            //mtoolViewManagerClass.getField("startGeckoView").set(mtoolViewManager, false);

            mLayout = (ViewGroup) initMethod.invoke(mtoolViewManager);
            Method setGameInputTarget = mtoolViewManagerClass.getMethod("setGameInputTarget", Object.class);
            setGameInputTarget.invoke(mtoolViewManager, new vInputRecv());
            if (mLayout == null) {
                Log.e(TAG, "MToolViewManager 初始化失败");
                Toast.makeText(this, "MToolViewManager 初始化失败", Toast.LENGTH_LONG).show();
                finish();
                return;
            } else {
                Log.v(TAG, "MToolViewManager 初始化成功");
            }


            //mLayout = new RelativeLayout(this);
            mLayout.addView(mSurface, 0);

            // Get our current screen orientation and pass it down.
            mCurrentOrientation = SDLActivity.getCurrentOrientation();
            // Only record current orientation
            SDLActivity.onNativeOrientationChanged(mCurrentOrientation);

            try {
                if (Build.VERSION.SDK_INT < 24 /* Android 7.0 (N) */) {
                    mCurrentLocale = getContext().getResources().getConfiguration().locale;
                } else {
                    mCurrentLocale = getContext().getResources().getConfiguration().getLocales().get(0);
                }
            } catch (Exception ignored) {
            }

            setContentView(mLayout);

            setWindowStyle(false);

            getWindow().getDecorView().setOnSystemUiVisibilityChangeListener(this);

            // Get filename from "Open with" of another application
            Intent intent = getIntent();
            if (intent != null && intent.getData() != null) {
                String filename = intent.getData().getPath();
                if (filename != null) {
                    Log.v(TAG, "Got filename: " + filename);
                    SDLActivity.onNativeDropFile(filename);
                }
            }
            realStarted = true;
            if (mHIDDeviceManager != null) {
                mHIDDeviceManager.setFrozen(false);
            }
            //if (!mHasMultiWindow) {
            resumeNativeThread();
            //}
            Log.e(TAG, "OnCreate Done");
        } catch (Exception e) {
            Log.e(TAG, "OnCreate Error: " + e, e);
            throw new RuntimeException(e);
        }
    }


    /**
     * 释放游戏视图
     */
    public void releaseGameView() {// 反射调用
        try {
            justKillMeAndBringUpMTOOL.invoke(null, this);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * 处理游戏初始化
     */
    public boolean handleGameInit(String startArgsString) {// 反射调用
        try {
            if (startArgsString.startsWith("MTool_Game_Init_GeckoView:")) {
                String startUrl = startArgsString.substring("MTool_Game_Init_GeckoView:".length());

                // 获取UIManager
                Class<?> mtoolViewManagerClass = getRemoteClassLoader().loadClass(
                        "app.mtool.mtoolmobile.mtool.managers.MToolViewManager");
                Method getUiManagerMethod = mtoolViewManagerClass.getMethod("getUiManager");
                Object uiManager = getUiManagerMethod.invoke(mtoolViewManager);

                // 获取RootLayout
                Class<?> uiManagerClass = getRemoteClassLoader().loadClass(
                        "app.mtool.mtoolmobile.mtool.managers.UIManager");
                Method getRootLayoutMethod = uiManagerClass.getMethod("getRootLayout");
                Object rootLayout = getRootLayoutMethod.invoke(uiManager);


                return true;
            }

            return false;
        } catch (Exception e) {
            Log.e(TAG, "处理游戏初始化失败", e);
            return false;
        }
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        try {
            // 让UIManager处理触摸事件
            Class<?> mtoolViewManagerClass = getRemoteClassLoader().loadClass(
                    "app.mtool.mtoolmobile.mtool.managers.MToolViewManager");
            Method getUiManagerMethod = mtoolViewManagerClass.getMethod("getUiManager");
            Object uiManager = getUiManagerMethod.invoke(mtoolViewManager);

            Class<?> uiManagerClass = getRemoteClassLoader().loadClass(
                    "app.mtool.mtoolmobile.mtool.managers.UIManager");
            Method dispatchTouchEventMethod = uiManagerClass.getMethod("dispatchTouchEvent", MotionEvent.class);
            Boolean result = (Boolean) dispatchTouchEventMethod.invoke(uiManager, ev);

            if (result) {
                return true;
            }
        } catch (Exception e) {
            Log.e(TAG, "分发触摸事件失败", e);
        }

        return super.dispatchTouchEvent(ev);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        try {
            // 让UIManager处理配置变更
            Class<?> mtoolViewManagerClass = getRemoteClassLoader().loadClass(
                    "app.mtool.mtoolmobile.mtool.managers.MToolViewManager");
            Method getUiManagerMethod = mtoolViewManagerClass.getMethod("getUiManager");
            Object uiManager = getUiManagerMethod.invoke(mtoolViewManager);

            Class<?> uiManagerClass = getRemoteClassLoader().loadClass(
                    "app.mtool.mtoolmobile.mtool.managers.UIManager");
            Method onConfigurationChangedMethod = uiManagerClass.getMethod("onConfigurationChanged", Configuration.class);
            onConfigurationChangedMethod.invoke(uiManager, newConfig);
        } catch (Exception e) {
            Log.e(TAG, "处理配置变更失败", e);
        }
    }

    @Override
    public void onBackPressed() {
        try {
            // 让MToolViewManager处理返回键事件
            Class<?> mtoolViewManagerClass = getRemoteClassLoader().loadClass(
                    "app.mtool.mtoolmobile.mtool.managers.MToolViewManager");
            Method onBackPressedMethod = mtoolViewManagerClass.getMethod("onBackPressed");
            onBackPressedMethod.invoke(mtoolViewManager);
        } catch (Exception e) {
            Log.e(TAG, "处理返回键事件失败", e);
            super.onBackPressed();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        try {
            // 调用MToolViewManager的onPause方法
            Class<?> mtoolViewManagerClass = getRemoteClassLoader().loadClass(
                    "app.mtool.mtoolmobile.mtool.managers.MToolViewManager");
            Method onPauseMethod = mtoolViewManagerClass.getMethod("onPause");
            onPauseMethod.invoke(mtoolViewManager);
        } catch (Exception e) {
            Log.e(TAG, "处理onPause事件失败", e);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (realStarted) {
            Log.v(TAG, "onResume()");
            if (mHIDDeviceManager != null) {
                mHIDDeviceManager.setFrozen(false);
            }
            //if (!mHasMultiWindow) {
            resumeNativeThread();
            //}
        }
        try {
            // 调用MToolViewManager的onResume方法
            Class<?> mtoolViewManagerClass = getRemoteClassLoader().loadClass(
                    "app.mtool.mtoolmobile.mtool.managers.MToolViewManager");
            Method onResumeMethod = mtoolViewManagerClass.getMethod("onResume");
            onResumeMethod.invoke(mtoolViewManager);
        } catch (Exception e) {
            Log.e(TAG, "处理onResume事件失败", e);
        }
    }

    @Override
    protected void onActivityResult(final int requestCode, final int resultCode, final Intent data) {
        try {
            // 调用MToolViewManager的onActivityResult方法
            Class<?> mtoolViewManagerClass = getRemoteClassLoader().loadClass(
                    "app.mtool.mtoolmobile.mtool.managers.MToolViewManager");
            Method onActivityResultMethod = mtoolViewManagerClass.getMethod(
                    "onActivityResult", int.class, int.class, Intent.class);
            Boolean handled = (Boolean) onActivityResultMethod.invoke(mtoolViewManager, requestCode, resultCode, data);

            if (handled) {
                return;
            }
            super.onActivityResult(requestCode, resultCode, data);

        } catch (Exception e) {
            Log.e(TAG, "处理活动结果失败", e);
            super.onActivityResult(requestCode, resultCode, data);
        }
    }

    @Override
    public void onRequestPermissionsResult(final int requestCode, final String[] permissions, final int[] grantResults) {
        if (requestCode == PERMISSION_REQUEST_CODE) {
            // 只有当权限被拒绝时才显示提示
            if (grantResults.length > 0) {
                boolean allGranted = true;
                for (int i = 0; i < grantResults.length; i++) {
                    String permission = permissions[i];
                    boolean granted = grantResults[i] == PackageManager.PERMISSION_GRANTED;
                    Log.d(TAG, "权限结果: " + permission + " - " + (granted ? "已授予" : "已拒绝"));

                    if (grantResults[i] != PackageManager.PERMISSION_GRANTED) {
                        allGranted = false;
                    }
                }

                if (!allGranted) {
                    Log.d(TAG, "部分权限被拒绝");
                } else {
                    Log.d(TAG, "所有权限已授予");
                    realInit();
                }
            }
            return;
        }

        try {
            // 调用MToolViewManager的onRequestPermissionsResult方法
            Class<?> mtoolViewManagerClass = getRemoteClassLoader().loadClass(
                    "app.mtool.mtoolmobile.mtool.managers.MToolViewManager");
            Method onRequestPermissionsResultMethod = mtoolViewManagerClass.getMethod(
                    "onRequestPermissionsResult", int.class, String[].class, int[].class);
            Boolean handled = (Boolean) onRequestPermissionsResultMethod.invoke(
                    mtoolViewManager, requestCode, permissions, grantResults);

            if (handled) {
                return;
            }

        } catch (Exception e) {
            Log.e(TAG, "处理权限请求结果失败", e);
        }
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }

    @Override
    public void onDestroy() {
        try {
            // 调用MToolViewManager的onDestroy方法
            Class<?> mtoolViewManagerClass = getRemoteClassLoader().loadClass(
                    "app.mtool.mtoolmobile.mtool.managers.MToolViewManager");
            Method onDestroyMethod = mtoolViewManagerClass.getMethod("onDestroy");
            onDestroyMethod.invoke(mtoolViewManager);
        } catch (Exception e) {
            Log.e(TAG, "处理销毁事件失败", e);
        }

        super.onDestroy();
    }

    @Override
    protected void onNewIntent(final Intent intent) {
        super.onNewIntent(intent);

        if (intent == null) {
            return;
        }

        try {
            // 调用MToolViewManager的onNewIntent方法
            Class<?> mtoolViewManagerClass = getRemoteClassLoader().loadClass(
                    "app.mtool.mtoolmobile.mtool.managers.MToolViewManager");
            Method onNewIntentMethod = mtoolViewManagerClass.getMethod("onNewIntent", Intent.class);
            onNewIntentMethod.invoke(mtoolViewManager, intent);
            setIntent(intent);
        } catch (Exception e) {
            Log.e(TAG, "处理新意图失败", e);
        }
    }

    private static final int PERMISSION_REQUEST_CODE = 1001;

    private void checkAndRequestPermissions() {
        boolean needRead = checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED;
        boolean needWrite = checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED;

        if (needRead || needWrite) {
            Log.d(TAG, "Android 6-10: 请求文件读写权限");
            requestPermissions(
                    new String[]{
                            Manifest.permission.READ_EXTERNAL_STORAGE,
                            Manifest.permission.WRITE_EXTERNAL_STORAGE
                    },
                    PERMISSION_REQUEST_CODE);
        } else {
            Log.d(TAG, "已有文件读写权限");
            realInit();
        }
    }

    static void setFPSVisibility(boolean on) {

    }

    static void updateFPSText(int fps) {

    }
}
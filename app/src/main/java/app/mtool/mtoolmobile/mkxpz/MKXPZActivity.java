package app.mtool.mtoolmobile.mkxpz;

import org.libsdl.app.HIDDeviceManager;
import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLAudioManager;
import org.libsdl.app.SDLClipboardHandler;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.content.pm.PackageManager;
import android.os.Build;
import android.Manifest;
import android.view.ActionMode;
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

import java.lang.reflect.Method;

public class MKXPZActivity extends SDLActivity {
    private static String GAME_PATH = Environment.getExternalStorageDirectory() + "/mkxp-z"; //SDL2 Jni 调用需要的游戏路径
    private static int SERVER_PORT = 54871;
    private static final String TAG = "MToolMKXPZActivity";
    private static final String TARGET_PACKAGE = "app.mtool.mtoolmobile";
    private static final int CONTEXT_MODE = Context.CONTEXT_INCLUDE_CODE | Context.CONTEXT_IGNORE_SECURITY;
    public static Class<?> mtoolViewManagerClass = null;
    public static Method justKillMeAndBringUpMTOOL = null;

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
        checkAndRequestPermissions();
        try {
            // 加载MToolViewManager类
            mtoolViewManagerClass = getRemoteClassLoader().loadClass(
                    "app.mtool.mtoolmobile.mtool.managers.MToolViewManager");
            justKillMeAndBringUpMTOOL = mtoolViewManagerClass.getMethod("justKillMeAndBringUpMTOOL", Activity.class);
            try {
                GAME_PATH = PackageContextHelper.readFileFromOtherApp(this, "node/mtoolWebRoot/mkxpz_game_path");
                SERVER_PORT = Integer.parseInt(PackageContextHelper.readFileFromOtherApp(this, "node/serverURI").split(":")[1]);
                Log.v(TAG, "游戏路径: " + GAME_PATH);
            } catch (Exception e) {
                finishAndRemoveTask();   // 结束当前 Activity 并把所属 Task 从最近任务里删掉
                justKillMeAndBringUpMTOOL.invoke(null, this);
                throw new RuntimeException(e);
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
            super.onCreate(savedInstanceState);
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
            Log.e(TAG, "OnCreate Done");
        } catch (Exception e) {
            Log.e(TAG, "OnCreate Error: " + e, e);
            throw new RuntimeException(e);
        }
    }


    /**
     * 释放游戏视图
     */
    public void releaseGameView() {
        try {
            justKillMeAndBringUpMTOOL.invoke(null, this);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * 处理游戏初始化
     */
    public boolean handleGameInit(String startArgsString) {
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


                // 设置游戏视图
                //Method setGameViewMethod = uiManagerClass.getMethod("setGameView", View.class);
                //setGameViewMethod.invoke(uiManager, gameView);

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
        try {
            //initMToolViewManager();
        } catch (Exception e) {
            throw new RuntimeException(e);
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
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
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
                }
            }
        } else {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
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
            Log.d(TAG, "Android 6-10: 已有文件读写权限");
        }
    }
}
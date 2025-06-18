/*
 ** binding-mri.cpp
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

#include "audio/audio.h"
#include "filesystem/filesystem.h"
#include "display/graphics.h"
#include "display/font.h"
#include "system/system.h"

#include "util/util.h"
#include "util/sdl-util.h"
#include "util/debugwriter.h"
#include "util/boost-hash.h"
#include "util/exception.h"
#include "util/encoding.h"

#include "config.h"

#include "binding-util.h"
#include "binding.h"

#include "sharedstate.h"
#include "eventthread.h"

#include <vector>
#include "util/rapidcsv.h"

extern "C" {
#include <ruby.h>

#if RAPI_FULL >= 190
#include <ruby/encoding.h>
#endif
}

#ifdef __WIN32__
#include "binding-mri-win32.h"
#endif

#include <assert.h>
#include <string>
#include <zlib.h>

#include <SDL_cpuinfo.h>
#include <SDL_filesystem.h>
#include <SDL_loadso.h>
#include <SDL_power.h>


extern const char module_rpg1[];
extern const char module_rpg2[];
extern const char module_rpg3[];

static void mriBindingExecute();

static void mriBindingTerminate();

static void mriBindingReset();

ScriptBinding scriptBindingImpl = {mriBindingExecute, mriBindingTerminate,
                                   mriBindingReset};

ScriptBinding *scriptBinding = &scriptBindingImpl;

void tableBindingInit();

void etcBindingInit();

void fontBindingInit();

void bitmapBindingInit();

void spriteBindingInit();

void viewportBindingInit();

void planeBindingInit();

void windowBindingInit();

void tilemapBindingInit();

void windowVXBindingInit();

void tilemapVXBindingInit();

void inputBindingInit();

void audioBindingInit();

void graphicsBindingInit();

void fileIntBindingInit();

#ifdef MKXPZ_MINIFFI

void MiniFFIBindingInit();

#endif

#ifdef MKXPZ_STEAM
void CUSLBindingInit();
#endif

void httpBindingInit();

RB_METHOD(mkxpDelta);

RB_METHOD(mriPrint);

RB_METHOD(mriP);

RB_METHOD(mkxpDataDirectory);

RB_METHOD(mkxpSetTitle);

RB_METHOD(mkxpGetTitle);

RB_METHOD(mkxpDesensitize);

RB_METHOD(mkxpPuts);

RB_METHOD(mkxpPlatform);

RB_METHOD(mkxpIsMacHost);

RB_METHOD(mkxpIsWindowsHost);

RB_METHOD(mkxpIsLinuxHost);

RB_METHOD(mkxpIsUsingRosetta);

RB_METHOD(mkxpIsUsingWine);

RB_METHOD(mkxpIsReallyMacHost);

RB_METHOD(mkxpIsReallyLinuxHost);

RB_METHOD(mkxpIsReallyWindowsHost);

RB_METHOD(mkxpUserLanguage);

RB_METHOD(mkxpUserName);

RB_METHOD(mkxpGameTitle);

RB_METHOD(mkxpPowerState);

RB_METHOD(mkxpSettingsMenu);

RB_METHOD(mkxpCpuCount);

RB_METHOD(mkxpSystemMemory);

RB_METHOD(mkxpReloadPathCache);

RB_METHOD(mkxpAddPath);

RB_METHOD(mkxpRemovePath);

RB_METHOD(mkxpFileExists);

RB_METHOD(mkxpLaunch);

RB_METHOD(mkxpGetJSONSetting);

RB_METHOD(mkxpSetJSONSetting);

RB_METHOD(mkxpGetAllJSONSettings);

RB_METHOD(mkxpSetDefaultFontFamily);

RB_METHOD(mriRgssMain);

RB_METHOD(mriRgssStop);

RB_METHOD(_kernelCaller);

RB_METHOD(mkxpStringToUTF8);

RB_METHOD(mkxpStringToUTF8Bang);

VALUE json2rb(json5pp::value const &v);

json5pp::value rb2json(VALUE v);

RB_METHOD(mkxpParseCSV);

static void mriBindingInit() {
    tableBindingInit();
    etcBindingInit();
    fontBindingInit();
    bitmapBindingInit();
    spriteBindingInit();
    viewportBindingInit();
    planeBindingInit();

    if (rgssVer == 1) {
        windowBindingInit();
        tilemapBindingInit();
    } else {
        windowVXBindingInit();
        tilemapVXBindingInit();
    }

    inputBindingInit();
    audioBindingInit();
    graphicsBindingInit();

    fileIntBindingInit();

#ifdef MKXPZ_MINIFFI
    MiniFFIBindingInit();
#endif

#ifdef MKXPZ_STEAM
    CUSLBindingInit();
#endif

    httpBindingInit();

    if (rgssVer >= 3) {
        _rb_define_module_function(rb_mKernel, "rgss_main", mriRgssMain);
        _rb_define_module_function(rb_mKernel, "rgss_stop", mriRgssStop);

        _rb_define_module_function(rb_mKernel, "msgbox", mriPrint);
        _rb_define_module_function(rb_mKernel, "msgbox_p", mriP);

        rb_define_global_const("RGSS_VERSION", rb_utf8_str_new_cstr("3.0.1"));
    } else {
        _rb_define_module_function(rb_mKernel, "print", mriPrint);
        _rb_define_module_function(rb_mKernel, "p", mriP);

        rb_define_alias(rb_singleton_class(rb_mKernel), "_mkxp_kernel_caller_alias",
                        "caller");
        _rb_define_module_function(rb_mKernel, "caller", _kernelCaller);
    }

  if (rgssVer == 1 || rgssVer == 2 || rgssVer == 3) {
    int state = 0;
    const char* module_code = nullptr;

    if (rgssVer == 1)
      module_code = module_rpg1;
    else if (rgssVer == 2)
      module_code = module_rpg2;
    else /* rgssVer == 3 */
      module_code = module_rpg3;

    rb_eval_string_protect(module_code, &state);

    if (state) {
      // 获取异常信息
      VALUE exc = rb_errinfo();
      VALUE msg = rb_funcall2(exc, rb_intern("message"), 0, NULL);
      VALUE bt = rb_funcall2(exc, rb_intern("backtrace"), 0, NULL);
      VALUE excClass = rb_class_path(rb_obj_class(exc));

      Debug() << "加载RPG模块失败 (RGSS" << rgssVer << ")";
      Debug() << "错误类型: " << StringValueCStr(excClass);
      Debug() << "错误信息: " << StringValueCStr(msg);

      // 打印回溯信息的前几行
      if (!NIL_P(bt) && RB_TYPE_P(bt, RUBY_T_ARRAY) && RARRAY_LEN(bt) > 0) {
        Debug() << "回溯信息:";
        for (long i = 0; i < RARRAY_LEN(bt) && i < 5; ++i) {
          VALUE line = rb_ary_entry(bt, i);
          Debug() << "  " << StringValueCStr(line);
        }
      }

      // 清除异常状态
#if RAPI_FULL > 187
      rb_set_errinfo(Qnil);
#else
      ruby_errinfo = Qnil;
#endif
    }
  } else {
    assert(!"unreachable");
  }

    VALUE mod = rb_define_module("System");

    // 全面检查Ruby解释器的基本状态
    Debug() << "Ruby VM检查开始 -----------------";
    Debug() << "Ruby版本: " << ruby_description;

    // 检查常见的Ruby核心类是否可用
    VALUE str_class = rb_const_get(rb_cObject, rb_intern("String"));
    VALUE arr_class = rb_const_get(rb_cObject, rb_intern("Array"));
    VALUE hash_class = rb_const_get(rb_cObject, rb_intern("Hash"));

    Debug() << "核心类检查: String=" << (NIL_P(str_class) ? "nil" : "ok")
            << ", Array=" << (NIL_P(arr_class) ? "nil" : "ok")
            << ", Hash=" << (NIL_P(hash_class) ? "nil" : "ok");

    // 检查 System 模块是否正确定义 - C API 方式
    VALUE system_module_check = rb_const_get(rb_cObject, rb_intern("System"));
    if (NIL_P(system_module_check)) {
        Debug() << "System 模块定义失败 (C API 检查)";
    } else {
        Debug() << "System 模块定义成功 (C API 检查)";

        // 检验模块是否具有正确的内部ID
        Debug() << "模块对象比较: " << (system_module_check == mod ? "相同" : "不同");
        Debug() << "模块类型检查: " << (RB_TYPE_P(system_module_check, T_MODULE) ? "正确(Module)" : "错误");
    }

    // 尝试获取模块对象ID，这可以帮助检查对象是否有效
    Debug() << "System模块对象ID: " << NUM2LL(rb_obj_id(mod));

    // 直接检查 mod 的值是否有效
    if (NIL_P(mod)) {
        Debug() << "System 模块定义失败 (mod 对象检查)";
    } else {
        Debug() << "System 模块定义成功 (mod 对象检查)";

        // 检查是否可以在模块上安全调用方法
        int err = 0;
        VALUE ret = rb_protect([](VALUE arg) -> VALUE {
            return rb_funcall(arg, rb_intern("name"), 0);
        }, (VALUE)mod, &err);

        if (err) {
            VALUE exc = rb_errinfo();
            VALUE msg = rb_funcall2(exc, rb_intern("message"), 0, NULL);
            VALUE exc_class = rb_class_path(rb_obj_class(exc));
            Debug() << "调用name方法出错: " << StringValueCStr(exc_class) << ": " << StringValueCStr(msg);
            rb_set_errinfo(Qnil); // 清除错误状态
        } else if (NIL_P(ret)) {
            Debug() << "模块名称调用返回nil (可能表明Ruby内部状态异常)";
        } else if (RB_TYPE_P(ret, T_STRING)) {
            Debug() << "模块名称: " << StringValueCStr(ret);
        } else {
            Debug() << "模块名称类型异常: " << rb_obj_classname(ret);
        }

        // 尝试执行一段简单的Ruby代码
        int eval_state;
        rb_eval_string_protect("$__test_global = 'Ruby解释器正常工作'", &eval_state);
        if (eval_state) {
            Debug() << "简单Ruby代码执行失败 (解释器可能已损坏)";
        } else {
            VALUE test_global = rb_gv_get("$__test_global");
            if (NIL_P(test_global)) {
                Debug() << "全局变量设置失败 (解释器状态异常)";
            } else {
                Debug() << "全局变量测试: " << StringValueCStr(test_global);
            }
        }

        // 使用Ruby代码检查System模块 - 分步执行并详细输出
        Debug() << "开始分步Ruby检查...";

        // 步骤1: 检查Object是否存在
        eval_state = 0;
        VALUE result1 = rb_eval_string_protect("defined?(Object) ? 'Object存在' : 'Object不存在'", &eval_state);
        if (eval_state || NIL_P(result1)) {
            Debug() << "步骤1失败: 无法检查Object是否存在";
        } else {
            Debug() << "步骤1结果: " << StringValueCStr(result1);
        }

        // 步骤2: 检查Object.constants是否包含:System
        eval_state = 0;
        VALUE result2 = rb_eval_string_protect(
            "begin; "
            "  Object.constants.include?(:System) ? 'System常量存在于Object中' : 'System常量不存在于Object中'; "
            "rescue Exception => e; "
            "  \"检查失败: #{e.class}: #{e.message}\"; "
            "end",
            &eval_state);
        if (eval_state || NIL_P(result2)) {
            Debug() << "步骤2失败: 无法检查Object.constants";
        } else {
            Debug() << "步骤2结果: " << StringValueCStr(result2);
        }

        // 步骤3: 直接使用System
        eval_state = 0;
        VALUE result3 = rb_eval_string_protect(
            "begin; "
            "  defined?(System) ? \"System直接可用: #{System.class}\" : 'System直接不可用'; "
            "rescue Exception => e; "
            "  \"直接访问失败: #{e.class}: #{e.message}\"; "
            "end",
            &eval_state);
        if (eval_state || NIL_P(result3)) {
            Debug() << "步骤3失败: 无法直接访问System";
        } else {
            Debug() << "步骤3结果: " << StringValueCStr(result3);
        }

        // 步骤4: 尝试在System上调用方法
        eval_state = 0;
        VALUE result4 = rb_eval_string_protect(
            "begin; "
            "  if defined?(System); "
            "    \"System.methods前5个: #{System.methods[0..4].inspect}\"; "
            "  else; "
            "    'System不可用，无法调用方法'; "
            "  end; "
            "rescue Exception => e; "
            "  \"访问System.methods失败: #{e.class}: #{e.message}\"; "
            "end",
            &eval_state);
        if (eval_state || NIL_P(result4)) {
            Debug() << "步骤4失败: 无法访问System.methods";
        } else {
            Debug() << "步骤4结果: " << StringValueCStr(result4);
        }

        // 尝试获取Object的类名
        eval_state = 0;
        VALUE result5 = rb_eval_string_protect(
            "begin; "
            "  \"原始Object类型: #{Object.class}\"; "
            "rescue Exception => e; "
            "  \"获取Object类型失败: #{e.class}: #{e.message}\"; "
            "end",
            &eval_state);
        if (eval_state || NIL_P(result5)) {
            Debug() << "无法获取Object的类型";
        } else {
            Debug() << "Object类型: " << StringValueCStr(result5);
        }

        Debug() << "分步Ruby检查结束";
    }
    Debug() << "Ruby VM检查结束 -----------------";

    _rb_define_module_function(mod, "delta", mkxpDelta);
    _rb_define_module_function(mod, "uptime", mkxpDelta);
    _rb_define_module_function(mod, "data_directory", mkxpDataDirectory);
    _rb_define_module_function(mod, "set_window_title", mkxpSetTitle);
    _rb_define_module_function(mod, "window_title", mkxpGetTitle);
    _rb_define_module_function(mod, "window_title=", mkxpSetTitle);
    _rb_define_module_function(mod, "show_settings", mkxpSettingsMenu);
    _rb_define_module_function(mod, "puts", mkxpPuts);
    _rb_define_module_function(mod, "desensitize", mkxpDesensitize);
    _rb_define_module_function(mod, "platform", mkxpPlatform);

    _rb_define_module_function(mod, "is_mac?", mkxpIsMacHost);
    _rb_define_module_function(mod, "is_rosetta?", mkxpIsUsingRosetta);

    _rb_define_module_function(mod, "is_linux?", mkxpIsLinuxHost);

    _rb_define_module_function(mod, "is_windows?", mkxpIsWindowsHost);
    _rb_define_module_function(mod, "is_wine?", mkxpIsUsingWine);
    _rb_define_module_function(mod, "is_really_mac?", mkxpIsReallyMacHost);
    _rb_define_module_function(mod, "is_really_linux?", mkxpIsReallyLinuxHost);
    _rb_define_module_function(mod, "is_really_windows?", mkxpIsReallyWindowsHost);


    _rb_define_module_function(mod, "user_language", mkxpUserLanguage);
    _rb_define_module_function(mod, "user_name", mkxpUserName);
    _rb_define_module_function(mod, "game_title", mkxpGameTitle);
    _rb_define_module_function(mod, "power_state", mkxpPowerState);
    _rb_define_module_function(mod, "nproc", mkxpCpuCount);
    _rb_define_module_function(mod, "memory", mkxpSystemMemory);
    _rb_define_module_function(mod, "reload_cache", mkxpReloadPathCache);
    _rb_define_module_function(mod, "mount", mkxpAddPath);
    _rb_define_module_function(mod, "unmount", mkxpRemovePath);
    _rb_define_module_function(mod, "file_exist?", mkxpFileExists);
    _rb_define_module_function(mod, "launch", mkxpLaunch);

    _rb_define_module_function(mod, "default_font_family=", mkxpSetDefaultFontFamily);

    _rb_define_module_function(mod, "parse_csv", mkxpParseCSV);

    _rb_define_method(rb_cString, "to_utf8", mkxpStringToUTF8);
    _rb_define_method(rb_cString, "to_utf8!", mkxpStringToUTF8Bang);

    VALUE cmod = rb_define_module("CFG");
    _rb_define_module_function(cmod, "[]", mkxpGetJSONSetting);
    _rb_define_module_function(cmod, "[]=", mkxpSetJSONSetting);
    _rb_define_module_function(cmod, "to_hash", mkxpGetAllJSONSettings);

    /* Load global constants */
    rb_gv_set("MKXP", Qtrue);

    VALUE debug = rb_bool_new(shState->config().editor.debug);
    if (rgssVer == 1)
        rb_gv_set("DEBUG", debug);
    else if (rgssVer >= 2)
        rb_gv_set("TEST", debug);

    rb_gv_set("BTEST", rb_bool_new(shState->config().editor.battleTest));

#ifdef MKXPZ_BUILD_XCODE
    std::string version = std::string(MKXPZ_VERSION "/") + getPlistValue("GIT_COMMIT_HASH");
    VALUE vers = rb_utf8_str_new_cstr(version.c_str());
#else
    VALUE vers = rb_utf8_str_new_cstr(MKXPZ_VERSION);
#endif
    rb_str_freeze(vers);
    rb_define_const(mod, "VERSION", vers);

    // Automatically load zlib if it's present -- the correct way this time
  int state;
  rb_eval_string_protect("require('zlib') if !Kernel.const_defined?(:Zlib)", &state);
  if (state) {
    Debug() << "Could not load Zlib. If this is important, make sure Ruby was built with static extensions, or that"
            << ((MKXPZ_PLATFORM == MKXPZ_PLATFORM_MACOS) ? "zlib.bundle" : "zlib.so")
            << "is present and reachable by Ruby's loadpath.";

    // 获取异常详细信息
#if RAPI_FULL > 187
    VALUE exc = rb_errinfo();
#else
    VALUE exc = ruby_errinfo;
#endif

    if (!NIL_P(exc)) {
      VALUE msg = rb_funcall2(exc, rb_intern("message"), 0, NULL);
      VALUE bt = rb_funcall2(exc, rb_intern("backtrace"), 0, NULL);
      VALUE excClass = rb_class_path(rb_obj_class(exc));

      Debug() << "错误类型: " << StringValueCStr(excClass);
      Debug() << "错误信息: " << StringValueCStr(msg);

      // 打印回溯信息
      if (!NIL_P(bt) && RB_TYPE_P(bt, RUBY_T_ARRAY) && RARRAY_LEN(bt) > 0) {
        Debug() << "回溯信息:";
        for (long i = 0; i < RARRAY_LEN(bt) && i < 5; ++i) {
          VALUE line = rb_ary_entry(bt, i);
          Debug() << "  " << StringValueCStr(line);
        }
      }
    }

    // 清除异常，避免影响后续代码
#if RAPI_FULL > 187
    rb_set_errinfo(Qnil);
#else
    ruby_errinfo = Qnil;
#endif
  }

    // Set $stdout and its ilk accordingly on Windows
    // I regret teaching you that word
#ifdef __WIN32__
    if (shState->config().winConsole)
        configureWindowsStreams();
#endif
}

static void showMsg(const std::string &msg) {
    shState->eThread().showMessageBox(msg.c_str());
}

static void printP(int argc, VALUE *argv, const char *convMethod,
                   const char *sep) {
    VALUE dispString = rb_str_buf_new(128);
    ID conv = rb_intern(convMethod);

    for (int i = 0; i < argc; ++i) {
        VALUE str = rb_funcall2(argv[i], conv, 0, NULL);
        rb_str_buf_append(dispString, str);

        if (i < argc)
                    rb_str_buf_cat2 (dispString, sep);
    }

    showMsg(RSTRING_PTR(dispString));
}


RB_METHOD_GUARD(mriPrint){
            RB_UNUSED_PARAM;

            printP(argc, argv, "to_s", "");

            shState->checkShutdown();
            shState->checkReset();

            return Qnil;
        }
RB_METHOD_GUARD_END

RB_METHOD_GUARD(mriP){
            RB_UNUSED_PARAM;

            printP(argc, argv, "inspect", "\n");

            shState->checkShutdown();
            shState->checkReset();

            return Qnil;
        }
RB_METHOD_GUARD_END

RB_METHOD(mkxpDelta) {
    RB_UNUSED_PARAM;
    return rb_float_new(shState->runTime());
}

RB_METHOD(mkxpDataDirectory) {
    RB_UNUSED_PARAM;

    const std::string &path = shState->config().customDataPath;
    const char *s = path.empty() ? "." : path.c_str();

    std::string s_nml = shState->fileSystem().normalize(s, 1, 1);
    VALUE ret = rb_utf8_str_new_cstr(s_nml.c_str());

    return ret;
}

RB_METHOD(mkxpSetTitle) {
    RB_UNUSED_PARAM;

    VALUE s;
    rb_scan_args(argc, argv, "1", &s);
    SafeStringValue(s);

    shState->eThread().requestWindowRename(RSTRING_PTR(s));
    return s;
}

RB_METHOD(mkxpGetTitle) {
    RB_UNUSED_PARAM;

    rb_check_argc(argc, 0);

    return rb_utf8_str_new_cstr(SDL_GetWindowTitle(shState->sdlWindow()));
}

RB_METHOD(mkxpDesensitize) {
    RB_UNUSED_PARAM;

    VALUE filename;
    rb_scan_args(argc, argv, "1", &filename);
    SafeStringValue(filename);

    return rb_utf8_str_new_cstr(
            shState->fileSystem().desensitize(RSTRING_PTR(filename)));
}

RB_METHOD(mkxpPuts) {
    RB_UNUSED_PARAM;

    const char *str;
    rb_get_args(argc, argv, "z", &str RB_ARG_END);

    Debug() << str;

    return Qnil;
}

RB_METHOD(mkxpPlatform) {
    RB_UNUSED_PARAM;

#if MKXPZ_PLATFORM == MKXPZ_PLATFORM_MACOS
    std::string platform("macOS");

    if (mkxp_sys::isRosetta())
        platform += " (Rosetta)";

#elif MKXPZ_PLATFORM == MKXPZ_PLATFORM_WINDOWS
    std::string platform("Windows");

    if (mkxp_sys::isWine()) {
        platform += " (Wine - ";
        switch (mkxp_sys::getRealHostType()) {
            case mkxp_sys::WineHostType::Mac:
                platform += "macOS)";
                break;
            default:
                platform += "Linux)";
                break;
        }
    }
#else
    std::string platform("Linux");
#endif

    return rb_utf8_str_new_cstr(platform.c_str());
}

RB_METHOD(mkxpIsMacHost) {
    RB_UNUSED_PARAM;

    return rb_bool_new(MKXPZ_PLATFORM == MKXPZ_PLATFORM_MACOS);
}

RB_METHOD(mkxpIsUsingRosetta) {
    RB_UNUSED_PARAM;

    return rb_bool_new(mkxp_sys::isRosetta());
}

RB_METHOD(mkxpIsLinuxHost) {
    RB_UNUSED_PARAM;

    return rb_bool_new(MKXPZ_PLATFORM == MKXPZ_PLATFORM_LINUX);
}

RB_METHOD(mkxpIsWindowsHost) {
    RB_UNUSED_PARAM;

    return rb_bool_new(MKXPZ_PLATFORM == MKXPZ_PLATFORM_WINDOWS);
}

RB_METHOD(mkxpIsUsingWine) {
    RB_UNUSED_PARAM;
    return rb_bool_new(mkxp_sys::isWine());
}

RB_METHOD(mkxpIsReallyMacHost) {
    RB_UNUSED_PARAM;
    return rb_bool_new(mkxp_sys::getRealHostType() == mkxp_sys::WineHostType::Mac);
}

RB_METHOD(mkxpIsReallyLinuxHost) {
    RB_UNUSED_PARAM;
    return rb_bool_new(mkxp_sys::getRealHostType() == mkxp_sys::WineHostType::Linux);
}

RB_METHOD(mkxpIsReallyWindowsHost) {
    RB_UNUSED_PARAM;
    return rb_bool_new(mkxp_sys::getRealHostType() == mkxp_sys::WineHostType::Windows);
}

RB_METHOD(mkxpUserLanguage) {
    RB_UNUSED_PARAM;

    return rb_utf8_str_new_cstr(mkxp_sys::getSystemLanguage().c_str());
}

RB_METHOD(mkxpUserName) {
    RB_UNUSED_PARAM;

    // Using the Windows API isn't working with usernames that involve Unicode
    // characters for some dumb reason
#ifdef __WIN32__
    VALUE env = rb_const_get(rb_mKernel, rb_intern("ENV"));
    return rb_funcall(env, rb_intern("[]"), 1, rb_str_new_cstr("USERNAME"));
#else
    return rb_utf8_str_new_cstr(mkxp_sys::getUserName().c_str());
#endif
}

RB_METHOD(mkxpGameTitle) {
    RB_UNUSED_PARAM;

    return rb_utf8_str_new_cstr(shState->config().game.title.c_str());
}

RB_METHOD(mkxpPowerState) {
    RB_UNUSED_PARAM;

    int secs, pct;
    SDL_PowerState ps = SDL_GetPowerInfo(&secs, &pct);

    VALUE hash = rb_hash_new();

    rb_hash_aset(hash, ID2SYM(rb_intern("seconds")),
                 (secs > -1) ? INT2NUM(secs) : RUBY_Qnil);

    rb_hash_aset(hash, ID2SYM(rb_intern("percent")),
                 (pct > -1) ? INT2NUM(pct) : RUBY_Qnil);

    rb_hash_aset(hash, ID2SYM(rb_intern("discharging")),
                 rb_bool_new(ps == SDL_POWERSTATE_ON_BATTERY));

    return hash;
}

RB_METHOD(mkxpSettingsMenu) {
    RB_UNUSED_PARAM;

    shState->eThread().requestSettingsMenu();

    return Qnil;
}

RB_METHOD(mkxpCpuCount) {
    RB_UNUSED_PARAM;

    return INT2NUM(SDL_GetCPUCount());
}

RB_METHOD(mkxpSystemMemory) {
    RB_UNUSED_PARAM;

    return INT2NUM(SDL_GetSystemRAM());
}

RB_METHOD_GUARD(mkxpReloadPathCache){
            RB_UNUSED_PARAM;

            shState->fileSystem().reloadPathCache();
            return Qnil;
        }
RB_METHOD_GUARD_END

RB_METHOD_GUARD(mkxpAddPath){
            RB_UNUSED_PARAM;

            VALUE path, mountpoint, reload;
            rb_scan_args(argc, argv, "12", &path, &mountpoint, &reload);
            SafeStringValue(path);
            if (mountpoint != Qnil) SafeStringValue(mountpoint);

            const char *mp = (mountpoint == Qnil) ? 0 : RSTRING_PTR(mountpoint);

            bool rl = true;
            if (reload != Qnil)
                rb_bool_arg(reload, &rl);

            shState->fileSystem().addPath(RSTRING_PTR(path), mp, rl);

            return path;
        }
RB_METHOD_GUARD_END

RB_METHOD_GUARD(mkxpRemovePath){
            RB_UNUSED_PARAM;

            VALUE path, reload;
            rb_scan_args(argc, argv, "11", &path, &reload);
            SafeStringValue(path);

            bool rl = true;
            if (reload != Qnil)
                rb_bool_arg(reload, &rl);

            shState->fileSystem().removePath(RSTRING_PTR(path), rl);

            return path;
        }
RB_METHOD_GUARD_END

RB_METHOD(mkxpFileExists) {
    RB_UNUSED_PARAM;

    VALUE path;
    rb_scan_args(argc, argv, "1", &path);
    SafeStringValue(path);

    if (shState->fileSystem().exists(RSTRING_PTR(path)))
        return Qtrue;
    return Qfalse;
}

RB_METHOD(mkxpSetDefaultFontFamily) {
    RB_UNUSED_PARAM;

    VALUE familyV;
    rb_scan_args(argc, argv, "1", &familyV);
    SafeStringValue(familyV);

    std::string family(RSTRING_PTR(familyV));
    shState->fontState().setDefaultFontFamily(family);

    return Qnil;
}

RB_METHOD_GUARD(mkxpStringToUTF8){
            RB_UNUSED_PARAM;

            rb_check_argc(argc, 0);

            std::string ret(RSTRING_PTR(self), RSTRING_LEN(self));
            ret = Encoding::convertString(ret);

            return rb_utf8_str_new(ret.c_str(), ret.length());
        }
RB_METHOD_GUARD_END

RB_METHOD_GUARD(mkxpStringToUTF8Bang){
            RB_UNUSED_PARAM;

            rb_check_argc(argc, 0);

            std::string ret(RSTRING_PTR(self), RSTRING_LEN(self));
            ret = Encoding::convertString(ret);

            rb_str_resize(self, ret.length());
            memcpy(RSTRING_PTR(self), ret.c_str(), RSTRING_LEN(self));

#if RAPI_FULL >= 190
            rb_funcall(self, rb_intern("force_encoding"), 1,
                       rb_enc_from_encoding(rb_utf8_encoding()));
#endif

            return self;
        }
RB_METHOD_GUARD_END

#ifdef __APPLE__
#define OPENCMD "open "
#define OPENARGS "--args"
#elif defined(__linux__)
#define OPENCMD "xdg-open "
#define OPENARGS ""
#else
#define OPENCMD "start /b \"launch\" "
#define OPENARGS ""
#endif

RB_METHOD_GUARD(mkxpLaunch){
            RB_UNUSED_PARAM;

            VALUE cmdname, args;

            rb_scan_args(argc, argv, "11", &cmdname, &args);
            SafeStringValue(cmdname);

            std::string command(OPENCMD);
            command += "\"";
            command += RSTRING_PTR(cmdname);
            command += "\"";

            if (args != RUBY_Qnil) {
#ifndef __linux__
                command += " ";
        command += OPENARGS;
        Check_Type(args, T_ARRAY);

        for (int i = 0; i < RARRAY_LEN(args); i++) {
            VALUE arg = rb_ary_entry(args, i);
            SafeStringValue(arg);

            if (RSTRING_LEN(arg) <= 0)
                continue;

            command += " ";
            command += RSTRING_PTR(arg);
        }
#else
                Debug() << command << ":" << "Arguments are not supported with xdg-open. Ignoring.";
#endif
            }

            if (std::system(command.c_str()) != 0) {
                throw Exception(Exception::MKXPError, "Failed to launch \"%s\"",
                                RSTRING_PTR(cmdname));
            }

            return RUBY_Qnil;
        }
RB_METHOD_GUARD_END

RB_METHOD_GUARD(mkxpParseCSV){
            RB_UNUSED_PARAM;

            VALUE str;
            rb_scan_args(argc, argv, "1", &str);
            SafeStringValue(str);

            VALUE ret = rb_ary_new();
            std::stringstream stream(RSTRING_PTR(str));
            try {
                rapidcsv::Document doc(stream, rapidcsv::LabelParams(-1, -1),
                                       rapidcsv::SeparatorParams(',', false, true, true, true));
                for (int r = 0; r < doc.GetRowCount(); r++) {
                    VALUE col = rb_ary_new();
                    for (int c = 0; c < doc.GetColumnCount(); c++) {
                        std::string str = doc.GetCell<std::string>(c, r);
                        rb_ary_push(col, rb_utf8_str_new(str.c_str(), str.length()));
                    }
                    rb_ary_push(ret, col);
                }
            } catch (std::exception &e) {
                throw Exception(Exception::MKXPError, "Failed to parse CSV: %s", e.what());
            }

            return ret;
        }
RB_METHOD_GUARD_END

json5pp::value loadUserSettings() {
    json5pp::value ret;
    VALUE cpath = rb_utf8_str_new_cstr(shState->config().userConfPath.c_str());

    if (rb_funcall(rb_cFile, rb_intern("exists?"), 1, cpath) == Qtrue) {
        VALUE f = rb_funcall(rb_cFile, rb_intern("open"), 2, cpath, rb_str_new("r", 1));
        VALUE data = rb_funcall(f, rb_intern("read"), 0);
        rb_funcall(f, rb_intern("close"), 0);
        ret = json5pp::parse5(RSTRING_PTR(data));
    }

    if (!ret.is_object())
        ret = json5pp::object({});

    return ret;
}

void saveUserSettings(json5pp::value &settings) {
    VALUE cpath = rb_utf8_str_new_cstr(shState->config().userConfPath.c_str());
    VALUE f = rb_funcall(rb_cFile, rb_intern("open"), 2, cpath, rb_str_new("w", 1));
    rb_funcall(f, rb_intern("write"), 1,
               rb_utf8_str_new_cstr(settings.stringify5(json5pp::rule::space_indent<>()).c_str()));
    rb_funcall(f, rb_intern("close"), 0);
}

RB_METHOD(mkxpGetJSONSetting) {
    RB_UNUSED_PARAM;

    VALUE sname;
    rb_scan_args(argc, argv, "1", &sname);
    SafeStringValue(sname);

    auto settings = loadUserSettings();
    auto &s = settings.as_object();

    if (s[RSTRING_PTR(sname)].is_null()) {
        return json2rb(shState->config().raw.as_object()[RSTRING_PTR(sname)]);
    }

    return json2rb(s[RSTRING_PTR(sname)]);

}

RB_METHOD_GUARD(mkxpSetJSONSetting){
            RB_UNUSED_PARAM;

            VALUE sname, svalue;
            rb_scan_args(argc, argv, "2", &sname, &svalue);
            SafeStringValue(sname);

            auto settings = loadUserSettings();
            auto &s = settings.as_object();
            s[RSTRING_PTR(sname)] = rb2json(svalue);
            saveUserSettings(settings);

            return Qnil;
        }
RB_METHOD_GUARD_END

RB_METHOD(mkxpGetAllJSONSettings) {
    RB_UNUSED_PARAM;

    return json2rb(shState->config().raw);
}


static VALUE rgssMainCb(VALUE block) {
    rb_funcall2(block, rb_intern("call"), 0, 0);
    return Qnil;
}

static VALUE rgssMainRescue(VALUE arg, VALUE exc) {
    VALUE *excRet = (VALUE *) arg;

    *excRet = exc;

    return Qnil;
}

static bool processReset(bool rubyExc) {
    const char *str = "Audio.__reset__; Graphics.__reset__;";

    if (rubyExc) {
        rb_eval_string(str);
    } else {
        int state;
        rb_eval_string_protect(str, &state);
        return state;
    }

    return 0;
}

#if RAPI_FULL > 187

static VALUE newStringUTF8(const char *string, long length) {
    return rb_enc_str_new(string, length, rb_utf8_encoding());
}

#else
#define newStringUTF8 rb_str_new
#endif

struct evalArg {
    VALUE string;
    VALUE filename;
};

static VALUE evalHelper(evalArg *arg) {
    VALUE argv[] = {arg->string, Qnil, arg->filename};
    return rb_funcall2(Qnil, rb_intern("eval"), ARRAY_SIZE(argv), argv);
}

static VALUE evalString(VALUE string, VALUE filename, int *state) {
    evalArg arg = {string, filename};
    return rb_protect((VALUE(*)(VALUE)) evalHelper, (VALUE) &arg, state);
}

static void runCustomScript(const std::string &filename) {
    std::string scriptData;

    if (!readFileSDL(filename.c_str(), scriptData)) {
        showMsg(std::string("Unable to open '") + filename + "'");
        return;
    }
    int state;
    evalString(newStringUTF8(scriptData.c_str(), scriptData.size()),
               newStringUTF8(filename.c_str(), filename.size()), &state);
    if(state){
        // 获取详细的错误信息
#if RAPI_FULL > 187
        VALUE exc = rb_errinfo();
#else
        VALUE exc = ruby_errinfo;
#endif
        if (!NIL_P(exc)) {
            VALUE msg = rb_funcall2(exc, rb_intern("message"), 0, NULL);
            VALUE bt = rb_funcall2(exc, rb_intern("backtrace"), 0, NULL);
            VALUE excClass = rb_class_path(rb_obj_class(exc));

            std::string errorMsg = std::string("执行脚本 '") + filename + "' 时出错\n";
            errorMsg += "错误类型: " + std::string(StringValueCStr(excClass)) + "\n";
            errorMsg += "错误信息: " + std::string(StringValueCStr(msg)) + "\n";

            // 添加回溯信息
            if (!NIL_P(bt) && RB_TYPE_P(bt, RUBY_T_ARRAY) && RARRAY_LEN(bt) > 0) {
                errorMsg += "堆栈回溯:\n";
                long btLen = RARRAY_LEN(bt);
                // 限制显示的行数，避免过长
                long maxLines = std::min(btLen, 10L);
                for (long i = 0; i < maxLines; ++i) {
                    VALUE line = rb_ary_entry(bt, i);
                    errorMsg += "  " + std::string(StringValueCStr(line)) + "\n";
                }
                if (maxLines < btLen) {
                    errorMsg += "  ... (还有 " + std::to_string(btLen - maxLines) + " 行未显示)\n";
                }
            }

            // 也在调试日志中输出
            Debug() << "执行脚本 '" << filename << "' 时出错";
            Debug() << "错误类型: " << StringValueCStr(excClass);
            Debug() << "错误信息: " << StringValueCStr(msg);

            showMsg(errorMsg);

            // 清除异常状态
#if RAPI_FULL > 187
            rb_set_errinfo(Qnil);
#else
            ruby_errinfo = Qnil;
#endif
        } else {
            showMsg(std::string("执行脚本 '") + filename + "' 时出错，但无法获取详细信息");
        }
        return;
    }
}

RB_METHOD_GUARD(mriRgssMain){
            RB_UNUSED_PARAM;

            /* Execute postload scripts */
            const Config &conf = shState->rtData().config;
            for (std::vector<std::string>::const_iterator i = conf.postloadScripts.begin();
                 i != conf.postloadScripts.end(); ++i) {
                if (shState->rtData().rqTerm)
                    break;
                runCustomScript(*i);
            }

            while (true) {
                VALUE exc = Qnil;
#if RAPI_FULL < 270
                rb_rescue2((VALUE(*)(ANYARGS))rgssMainCb, rb_block_proc(),
                   (VALUE(*)(ANYARGS))rgssMainRescue, (VALUE)&exc, rb_eException,
                   (VALUE)0);
#else
                rb_rescue2(rgssMainCb, rb_block_proc(), rgssMainRescue, (VALUE) &exc,
                           rb_eException, (VALUE) 0);
#endif

                if (NIL_P(exc))
                    break;

                if (rb_obj_class(exc) == getRbData()->exc[Reset])
                    processReset(true);
                else
                    rb_exc_raise(exc);
            }

            return Qnil;
        }
RB_METHOD_GUARD_END

RB_METHOD_GUARD(mriRgssStop){
            RB_UNUSED_PARAM;

            while (true)
                shState->graphics().update();

            return Qnil;
        }
RB_METHOD_GUARD_END

RB_METHOD(_kernelCaller) {
    RB_UNUSED_PARAM;

    VALUE trace =
            rb_funcall2(rb_mKernel, rb_intern("_mkxp_kernel_caller_alias"), 0, 0);

    if (!RB_TYPE_P(trace, RUBY_T_ARRAY))
        return trace;

    long len = RARRAY_LEN(trace);

    if (len < 2)
        return trace;

    /* Remove useless "ruby:1:in 'eval'" */
    rb_ary_pop(trace);

    /* Also remove trace of this helper function */
    rb_ary_shift(trace);

    len -= 2;

    if (len == 0)
        return trace;

    /* RMXP does this, not sure if specific or 1.8 related */
    VALUE args[] = {rb_utf8_str_new_cstr(":in `<main>'"), rb_utf8_str_new_cstr("")};
    rb_funcall2(rb_ary_entry(trace, len - 1), rb_intern("gsub!"), 2, args);

    return trace;
}


VALUE kernelLoadDataInt(const char *filename, bool rubyExc, bool raw);

struct BacktraceData {
    /* Maps: Ruby visible filename, To: Actual script name */
    BoostHash<std::string, std::string> scriptNames;
};

bool evalScript(VALUE string, const char *filename) {
    int state;
    evalString(string, rb_utf8_str_new_cstr(filename), &state);
    if (state) return false;
    return true;
}


#define SCRIPT_SECTION_FMT (rgssVer >= 3 ? "{%04ld}" : "Section%03ld")
#include <MtoolProc.h>
static void runRMXPScripts(BacktraceData &btData) {
    const Config &conf = shState->rtData().config;
    const std::string &scriptPack = conf.game.scripts;

    if (scriptPack.empty()) {
        showMsg("No script file has been specified. Check the game's INI and try again.");
        return;
    }

    if (!shState->fileSystem().exists(shState->fileSystem().desensitize(scriptPack.c_str()))) {
        showMsg("Unable to load scripts from '" + scriptPack + "'");
        return;
    }

#ifdef __ANDROID__
    // 通知加载状态：开始加载脚本文件
    MtoolProc::notifyLoadingStatus(2);
#endif

    VALUE scriptArray;

    /* We checked if Scripts.rxdata exists, but something might
     * still go wrong */
    try {
        scriptArray = kernelLoadDataInt(shState->fileSystem().desensitize(scriptPack.c_str()), false, false);
    } catch (const Exception &e) {
        showMsg(std::string("Failed to read script data: ") + e.msg);
        return;
    }

    if (!RB_TYPE_P(scriptArray, RUBY_T_ARRAY)) {
        showMsg("Failed to read script data");
        return;
    }

    rb_gv_set("$RGSS_SCRIPTS", scriptArray);

    long scriptCount = RARRAY_LEN(scriptArray);

    std::string decodeBuffer;
    decodeBuffer.resize(0x1000);

#ifdef __ANDROID__
    // 通知加载状态：开始解压脚本数据
    MtoolProc::notifyLoadingStatus(3);
#endif

    for (long i = 0; i < scriptCount; ++i) {
        VALUE script = rb_ary_entry(scriptArray, i);

        if (!RB_TYPE_P(script, RUBY_T_ARRAY))
            continue;

        VALUE scriptName = rb_ary_entry(script, 1);
        VALUE scriptString = rb_ary_entry(script, 2);

        int result = Z_OK;
        unsigned long bufferLen;

        while (true) {
            unsigned char *bufferPtr = reinterpret_cast<unsigned char *>(
                    const_cast<char *>(decodeBuffer.c_str()));
            const unsigned char *sourcePtr =
                    reinterpret_cast<const unsigned char *>(RSTRING_PTR(scriptString));

            bufferLen = decodeBuffer.length();

            result = uncompress(bufferPtr, &bufferLen, sourcePtr,
                                RSTRING_LEN(scriptString));

            bufferPtr[bufferLen] = '\0';

            if (result != Z_BUF_ERROR)
                break;

            decodeBuffer.resize(decodeBuffer.size() * 2);
        }

        if (result != Z_OK) {
            static char buffer[256];
            snprintf(buffer, sizeof(buffer), "Error decoding script %ld: '%s'", i,
                     RSTRING_PTR(scriptName));

            showMsg(buffer);

            break;
        }

        rb_ary_store(script, 3, rb_utf8_str_new_cstr(decodeBuffer.c_str()));
    }

#ifdef __ANDROID__
    // 通知加载状态：开始执行预加载脚本
    MtoolProc::notifyLoadingStatus(4);
#endif

    /* Execute preloaded scripts */
    for (std::vector<std::string>::const_iterator i = conf.preloadScripts.begin();
         i != conf.preloadScripts.end(); ++i) {
        if (shState->rtData().rqTerm)
            break;
        runCustomScript(*i);
    }
#if RAPI_FULL <= 187
    VALUE exc = ruby_errinfo;
#else
    VALUE exc = rb_errinfo();
#endif
    if (exc != Qnil)
        return;

#ifdef __ANDROID__
    // 通知加载状态：正在执行脚本
    MtoolProc::notifyLoadingStatus(5);
#endif
    while (true) {
        for (long i = 0; i < scriptCount; ++i) {
            if (shState->rtData().rqTerm)
                break;

            VALUE script = rb_ary_entry(scriptArray, i);
            VALUE scriptDecoded = rb_ary_entry(script, 3);
            VALUE string =
                    newStringUTF8(RSTRING_PTR(scriptDecoded), RSTRING_LEN(scriptDecoded));

            VALUE fname;
            const char *scriptName = RSTRING_PTR(rb_ary_entry(script, 1));
            char buf[512];
            int len;

            if (conf.useScriptNames)
                len = snprintf(buf, sizeof(buf), "%03ld:%s", i, scriptName);
            else
                len = snprintf(buf, sizeof(buf), SCRIPT_SECTION_FMT, i);

            fname = newStringUTF8(buf, len);
            btData.scriptNames.insert(buf, scriptName);


            // if the script name starts with |s|, only execute
            // it if "s" is the same first letter as the platform
            // we're running on

            // |W| - Windows, |M| - Mac OS X, |L| - Linux

            // Adding a 'not' symbol means it WON'T run on that
            // platform (i.e. |!W| won't run on Windows)
            /*
             if (scriptName[0] == '|') {
             int len = strlen(scriptName);
             if (len > 2) {
             if (scriptName[1] == '!' && len > 3 &&
             scriptName[3] == scriptName[0]) {
             if (toupper(scriptName[2]) == platform[0])
             continue;
             }
             if (scriptName[2] == scriptName[0] &&
             toupper(scriptName[1]) != platform[0])
             continue;
             }
             }
             */

            int state;
            evalString(string, fname, &state);
            if (state)
                break;
        }

      #if RAPI_FULL <= 187
        VALUE exc = ruby_errinfo;
      #else
        VALUE exc = rb_errinfo();
      #endif
        if (rb_obj_class(exc) != getRbData()->exc[Reset])
            break;

        if (processReset(false))
            break;
    }

}

static void showExc(VALUE exc, const BacktraceData &btData) {
    VALUE bt = rb_funcall2(exc, rb_intern("backtrace"), 0, NULL);
    VALUE msg = rb_funcall2(exc, rb_intern("message"), 0, NULL);
    VALUE name = rb_class_path(rb_obj_class(exc));

    // 记录完整的错误信息到调试日志
    Debug() << "Ruby 异常: " << StringValueCStr(name);
    Debug() << "错误信息: " << StringValueCStr(msg);

    // 处理空堆栈的情况
    if (NIL_P(bt) || !RB_TYPE_P(bt, RUBY_T_ARRAY) || RARRAY_LEN(bt) == 0) {
        Debug() << "堆栈回溯: <不可用>";

        std::string ms(640, '\0');
        snprintf(&ms[0], ms.size(), "Ruby 错误: %s\n\n%s\n\n堆栈回溯不可用",
                 RSTRING_PTR(name), RSTRING_PTR(msg));

        showMsg(ms);
        return;
    }

    VALUE bt0 = rb_ary_entry(bt, 0);

    // 创建完整的调试输出
    VALUE ds = rb_sprintf("%" PRIsVALUE ": %" PRIsVALUE " (%" PRIsVALUE ")",
#if RAPI_MAJOR >= 2
                          bt0, exc, name);
#else
    // Ruby 1.9's version of this function needs char*
    RSTRING_PTR(bt0), RSTRING_PTR(exc), RSTRING_PTR(name));
#endif
    /* omit "useless" last entry (from ruby:1:in `eval') */
    for (long i = 1, btlen = RARRAY_LEN(bt) - 1; i < btlen; ++i)
        rb_str_catf(ds, "\n\tfrom %" PRIsVALUE,
#if RAPI_MAJOR >= 2
                    rb_ary_entry(bt, i));
#else
    RSTRING_PTR(rb_ary_entry(bt, i)));
#endif
    Debug() << StringValueCStr(ds);

    char *s = RSTRING_PTR(bt0);

    // 解析堆栈的第一行以获取文件和行号信息
    char line[16];
    std::string file(512, '\0');

    char *p = s + strlen(s);
    char *e;

    while (p != s)
        if (*--p == ':')
            break;

    e = p;

    while (p != s)
        if (*--p == ':')
            break;

    /* s         p  e
     * SectionXXX:YY: in 'blabla' */

    *e = '\0';
    strncpy(line, *p ? p + 1 : p, sizeof(line));
    line[sizeof(line) - 1] = '\0';
    *e = ':';
    e = p;

    /* s         e
     * SectionXXX:YY: in 'blabla' */

    *e = '\0';
    strncpy(&file[0], s, file.size());
    *e = ':';

    /* Shrink to fit */
    file.resize(strlen(file.c_str()));
    file = btData.scriptNames.value(file, file);

    // 创建并显示美观的错误消息，包括完整的堆栈
    std::string ms(1024, '\0');  // 增加缓冲区大小以适应更多文本
    int offset = snprintf(&ms[0], ms.size(),
                "脚本 '%s' 第 %s 行发生错误: %s\n\n错误信息: %s\n\n堆栈回溯:\n",
                file.c_str(), line, RSTRING_PTR(name), RSTRING_PTR(msg));

    // 添加更完整的堆栈回溯到错误信息
    long btlen = RARRAY_LEN(bt);
    long maxLines = std::min(btlen, 10L); // 最多显示10行堆栈

    for (long i = 0; i < maxLines && offset < ms.size() - 1; ++i) {
        VALUE btLine = rb_ary_entry(bt, i);
        if (!NIL_P(btLine)) {
            const char* btStr = StringValueCStr(btLine);

            // 处理堆栈中的文件名映射
            std::string currentLine(btStr);
            size_t firstColon = currentLine.find(':');
            if (firstColon != std::string::npos) {
                std::string scriptSection = currentLine.substr(0, firstColon);
                std::string mapped = btData.scriptNames.value(scriptSection, scriptSection);
                if (mapped != scriptSection) {
                    currentLine = mapped + currentLine.substr(firstColon);
                }
            }

            offset += snprintf(&ms[offset], ms.size() - offset, "  %s\n", currentLine.c_str());
        }
    }

    // 如果堆栈超过了最大行数，显示剩余行数
    if (maxLines < btlen) {
        offset += snprintf(&ms[offset], ms.size() - offset,
                         "  ... (还有 %ld 行未显示)\n", btlen - maxLines);
    }

    // 确保字符串以正确长度结束
    ms.resize(strlen(ms.c_str()));

    showMsg(ms);
}

static void mriBindingExecute() {
    Config &conf = shState->rtData().config;


#if RAPI_MAJOR >= 2
    /* Normally only a ruby executable would do a sysinit,
     * but not doing it will lead to crashes due to closed
     * stdio streams on some platforms (eg. Windows) */
    int argc = 0;
    char **argv = 0;
    ruby_sysinit(&argc, &argv);

    RUBY_INIT_STACK;
    ruby_init();

    std::vector<const char *> rubyArgsC{"mkxp-z"};
    rubyArgsC.push_back("-e ");
    void *node;
    if (conf.jit.enabled) {
#if RAPI_FULL >= 310
        // Ruby v3.1.0 renamed the --jit options to --mjit.
        std::string verboseLevel("--mjit-verbose=");
        std::string maxCache("--mjit-max-cache=");
        std::string minCalls("--mjit-min-calls=");
        rubyArgsC.push_back("--mjit");
#else
        std::string verboseLevel("--jit-verbose=");
        std::string maxCache("--jit-max-cache=");
        std::string minCalls("--jit-min-calls=");
        rubyArgsC.push_back("--jit");
#endif
        verboseLevel += std::to_string(conf.jit.verboseLevel);
        maxCache += std::to_string(conf.jit.maxCache);
        minCalls += std::to_string(conf.jit.minCalls);

        rubyArgsC.push_back(verboseLevel.c_str());
        rubyArgsC.push_back(maxCache.c_str());
        rubyArgsC.push_back(minCalls.c_str());
        node = ruby_options(rubyArgsC.size(), const_cast<char **>(rubyArgsC.data()));
    } else if (conf.yjit.enabled) {
        rubyArgsC.push_back("--yjit");
        // TODO: Maybe support --yjit-exec-mem-size, --yjit-call-threshold
        node = ruby_options(rubyArgsC.size(), const_cast<char **>(rubyArgsC.data()));
    } else {
        node = ruby_options(rubyArgsC.size(), const_cast<char **>(rubyArgsC.data()));
    }

    int state;
    bool valid = ruby_executable_node(node, &state);
    if (valid)
        state = ruby_exec_node(node);
    if (state || !valid) {
        // The message is formatted for and automatically spits
        // out to the terminal, so let's leave it that way for now
        /*
         VALUE exc = rb_errinfo();
         #if RAPI_FULL >= 250
         VALUE msg = rb_funcall(exc, rb_intern("full_message"), 0);
         #else
         VALUE msg = rb_funcall(exc, rb_intern("message"), 0);
         #endif
         */
        showMsg("An error occurred while initializing Ruby. (Invalid JIT settings?)");
        ruby_cleanup(state);
        shState->rtData().rqTermAck.set();
        return;
    }
    rb_enc_set_default_internal(rb_enc_from_encoding(rb_utf8_encoding()));
    rb_enc_set_default_external(rb_enc_from_encoding(rb_utf8_encoding()));
#else
    ruby_init();
    rb_eval_string("$KCODE='U'");
#if RAPI_FULL > 190
    VALUE utf8 = rb_enc_from_encoding(rb_utf8_encoding());
    rb_enc_set_default_external(utf8);
    rb_enc_set_default_internal(utf8);
#endif

#ifdef __WIN32__
    if (!conf.winConsole) {
        VALUE iostr = rb_str_new2("NUL");
        // Sysinit isn't a thing yet, so send io to /dev/null instead
        rb_funcall(rb_gv_get("$stderr"), rb_intern("reopen"), 1, iostr);
        rb_funcall(rb_gv_get("$stdout"), rb_intern("reopen"), 1, iostr);
    }
#endif
#endif

#if RAPI_FULL > 187
    VALUE rbArgv = rb_get_argv();
#else
    VALUE rbArgv = rb_gv_get("$*");
#endif
    for (const auto &str: conf.launchArgs)
        rb_ary_push(rbArgv, rb_utf8_str_new_cstr(str.c_str()));

    // Duplicates get pushed for some reason
    rb_funcall(rbArgv, rb_intern("uniq!"), 0);

    VALUE lpaths = rb_gv_get(":");
    rb_ary_clear(lpaths);

#if defined(MKXPZ_BUILD_XCODE) && RAPI_MAJOR >= 2
    std::string resPath = mkxp_fs::getResourcePath();
    resPath += "/Ruby/" + std::to_string(RAPI_MAJOR) + "." + std::to_string(RAPI_MINOR) + ".0";
    rb_ary_push(lpaths, rb_str_new(resPath.c_str(), resPath.size()));
#endif

    if (!conf.rubyLoadpaths.empty()) {
        /* Setup custom load paths */
        for (size_t i = 0; i < conf.rubyLoadpaths.size(); ++i) {
            std::string &path = conf.rubyLoadpaths[i];

            VALUE pathv = rb_str_new(path.c_str(), path.size());
            rb_ary_push(lpaths, pathv);
        }
    }
#ifndef WORKDIR_CURRENT
    else {
        std::string cwd = mkxp_fs::getCurrentDirectory();
#if RAPI_FULL > 187
        rb_ary_push(lpaths, rb_enc_str_new(cwd.c_str(), cwd.size(), rb_filesystem_encoding()));
#else
        rb_ary_push(lpaths, rb_str_new(cwd.c_str(), cwd.size()));
#endif
        //rb_ary_push(lpaths, rb_utf8_str_new_cstr(mkxp_fs::getCurrentDirectory().c_str()));
    }
#endif

    RbData rbData;
    shState->setBindingData(&rbData);
    BacktraceData btData;

    mriBindingInit();

    std::string &customScript = conf.customScript;
    if (!customScript.empty())
        runCustomScript(customScript);
    else
        runRMXPScripts(btData);

#if RAPI_FULL > 187
    VALUE exc = rb_errinfo();
#else
    VALUE exc = ruby_errinfo;
#endif
    if (!NIL_P(exc) && !rb_obj_is_kind_of(exc, rb_eSystemExit))
        showExc(exc, btData);

    ruby_cleanup(0);

    shState->rtData().rqTermAck.set();
}

static void mriBindingTerminate() { throw Exception(Exception::SystemExit, " "); }

static void mriBindingReset() { throw Exception(Exception::Reset, " "); }

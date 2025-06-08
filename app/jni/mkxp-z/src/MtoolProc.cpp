//
// Created by root on 5/29/25.
//

#include "MtoolProc.h"
#include "jni.h"
#include "json.hpp"
#include <thread>
#include <unistd.h>
#include "android/log.h"
#include "concurrent_queue.h"


#include <ruby.h>

// Ruby版本兼容性检测 - 使用构建时定义的宏
#ifdef MKXPZ_RUBY_193
#include <ruby/encoding.h>
#endif

// Ruby版本兼容性函数
#ifdef MKXPZ_RUBY_187
// Ruby 1.8.7版本的兼容性定义
static VALUE compat_rb_errinfo(void) {
    return ruby_errinfo;
}

static void compat_rb_set_errinfo(VALUE err) {
  ruby_errinfo = err;
}

static VALUE compat_rb_str_new_cstr(const char* str) {
    return rb_str_new2(str);
}

#define rb_errinfo() compat_rb_errinfo()
#define rb_set_errinfo(err) compat_rb_set_errinfo(err)
#define rb_str_new_cstr(str) compat_rb_str_new_cstr(str)

#endif
using namespace nlohmann;

MtoolProc* MtoolProc::instance = nullptr;

void debugout(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(ANDROID_LOG_WARN, "MToolClient", fmt, args);
    va_end(args);
}

int MtoolServerport = 0;

MtoolProc::MtoolProc() {
    this->trsLoaded = false;
    this->m_initialized = false;
}

void MtoolProc::staticCall() {
    if (instance == nullptr) {
        instance = new MtoolProc();
    }
    instance->onEventLoop();
}
VALUE rb_mri_fixString(VALUE self, VALUE arg, VALUE len);
VALUE rb_MRI_doEvals(VALUE self, VALUE arg) {
    return 0;
}
VALUE callMtoolServer(VALUE self, VALUE arg0, VALUE arg1) {
    return 0;
}
void MtoolProc::init() {
    if (this->m_initialized) {
        return;
    }
    this->m_initialized = true;

#ifdef __ANDROID__
    // 通知游戏已加载完成，因为此方法在Ruby环境初始化完成后被调用
    // 无论脚本执行到哪里，这时候都确保移除加载UI
    MtoolProc::notifyLoadingStatus(0);
#endif

    rb_eval_string("$isMRI = true");
    rb_eval_string("$isMRIAndroid = true");
    rb_define_global_function("callMtoolServer", RUBY_METHOD_FUNC(callMtoolServer), 2);
    rb_define_global_function("fixString", RUBY_METHOD_FUNC(rb_mri_fixString), 2);
    rb_define_global_function("doMToolEvals", RUBY_METHOD_FUNC(rb_MRI_doEvals), 0);

    m_host = "127.0.0.1"; // 默认主机地址
    m_port = MtoolServerport; // 默认端口号
    m_client = new MToolClient(m_host, m_port, "/");

    // 设置回调函数
    m_client->SetMessageCallback([this](const std::string &message) {
        handleTextMessage(message);
    });

    m_client->SetBinaryMessageCallback([this](const std::vector<uint8_t> &data) {
        handleBinaryMessage(data);
    });

    m_client->SetErrorCallback([this](const std::string &error) {
        handleError(error);
    });

    m_client->SetConnectedCallback([this]() {
        handleConnected();
    });

    m_client->SetClosedCallback([this]() {
        handleClosed();
    });
    m_client->ConnectAsync();
}

// Ruby异常类，用于在C++中抛出Ruby异常
class RubyException : public std::exception {
private:
    std::string message;

public:
    explicit RubyException(const std::string &msg) : message(msg) {}

    const char *what() const noexcept override {
        return message.c_str();
    }
};

// 获取Ruby异常信息的辅助函数
std::string getRubyErrorMessage(int state) {
    if (state == 0) return "";

    VALUE errinfo = rb_errinfo();
    if (errinfo == Qnil) return "Unknown Ruby error";

    VALUE message = rb_funcall(errinfo, rb_intern("message"), 0);
    VALUE klass = rb_class_name(rb_class_of(errinfo));
    VALUE backtrace = rb_funcall(errinfo, rb_intern("backtrace"), 0);

    std::string error_message = "Ruby Exception: ";
    error_message += StringValuePtr(klass);
    error_message += ": ";
    error_message += StringValuePtr(message);

    if (backtrace != Qnil && RARRAY_LEN(backtrace) > 0) {
        error_message += "\nBacktrace:\n";
        for (long i = 0; i < std::min(3L, RARRAY_LEN(backtrace)); i++) {
            VALUE line = rb_ary_entry(backtrace, i);
            error_message += "  ";
            error_message += StringValuePtr(line);
            error_message += "\n";
        }
    }

    return error_message;
}

// Ruby 求值函数，返回字符串（UTF-8 编码）
extern "C" char *RGSSGetStringUTF8(const char *script) {
    int state = 0;
    VALUE result = rb_eval_string_protect(script, &state);

    if (state != 0) {
        // Ruby 执行发生异常
        std::string error_msg = getRubyErrorMessage(state);
        rb_set_errinfo(Qnil); // 清除Ruby异常状态
        throw RubyException(error_msg);
    }

    if (TYPE(result) != T_STRING) {
        // 如果结果不是字符串，尝试转换
        result = rb_funcall(result, rb_intern("to_s"), 0);
    }

    // 获取字符串内容
    char *str = StringValuePtr(result);

    return str;
}

// Ruby 求值函数，返回整数（0表示正确执行，非0表示错误）
extern "C" int RGSSEval(const char *script) {
    int state = 0;
    rb_eval_string_protect(script, &state);

    if (state != 0) {
        // Ruby 执行发生异常
        std::string error_msg = getRubyErrorMessage(state);
        rb_set_errinfo(Qnil); // 清除Ruby异常状态
        throw RubyException(error_msg);
    }

    // state为0表示正确执行
    return state;
}
std::vector<char *> cacheOriV = std::vector<char *>();
std::vector<char *> cacheDstV = std::vector<char *>();

char* __cdecl fixString(char* str, int size)
{
    try {
        if (!MtoolProc::instance->trsLoaded) {
            return str;
        }
        std::string traget = std::string(str);
        if (distance(traget.begin(), traget.end()) < 2) {
            return str;
        }

        for (int i = 0; i < cacheOriV.size(); i++) {
            if (traget.compare(cacheDstV[i]) == 0) {
                return cacheDstV[i];
            }
            if (traget.compare(cacheOriV[i]) == 0) {
                int len = strlen(cacheDstV[i]);
                memcpy(str, cacheDstV[i], len);
                if (size - len > 0) {
                    memset(str + len, 0, size - len);
                }
                return cacheDstV[i];
            }
        }

        const char* outStr = str;
        std::string retStr;
        try {
            json::array_t args;
            args.push_back(traget);
            json ret = MtoolProc::instance->doTask("trs", args, 3000);
            retStr = ret["ret"].get<std::string>();
            outStr = retStr.c_str();
        }
        catch (std::exception e) {
            return str;
        }

        int len = strlen(outStr);
        char* cacheStr = (char*)malloc(len + 1);
        memcpy((void*)cacheStr, outStr, len + 1);
        cacheStr[len] = 0;


        // 若缓存已达到 1000 条，移除最早的一条
        if (cacheOriV.size() >= 1000) {
            // 释放最前面一条对应的内存
            free(cacheOriV.front());
            cacheOriV.erase(cacheOriV.begin());
            free(cacheDstV.front());
            cacheDstV.erase(cacheDstV.begin());
        }

        int nlen = strlen(cacheStr);
        cacheDstV.push_back(cacheStr);
        int tlen = traget.length();
        char* cacheOri = (char*)malloc(tlen + 1);
        memcpy(cacheOri, traget.c_str(), tlen);
        cacheOri[tlen] = 0;
        cacheOriV.push_back(cacheOri);
        //zeroMemory
        for(int i = 0; i < size; i++) {
            str[i] = 0;
        }
        int copyLen = (((size) < (nlen)) ? (size) : (nlen));
        if (copyLen > 0) {
            memcpy(str, cacheStr, copyLen);
            str[copyLen] = 0;
        }
        //str[nlen + 1] = 0;
        return cacheStr;
    }
    catch (std::exception& err) {
        return str;
    }
}

VALUE rb_mri_fixString(VALUE self, VALUE arg, VALUE len) {
    int state = 0;
    VALUE str_val;

    // 使用rb_protect调用rb_string_value_ptr获取字符串指针
    // 注意：rb_string_value_ptr函数期望接收VALUE*类型参数
    str_val = rb_protect((VALUE (*)(VALUE))rb_string_value_ptr, (VALUE)&arg, &state);

    if (state) {
        // 如果发生异常，清除异常状态并返回原始参数
        rb_set_errinfo(Qnil);  // 根据文档警告，必须清除错误信息
        return arg;
    }

    // 获取C字符串指针
    char* str = (char*)str_val;

    // 调用fixString修正字符串，获取长度参数
    int length = NUM2INT(len);  // 使用NUM2INT代替rb_fix2int以支持更广泛的整数类型
    char* out = fixString(str, length);

    // 使用修正后的字符串创建新的Ruby字符串
    VALUE result = rb_str_new_cstr(out);

    // 注意：这里不需要手动释放out指针，因为fixString内部已经缓存了字符串

    return result;
}

concurrent_queue<RGSSEvalStruct *> tq;

void MtoolProc::onEventLoop() {
    if (!this->m_initialized) {
        this->init();
    }
    RGSSEvalStruct *evalStruct;
    while (tq.try_pop(evalStruct)) {
        //debugout("eval script: %s\n", evalStruct->script);
        try {
            if (evalStruct->retUTF8Str) {
                evalStruct->ret = (vptr) (RGSSGetStringUTF8(evalStruct->script));
            } else {
                evalStruct->ret = RGSSEval(evalStruct->script);
            }
        }
        catch (...) {
            if (evalStruct->retUTF8Str) {
                evalStruct->ret = (vptr) "Err";
            } else {
                evalStruct->ret = 99;
            }
        }
        evalStruct->evalDone = true;
        evalStruct->cv.notify_all();
        if (evalStruct->free) {
            //free(evalStruct->script);
        }
    }
}

bool keyDownStatus[65535];
using json = nlohmann::json;
long taskId = 0;
std::map<long, MTOOL_WsTask *> wsTasks;

json MtoolProc::doTask(std::string cmd, json args, long timeoutMS) {
    if (!m_client->IsConnected()) {
        throw std::exception();
    }
    taskId++;
    long id = taskId;
    MTOOL_WsTask task{id};
    if (timeoutMS >= 0) {
        wsTasks.emplace(id, &task);
    }
    json sendJson;
    sendJson["id"] = id;
    sendJson["cmd"] = cmd;
    sendJson["args"] = args;
    std::string dump = sendJson.dump(-1, (char) 32, false, json::error_handler_t::replace);
    bool sendOK = this->m_client->SendTextMessage(dump);
    if (timeoutMS == -1) {
        return NULL;
    }
    if (!sendOK) {
        throw std::exception();
    }
    if (timeoutMS == 0) {
        task.cv.wait(task.lck);
    } else {
        if (timeoutMS >= 0) {
            std::cv_status cvs = task.cv.wait_for(task.lck, std::chrono::milliseconds(timeoutMS));
            if (cvs == std::cv_status::timeout) {
                wsTasks.erase(id);
                throw std::exception();
            }
        } else {
            return NULL;
        }
    }
    wsTasks.erase(id);
    if (task.ret.contains("err")) {
        if (timeoutMS == -1) { return NULL; }
        throw std::exception();

    }
    return task.ret;
}


void MtoolProc::handleTextMessage(const std::string &message) {
    // 处理文本消息
    //debugout("Received text message: %s\n", message.c_str());
    json j = json::parse(message.c_str());
    long id = j["id"].get<long>();
    if (j.contains("id") && j.contains("ret")) {
        if (wsTasks.count(id) != 0) {
            MTOOL_WsTask *wsTask = wsTasks[id];
            wsTask->ret = j;
            wsTask->cv.notify_one();
        }
        return;
    }
    std::thread([j = std::move(j), this]() {  // 值捕获 j，而不是引用捕获
/*        bool logTask = true;
        if (j.contains("cmd")) {
            if (j["cmd"].get<std::string>() == "whoareyou") {
                logTask = false;
            }
            if (j["cmd"].get<std::string>() == "cwd") {
                logTask = false;
            }
        }
        if (logTask) {
            debugout("Task: %s\n", j.dump().c_str());
        }*/
        std::string ret = processTask(j);
        json jret;
        jret["id"] = j["id"];
        jret["ret"] = ret;
        std::string dump = jret.dump(-1, (char) 32, false, json::error_handler_t::replace);
/*        if (logTask) {
            debugout("Task Return: %s\n", dump.c_str());
        }*/
        this->m_client->SendTextMessage(dump);
    }).detach();
}




RGSSEvalStruct *genEvalStructArg(char *script, bool returnUTF8Str) {
    RGSSEvalStruct *retS = new RGSSEvalStruct();
    retS->script = script;
    retS->ret = 0;
    retS->retUTF8Str = returnUTF8Str;
    retS->evalDone = false;
    retS->free = true;
    return retS;
}

RGSSEvalStruct *genEvalUTF8Struct(char *script) {
    return genEvalStructArg(script, true);
}

RGSSEvalStruct *genEvalStruct(char *script) {
    return genEvalStructArg(script, false);
}

int RGSSEvalLoop(char *script) {
    RGSSEvalStruct *retS = genEvalStruct(script);
    tq.push(retS);
    retS->cv.wait(retS->lck);
    return retS->ret;
}

int RGSSEvalLoopNofree(char *script) {
    RGSSEvalStruct *retS = genEvalStruct(script);
    retS->free = false;
    tq.push(retS);
    retS->cv.wait(retS->lck);
    return retS->ret;
}

char *RGSSEvalUTF8Loop(char *script) {
    RGSSEvalStruct *retS = genEvalUTF8Struct(script);
    tq.push(retS);
    retS->cv.wait(retS->lck);
    return (char *) retS->ret;
}

std::string MtoolProc::processTask(json data) {
    if (data.contains("cmd")) {
        std::string command = data["cmd"].get<std::string>();
        std::string arg0 = "";
        if (data["args"].size() > 0 && data["args"][0].is_string()) {
            arg0 = data["args"][0];
        }
        if (command.compare("eval") == 0) {
            return RGSSEvalUTF8Loop((char *) arg0.c_str());
        }

        if (command.compare("evalI") == 0) {
            return std::to_string(RGSSEvalLoop((char *) arg0.c_str()));
        }

        if (command.compare("evalIBin") == 0) {
            std::string script;
            if (arg0.c_str()[0] == 0x5b) {
                json::array_t ca = json::parse(arg0);
                for (int i = 0; i < ca.size(); i++) {
                    script.push_back((ca[i].get<char>()) ^ 123);
                }
            } else {
                script = arg0;
            }
            return std::to_string(RGSSEvalLoop((char *) script.c_str()));
        }

        if (command.compare("whoareyou") == 0) {
            return "RGSS";
        }

        if (command.compare("cwd") == 0) {
            char buffer[PATH_MAX];
            if (getcwd(buffer, sizeof(buffer)) != nullptr) {
                return std::string(buffer);
            } else {
                return "Error getting current working directory";
            }
        }

        if (command.compare("applyTrs") == 0) {
            trsLoaded = false;
            cacheDstV.clear();
            cacheOriV.clear();
            trsLoaded = true;
            return "0";
        }
        if (command.compare("unloadTrs") == 0) {
            trsLoaded = false;
            cacheDstV.clear();
            cacheOriV.clear();
            return "0";
        }
        if (command.compare("clearCache") == 0) {
            cacheDstV.clear();
            cacheOriV.clear();
            return "0";
        }

    }
    return "unknown command";
}

void MtoolProc::handleBinaryMessage(const std::vector<uint8_t> &data) {
    // 处理二进制消息
    //debugout("Received binary message of size: %zu\n", data.size());
}

void MtoolProc::handleError(const std::string &error) {
    // 处理错误
    debugout("Error: %s\n", error.c_str());
    //if (!m_client->IsConnected()) {
    //    debugout("MTool client is not connected, attempting to reconnect...\n");
    //    m_client->ConnectAsync();
    //}
}

void MtoolProc::handleConnected() {
    // 处理连接成功事件
    debugout("Connected to MTool server at %s:%d\n", m_host.c_str(), m_port);
}

void MtoolProc::handleClosed() {
    // 处理连接关闭事件
    debugout("Connection to MTool server closed.\n");
    //if (!m_client->IsConnected()) {
        //debugout("MTool client is not connected, attempting to reconnect...\n");
        //m_client->ConnectAsync();
    //}
}

#ifdef __ANDROID__
// 初始化JNI全局变量
JavaVM* MtoolProc::javaVM = nullptr;
jclass MtoolProc::activityClass = nullptr;
jmethodID MtoolProc::notifyLoadingStatusMethod = nullptr;

void MtoolProc::initJNI(JNIEnv* env) {
    // 保存JavaVM引用
    env->GetJavaVM(&javaVM);

    // 查找MKXPZActivity类
    jclass localClass = env->FindClass("app/mtool/mtoolmobile/mkxpz/MKXPZActivity");
    if (localClass == nullptr) {
        debugout("无法找到MKXPZActivity类");
        return;
    }

    // 创建全局引用，防止类被GC
    activityClass = (jclass)env->NewGlobalRef(localClass);
    env->DeleteLocalRef(localClass);

    // 获取notifyLoadingStatus方法ID
    notifyLoadingStatusMethod = env->GetStaticMethodID(
        activityClass,
        "notifyLoadingStatus",
        "(I)V"
    );

    if (notifyLoadingStatusMethod == nullptr) {
        debugout("无法获取notifyLoadingStatus方法");
        return;
    }

    debugout("MtoolProc JNI初始化完成");
}

void MtoolProc::notifyLoadingStatus(int statusCode) {
    if (javaVM == nullptr || activityClass == nullptr || notifyLoadingStatusMethod == nullptr) {
        debugout("JNI环境未初始化，无法通知加载状态");
        return;
    }

    // 获取JNIEnv
    JNIEnv* env = nullptr;
    bool detach = false;

    // 获取当前线程JNIEnv
    int getEnvResult = javaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (getEnvResult == JNI_EDETACHED) {
        // 如果当前线程未附加到JVM，则附加它
        if (javaVM->AttachCurrentThread(&env, nullptr) == JNI_OK) {
            detach = true;
        } else {
            debugout("无法附加线程到JVM");
            return;
        }
    } else if (getEnvResult != JNI_OK) {
        debugout("获取JNIEnv失败");
        return;
    }

    // 调用Java方法
    env->CallStaticVoidMethod(activityClass, notifyLoadingStatusMethod, (jint)statusCode);

    // 如果之前附加了线程，则需要分离
    if (detach) {
        javaVM->DetachCurrentThread();
    }
}
#endif

// MtoolProc.cpp ends here


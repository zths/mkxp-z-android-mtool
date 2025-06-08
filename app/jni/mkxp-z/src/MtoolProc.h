//
// Created by root on 5/29/25.
//

#ifndef MKXP_Z_MTOOLPROC_H
#define MKXP_Z_MTOOLPROC_H

#include <string>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "MToolClient.h"

#ifdef __ANDROID__
#include <jni.h>
#endif

// 解决Ruby 1.9.3头文件与json.hpp的strtod冲突
#ifdef strtod
#undef strtod
#endif

#include "json.hpp"
extern int MtoolServerport;

struct MTOOL_WsTask
{
    long taskid;
    std::condition_variable cv;
    std::mutex mux;
    std::unique_lock<std::mutex> lck = std::unique_lock<std::mutex>(mux);
    nlohmann::json ret;
};
class MtoolProc {
public:
    MtoolProc();
    static MtoolProc* instance;
    static void staticCall();

#ifdef __ANDROID__
    // JNI 方法：通知 Java 层加载状态
    static void notifyLoadingStatus(int statusCode);
    // JNI 环境初始化
    static void initJNI(JNIEnv* env);
    static JavaVM* javaVM;
    static jclass activityClass;
    static jmethodID notifyLoadingStatusMethod;
#endif

    bool trsLoaded;

    nlohmann::json doTask(std::string cmd, nlohmann::json args, long timeoutMS = 0);
private:
    bool m_initialized = false;
    std::string m_host;
    int m_port;
    MToolClient* m_client;

    void init();
    void onEventLoop();
    void handleTextMessage(const std::string& message);
    void handleBinaryMessage(const std::vector<uint8_t>& data);
    void handleError(const std::string& error);
    void handleConnected();
    void handleClosed();

    std::string processTask(nlohmann::json j);
};
extern MtoolProc *mtoolProc;

#endif //MKXP_Z_MTOOLPROC_H


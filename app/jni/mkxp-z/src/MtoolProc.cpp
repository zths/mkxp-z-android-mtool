//
// Created by root on 5/29/25.
//

#include "MtoolProc.h"
#include "jni.h"
#include "json.hpp"
#include <thread>
#include <unistd.h>
#include "android/log.h"

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


void MtoolProc::init() {
    if (this->m_initialized) {
        return;
    }
    this->m_initialized = true;
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


void MtoolProc::onEventLoop() {
    if (!this->m_initialized) {
        this->init();
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
    debugout("Received text message: %s\n", message.c_str());
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
    std::thread th([&j, this, id]() {
        std::string ret = processTask(j);
        json jret;
        jret["id"] = j["id"];
        jret["ret"] = ret;
        std::string dump = jret.dump(-1, (char) 32, false, json::error_handler_t::replace);
        this->m_client->SendTextMessage(dump);
    });
}

std::vector<char *> cacheOriV = std::vector<char *>();
std::vector<char *> cacheDstV = std::vector<char *>();


std::string RGSSEvalUTF8Loop(std::string script) {
    return "";
}

int RGSSEvalLoop(std::string script) {
    return 0;
}

std::string MtoolProc::processTask(json data) {
    return "";
    if (data.contains("cmd")) {
        std::string command = data["cmd"].get<std::string>();
        std::string arg0 = "";
        if (data["args"].size() > 0 && data["args"][0].is_string()) {
            arg0 = data["args"][0];
        }
        if (command.compare("eval") == 0) {
            return RGSSEvalUTF8Loop(arg0);
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
}

void MtoolProc::handleBinaryMessage(const std::vector<uint8_t> &data) {
    // 处理二进制消息
    //debugout("Received binary message of size: %zu\n", data.size());
}

void MtoolProc::handleError(const std::string &error) {
    // 处理错误
    debugout("Error: %s\n", error.c_str());
    if (!m_client->IsConnected()) {
        debugout("MTool client is not connected, attempting to reconnect...\n");
        m_client->ConnectAsync();
    }
}

void MtoolProc::handleConnected() {
    // 处理连接成功事件
    debugout("Connected to MTool server at %s:%d\n", m_host.c_str(), m_port);
}

void MtoolProc::handleClosed() {
    // 处理连接关闭事件
    debugout("Connection to MTool server closed.\n");
}
// MtoolProc.cpp ends here


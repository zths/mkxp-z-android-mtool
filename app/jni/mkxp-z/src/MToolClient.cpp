#include "MToolClient.h"

#include <cstring>
#include <thread>
#include <queue>
#include <condition_variable>
//#include "jni.h"
#include <android/log.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    #define SOCKET_INVALID INVALID_SOCKET
    #define SOCKET_ERROR_VAL SOCKET_ERROR
    #define CLOSE_SOCKET(s) closesocket(s)
    #define GET_SOCKET_ERROR WSAGetLastError()
    inline int init_socket_system() {
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    inline void cleanup_socket_system() {
        WSACleanup();
    }
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netdb.h>
    using socket_t = int;
    #define SOCKET_INVALID -1
    #define SOCKET_ERROR_VAL -1
    #define CLOSE_SOCKET(s) close(s)
    #define GET_SOCKET_ERROR errno
    inline int init_socket_system() { return 0; }
    inline void cleanup_socket_system() { }
#endif

class MToolClientImpl {
public:
    MToolClientImpl(const std::string& host, int port, const std::string& path) 
        : m_host(host), m_port(port), m_path(path), m_socket(SOCKET_INVALID), 
          m_connected(false), m_shouldRun(false) {
        init_socket_system();
    }

    ~MToolClientImpl() {
        Close();
        cleanup_socket_system();
    }

    bool Connect() {
        if (m_connected) return true;
        m_connecting = true;
        std::lock_guard<std::mutex> lock(m_mutex);

        // 创建套接字
        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == SOCKET_INVALID) {
            ReportError("创建套接字失败");
            m_connecting = false;
            return false;
        }

        // 解析主机名
        struct addrinfo hints = {}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        if (getaddrinfo(m_host.c_str(), std::to_string(m_port).c_str(), &hints, &result) != 0) {
            ReportError("解析主机名失败: " + m_host);
            CLOSE_SOCKET(m_socket);
            m_socket = SOCKET_INVALID;
            m_connecting = false;
            return false;
        }

        // 设置TCP_NODELAY（禁用Nagle算法）
        int flag = 1;
        setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

        // 设置缓冲区大小
        int bufferSize = 64 * 1024; // 64KB
        setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&bufferSize, sizeof(bufferSize));
        setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&bufferSize, sizeof(bufferSize));

        // 连接到服务器
        if (connect(m_socket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR_VAL) {
            ReportError("连接到服务器失败");
            freeaddrinfo(result);
            CLOSE_SOCKET(m_socket);
            m_socket = SOCKET_INVALID;
            m_connecting = false;
            return false;
        }

        freeaddrinfo(result);

        // 发送HTTP升级请求
        std::string upgradeRequest = "GET " + m_path + " HTTP/1.1\r\n"
                                    "Host: " + m_host + ":" + std::to_string(m_port) + "\r\n"
                                    "Connection: Upgrade\r\n"
                                    "Upgrade: mtoolsocket\r\n\r\n";

        if (send(m_socket, upgradeRequest.c_str(), (int)upgradeRequest.length(), 0) == SOCKET_ERROR_VAL) {
            ReportError("发送升级请求失败");
            CLOSE_SOCKET(m_socket);
            m_socket = SOCKET_INVALID;
            m_connecting = false;
            return false;
        }

        // 读取服务器响应
        char responseBuffer[1024] = {0};
        int bytesRead = recv(m_socket, responseBuffer, sizeof(responseBuffer) - 1, 0);
        if (bytesRead <= 0) {
            ReportError("读取升级响应失败");
            CLOSE_SOCKET(m_socket);
            m_socket = SOCKET_INVALID;
            m_connecting = false;
            return false;
        }

        responseBuffer[bytesRead] = 0;
        std::string response(responseBuffer);

        // 检查响应是否包含 "okmtool"
        if (response.find("okmtool") == std::string::npos) {
            ReportError("升级到mtoolsocket协议失败");
            CLOSE_SOCKET(m_socket);
            m_socket = SOCKET_INVALID;
            m_connecting = false;
            return false;
        }

        m_connected = true;
        m_shouldRun = true;

        // 启动接收线程
        m_receiveThread = std::thread(&MToolClientImpl::ReceiveThread, this);

        // 调用连接回调
        if (m_connectedCallback) {
            m_connectedCallback();
        }
        m_connecting = false;
        return true;
    }

    void ConnectAsync() {
        std::thread([this]() {
            if(!this->Connect()){
                // 如果连接失败，调用错误回调
                if (m_errorCallback) {
                    m_errorCallback("Connect failed");
                }
            }
        }).detach();
    }

    bool SendTextMessage(const std::string& message) {
        return SendData(message.data(), message.size(), 1); // 类型1为文本消息
    }

    bool SendBinary(const std::vector<uint8_t>& data) {
        return SendData(data.data(), data.size(), 2); // 类型2为二进制消息
    }

    void Close() {
        if (!m_connected) return;

        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_connected) {
            m_shouldRun = false;

            // 发送关闭消息
            uint8_t header[8] = {0};
            // 设置长度为0
            // 设置类型为255
            *((uint32_t*)&header[4]) = 255;
            
            send(m_socket, (const char*)header, sizeof(header), 0);

            // 等待接收线程结束
            if (m_receiveThread.joinable()) {
                m_receiveThread.join();
            }

            CLOSE_SOCKET(m_socket);
            m_socket = SOCKET_INVALID;
            m_connected = false;

            // 调用关闭回调
            if (m_closedCallback) {
                m_closedCallback();
            }
        }
    }

    bool IsConnected() const {
        return m_connected;
    }

    bool IsConnecting() const {
        return m_connecting;
    }

    void SetMessageCallback(MToolClient::MessageCallback callback) {
        m_messageCallback = std::move(callback);
    }

    void SetBinaryMessageCallback(MToolClient::BinaryMessageCallback callback) {
        m_binaryMessageCallback = std::move(callback);
    }

    void SetErrorCallback(MToolClient::ErrorCallback callback) {
        m_errorCallback = std::move(callback);
    }

    void SetClosedCallback(MToolClient::StateCallback callback) {
        m_closedCallback = std::move(callback);
    }

    void SetConnectedCallback(MToolClient::StateCallback callback) {
        m_connectedCallback = std::move(callback);
    }

private:
    bool SendData(const void* data, size_t size, uint32_t type) {
        if (!m_connected || m_socket == SOCKET_INVALID) {
            ReportError("客户端未连接");
            return false;
        }

        std::lock_guard<std::mutex> lock(m_sendMutex);

        // 构建消息头
        uint8_t header[8] = {0};
        *((uint32_t*)&header[0]) = (uint32_t)size; // 小端序
        *((uint32_t*)&header[4]) = type;           // 小端序

        // 发送消息头
        if (send(m_socket, (const char*)header, sizeof(header), 0) == SOCKET_ERROR_VAL) {
            ReportError("发送消息头失败");
            return false;
        }

        // 发送消息体
        if (size > 0) {
            if (send(m_socket, (const char*)data, (int)size, 0) == SOCKET_ERROR_VAL) {
                ReportError("发送消息体失败");
                return false;
            }
        }

        return true;
    }

    void ReceiveThread() {
        while (m_shouldRun && m_connected) {
            // 读取消息头（8字节）
            uint8_t headerBuffer[8] = {0};
            if (!ReadExact(headerBuffer, sizeof(headerBuffer))) {
                break;
            }

            // 解析长度和类型
            uint32_t length = *((uint32_t*)&headerBuffer[0]); // 小端序
            uint32_t type = *((uint32_t*)&headerBuffer[4]);   // 小端序

            // 处理关闭消息
            if (type == 255) {
                break;
            }

            // 读取消息内容
            if (length > 0) {
                std::vector<uint8_t> dataBuffer(length);
                if (!ReadExact(dataBuffer.data(), length)) {
                    break;
                }

                // 处理消息
                if (type == 2 && m_binaryMessageCallback) {
                    // 二进制消息
                    m_binaryMessageCallback(dataBuffer);
                }
                else if (type == 1 && m_messageCallback) {
                    // 文本消息
                    std::string message(dataBuffer.begin(), dataBuffer.end());
                    m_messageCallback(message);
                }
            }
        }

        // 如果接收线程结束但未主动关闭，则调用关闭回调
        if (m_connected) {
            m_connected = false;
            
            if (m_closedCallback) {
                m_closedCallback();
            }
        }
    }

    bool ReadExact(void* buffer, size_t size) {
        size_t bytesRead = 0;
        char* buf = static_cast<char*>(buffer);

        while (bytesRead < size && m_shouldRun) {
            int result = recv(m_socket, buf + bytesRead, (int)(size - bytesRead), 0);
            
            if (result <= 0) {
                if (result == 0) {
                    // 连接已关闭
                    ReportError("连接已关闭");
                }
                else {
                    // 错误
                    ReportError("接收数据失败: " + std::to_string(GET_SOCKET_ERROR));
                }
                return false;
            }

            bytesRead += result;
        }

        return bytesRead == size;
    }

    void ReportError(const std::string& error) {
        if (m_errorCallback) {
            m_errorCallback(error);
        }
    }

private:
    std::string m_host;
    int m_port;
    std::string m_path;
    socket_t m_socket;
    std::atomic<bool> m_connected;
    std::atomic<bool> m_connecting;
    std::atomic<bool> m_shouldRun;
    std::thread m_receiveThread;
    
    std::mutex m_mutex;
    std::mutex m_sendMutex;

    MToolClient::MessageCallback m_messageCallback;
    MToolClient::BinaryMessageCallback m_binaryMessageCallback;
    MToolClient::ErrorCallback m_errorCallback;
    MToolClient::StateCallback m_closedCallback;
    MToolClient::StateCallback m_connectedCallback;
};

// MToolClient实现
MToolClient::MToolClient(const std::string& host, int port, const std::string& path)
    : m_impl(new MToolClientImpl(host, port, path)) {
}

MToolClient::~MToolClient() {
    // unique_ptr会自动调用impl的析构函数
}

bool MToolClient::Connect() {
    return m_impl->Connect();
}

void MToolClient::ConnectAsync() {
    m_impl->ConnectAsync();
}

bool MToolClient::SendTextMessage(const std::string& message) {
    return m_impl->SendTextMessage(message);
}

bool MToolClient::SendBinary(const std::vector<uint8_t>& data) {
    return m_impl->SendBinary(data);
}

void MToolClient::Close() {
    m_impl->Close();
}

bool MToolClient::IsConnected() const {
    return m_impl->IsConnected();
}

bool MToolClient::IsConnecting() const {
    return m_impl->IsConnecting();
}

void MToolClient::SetMessageCallback(MessageCallback callback) {
    m_impl->SetMessageCallback(std::move(callback));
}

void MToolClient::SetBinaryMessageCallback(BinaryMessageCallback callback) {
    m_impl->SetBinaryMessageCallback(std::move(callback));
}

void MToolClient::SetErrorCallback(ErrorCallback callback) {
    m_impl->SetErrorCallback(std::move(callback));
}

void MToolClient::SetClosedCallback(StateCallback callback) {
    m_impl->SetClosedCallback(std::move(callback));
}

void MToolClient::SetConnectedCallback(StateCallback callback) {
    m_impl->SetConnectedCallback(std::move(callback));
}

#ifdef __ANDROID__
// JNI实现部分
#include <android/log.h>

#define LOG_TAG "MToolClient"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 全局引用
static jclass g_javaClientClass = nullptr;
static jmethodID g_onMessageMethod = nullptr;
static jmethodID g_onBinaryMessageMethod = nullptr;
static jmethodID g_onErrorMethod = nullptr;
static jmethodID g_onClosedMethod = nullptr;
static jmethodID g_onConnectedMethod = nullptr;

// JNI帮助函数
static JNIEnv* GetJNIEnv(JavaVM* vm, bool& needDetach) {
    JNIEnv* env = nullptr;
    int status = vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    needDetach = false;
    
    if (status == JNI_EDETACHED) {
        status = vm->AttachCurrentThread(&env, nullptr);
        if (status < 0) {
            LOGE("获取JNI环境失败");
            return nullptr;
        }
        needDetach = true;
    }
    
    return env;
}

// JNI方法
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_mtool_MToolClient_nativeCreate(JNIEnv* env, jobject thiz, jstring host, jint port, jstring path) {
    const char* hostStr = env->GetStringUTFChars(host, nullptr);
    const char* pathStr = env->GetStringUTFChars(path, nullptr);
    
    MToolClient* client = new MToolClient(hostStr, port, pathStr);
    
    env->ReleaseStringUTFChars(host, hostStr);
    env->ReleaseStringUTFChars(path, pathStr);
    
    // 获取Java对象的全局引用
    jobject globalRef = env->NewGlobalRef(thiz);
    
    // 获取JavaVM
    JavaVM* vm;
    env->GetJavaVM(&vm);
    
    // 设置回调
    client->SetMessageCallback([vm, globalRef](const std::string& message) {
        bool needDetach = false;
        JNIEnv* env = GetJNIEnv(vm, needDetach);
        
        if (env) {
            jstring jMessage = env->NewStringUTF(message.c_str());
            env->CallVoidMethod(globalRef, g_onMessageMethod, jMessage);
            env->DeleteLocalRef(jMessage);
            
            if (needDetach) {
                vm->DetachCurrentThread();
            }
        }
    });
    
    client->SetBinaryMessageCallback([vm, globalRef](const std::vector<uint8_t>& data) {
        bool needDetach = false;
        JNIEnv* env = GetJNIEnv(vm, needDetach);
        
        if (env) {
            jbyteArray jData = env->NewByteArray(data.size());
            env->SetByteArrayRegion(jData, 0, data.size(), reinterpret_cast<const jbyte*>(data.data()));
            env->CallVoidMethod(globalRef, g_onBinaryMessageMethod, jData);
            env->DeleteLocalRef(jData);
            
            if (needDetach) {
                vm->DetachCurrentThread();
            }
        }
    });
    
    client->SetErrorCallback([vm, globalRef](const std::string& error) {
        bool needDetach = false;
        JNIEnv* env = GetJNIEnv(vm, needDetach);
        
        if (env) {
            jstring jError = env->NewStringUTF(error.c_str());
            env->CallVoidMethod(globalRef, g_onErrorMethod, jError);
            env->DeleteLocalRef(jError);
            
            if (needDetach) {
                vm->DetachCurrentThread();
            }
        }
    });
    
    client->SetClosedCallback([vm, globalRef]() {
        bool needDetach = false;
        JNIEnv* env = GetJNIEnv(vm, needDetach);
        
        if (env) {
            env->CallVoidMethod(globalRef, g_onClosedMethod);
            
            if (needDetach) {
                vm->DetachCurrentThread();
            }
        }
    });
    
    client->SetConnectedCallback([vm, globalRef]() {
        bool needDetach = false;
        JNIEnv* env = GetJNIEnv(vm, needDetach);
        
        if (env) {
            env->CallVoidMethod(globalRef, g_onConnectedMethod);
            
            if (needDetach) {
                vm->DetachCurrentThread();
            }
        }
    });
    
    return reinterpret_cast<jlong>(client);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mtool_MToolClient_nativeDestroy(JNIEnv* env, jobject thiz, jlong handle) {
    MToolClient* client = reinterpret_cast<MToolClient*>(handle);
    delete client;
    
    // 释放全局引用
    env->DeleteGlobalRef(thiz);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_mtool_MToolClient_nativeConnect(JNIEnv* env, jobject thiz, jlong handle) {
    MToolClient* client = reinterpret_cast<MToolClient*>(handle);
    return client->Connect();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mtool_MToolClient_nativeConnectAsync(JNIEnv* env, jobject thiz, jlong handle) {
    MToolClient* client = reinterpret_cast<MToolClient*>(handle);
    client->ConnectAsync();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_mtool_MToolClient_nativeSendMessage(JNIEnv* env, jobject thiz, jlong handle, jstring message) {
    MToolClient* client = reinterpret_cast<MToolClient*>(handle);
    const char* messageStr = env->GetStringUTFChars(message, nullptr);
    bool result = client->SendTextMessage(messageStr);
    env->ReleaseStringUTFChars(message, messageStr);
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_mtool_MToolClient_nativeSendBinary(JNIEnv* env, jobject thiz, jlong handle, jbyteArray data) {
    MToolClient* client = reinterpret_cast<MToolClient*>(handle);
    
    jsize length = env->GetArrayLength(data);
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    
    std::vector<uint8_t> buffer(bytes, bytes + length);
    bool result = client->SendBinary(buffer);
    
    env->ReleaseByteArrayElements(data, bytes, JNI_ABORT);
    return result;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mtool_MToolClient_nativeClose(JNIEnv* env, jobject thiz, jlong handle) {
    MToolClient* client = reinterpret_cast<MToolClient*>(handle);
    client->Close();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_mtool_MToolClient_nativeIsConnected(JNIEnv* env, jobject thiz, jlong handle) {
    MToolClient* client = reinterpret_cast<MToolClient*>(handle);
    return client->IsConnected();
}

void MToolClient::InitJNI(JNIEnv* env) {
    // 缓存Java类和方法ID
    jclass localClass = env->FindClass("com/example/mtool/MToolClient");
    g_javaClientClass = (jclass)env->NewGlobalRef(localClass);
    
    g_onMessageMethod = env->GetMethodID(g_javaClientClass, "onMessage", "(Ljava/lang/String;)V");
    g_onBinaryMessageMethod = env->GetMethodID(g_javaClientClass, "onBinaryMessage", "([B)V");
    g_onErrorMethod = env->GetMethodID(g_javaClientClass, "onError", "(Ljava/lang/String;)V");
    g_onClosedMethod = env->GetMethodID(g_javaClientClass, "onClosed", "()V");
    g_onConnectedMethod = env->GetMethodID(g_javaClientClass, "onConnected", "()V");
    
    env->DeleteLocalRef(localClass);
}

#endif // __ANDROID__ 
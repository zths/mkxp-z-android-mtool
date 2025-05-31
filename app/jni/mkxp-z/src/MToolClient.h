#ifndef MTOOL_CLIENT_H
#define MTOOL_CLIENT_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>

#ifdef __ANDROID__
#include <jni.h>
#endif

// 前向声明
class MToolClientImpl;

class MToolClient {
public:
    // 回调函数类型定义
    using MessageCallback = std::function<void(const std::string&)>;
    using BinaryMessageCallback = std::function<void(const std::vector<uint8_t>&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using StateCallback = std::function<void()>;

    MToolClient(const std::string& host, int port, const std::string& path = "/");
    ~MToolClient();

    // 禁止拷贝和赋值
    MToolClient(const MToolClient&) = delete;
    MToolClient& operator=(const MToolClient&) = delete;

    // 连接方法
    bool Connect();
    void ConnectAsync();

    // 发送方法 - 避免与Windows API SendMessage冲突
    bool SendTextMessage(const std::string& message);
    bool SendBinary(const std::vector<uint8_t>& data);

    // 关闭连接
    void Close();

    // 检查连接状态
    bool IsConnected() const;
    bool IsConnecting() const;

    // 设置回调
    void SetMessageCallback(MessageCallback callback);
    void SetBinaryMessageCallback(BinaryMessageCallback callback);
    void SetErrorCallback(ErrorCallback callback);
    void SetClosedCallback(StateCallback callback);
    void SetConnectedCallback(StateCallback callback);

#ifdef __ANDROID__
    // 为JNI提供的静态方法
    static void InitJNI(JNIEnv* env);
#endif

private:
    std::unique_ptr<MToolClientImpl> m_impl;
};

#endif // MTOOL_CLIENT_H 
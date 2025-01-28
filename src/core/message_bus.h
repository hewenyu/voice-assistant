#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "subscriber.h"
#include "message_types.h"

namespace core {

class MessageBus {
public:
    MessageBus() = default;
    ~MessageBus() = default;

    // 禁止拷贝和赋值
    MessageBus(const MessageBus&) = delete;
    MessageBus& operator=(const MessageBus&) = delete;

    // 发布消息
    void publish(const MessagePtr& message);

    // 订阅特定类型的消息
    void subscribe(const std::string& type, const SubscriberPtr& subscriber);

    // 取消订阅
    void unsubscribe(const std::string& type, const SubscriberPtr& subscriber);

    // 获取单例实例
    static MessageBus& get_instance();

private:
    std::unordered_map<std::string, std::vector<std::weak_ptr<ISubscriber>>> subscribers_;
    mutable std::mutex mutex_;

    // 清理失效的订阅者
    void cleanup_expired_subscribers(const std::string& type);
};

} // namespace core 
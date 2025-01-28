#pragma once

#include <functional>
#include <string>
#include "message_types.h"

namespace core {

// 订阅者接口
class ISubscriber {
public:
    virtual ~ISubscriber() = default;
    virtual void on_message(const MessagePtr& message) = 0;
    virtual std::string get_subscription_type() const = 0;
};

// 使用std::function的回调订阅者实现
class CallbackSubscriber : public ISubscriber {
public:
    using MessageCallback = std::function<void(const MessagePtr&)>;

    CallbackSubscriber(const std::string& type, MessageCallback callback)
        : type_(type), callback_(callback) {}

    void on_message(const MessagePtr& message) override {
        if (callback_) {
            callback_(message);
        }
    }

    std::string get_subscription_type() const override {
        return type_;
    }

private:
    std::string type_;
    MessageCallback callback_;
};

using SubscriberPtr = std::shared_ptr<ISubscriber>;

} // namespace core 
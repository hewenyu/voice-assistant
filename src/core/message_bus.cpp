#include "message_bus.h"
#include <algorithm>

namespace core {

MessageBus& MessageBus::get_instance() {
    static MessageBus instance;
    return instance;
}

void MessageBus::publish(const MessagePtr& message) {
    if (!message) return;

    std::lock_guard<std::mutex> lock(mutex_);
    const auto& type = message->get_type();
    
    auto it = subscribers_.find(type);
    if (it != subscribers_.end()) {
        cleanup_expired_subscribers(type);
        
        // 创建一个临时副本以防止在迭代过程中修改
        auto subscribers = it->second;
        for (const auto& weak_subscriber : subscribers) {
            if (auto subscriber = weak_subscriber.lock()) {
                subscriber->on_message(message);
            }
        }
    }
}

void MessageBus::subscribe(const std::string& type, const SubscriberPtr& subscriber) {
    if (!subscriber) return;

    std::lock_guard<std::mutex> lock(mutex_);
    auto& type_subscribers = subscribers_[type];
    
    // 检查是否已经订阅
    auto it = std::find_if(type_subscribers.begin(), type_subscribers.end(),
        [&subscriber](const std::weak_ptr<ISubscriber>& weak_sub) {
            auto sub = weak_sub.lock();
            return sub && sub.get() == subscriber.get();
        });
    
    if (it == type_subscribers.end()) {
        // 将shared_ptr转换为weak_ptr后存储
        type_subscribers.push_back(std::weak_ptr<ISubscriber>(subscriber));
    }
}

void MessageBus::unsubscribe(const std::string& type, const SubscriberPtr& subscriber) {
    if (!subscriber) return;

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subscribers_.find(type);
    if (it != subscribers_.end()) {
        auto& type_subscribers = it->second;
        
        type_subscribers.erase(
            std::remove_if(type_subscribers.begin(), type_subscribers.end(),
                [&subscriber](const std::weak_ptr<ISubscriber>& weak_sub) {
                    auto sub = weak_sub.lock();
                    return !sub || sub.get() == subscriber.get();
                }),
            type_subscribers.end()
        );
        
        if (type_subscribers.empty()) {
            subscribers_.erase(it);
        }
    }
}

void MessageBus::cleanup_expired_subscribers(const std::string& type) {
    auto it = subscribers_.find(type);
    if (it != subscribers_.end()) {
        auto& type_subscribers = it->second;
        
        type_subscribers.erase(
            std::remove_if(type_subscribers.begin(), type_subscribers.end(),
                [](const std::weak_ptr<ISubscriber>& weak_sub) {
                    return weak_sub.expired();
                }),
            type_subscribers.end()
        );
        
        if (type_subscribers.empty()) {
            subscribers_.erase(it);
        }
    }
}

} // namespace core 
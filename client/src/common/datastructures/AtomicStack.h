#pragma once

#include <atomic>

template<typename T>
class AtomicStack {
    struct Node {
        T* data;
        Node* next;
        Node(T* d, Node* n) : data(d), next(n) {}
    };
    std::atomic<Node*> m_head{nullptr};
public:
    AtomicStack() = default;

    AtomicStack(const AtomicStack&) = delete;
    AtomicStack& operator=(const AtomicStack&) = delete;
    AtomicStack(AtomicStack&&) = delete;
    AtomicStack& operator=(AtomicStack&&) = delete;

    ~AtomicStack() {
        Node* node = m_head.load(std::memory_order_acquire);
        while (node) {
            Node* next = node->next;
            delete node;
            node = next;
        }
    }

    void push(T* value) {
        auto* newNode = new Node(value, nullptr);
        Node* oldHead;
        do {
            oldHead = m_head.load(std::memory_order_acquire);
            newNode->next = oldHead;
        } while (!m_head.compare_exchange_weak(oldHead, newNode, std::memory_order_acq_rel, std::memory_order_acquire));
    }

    T* pop() {
        Node* oldHead;
        Node* newHead;
        do {
            oldHead = m_head.load(std::memory_order_acquire);
            if (!oldHead) return nullptr;
            newHead = oldHead->next;
        } while (!m_head.compare_exchange_weak(oldHead, newHead, std::memory_order_acq_rel, std::memory_order_acquire));
        T* value = oldHead->data;
        delete oldHead;
        return value;
    }

    bool empty() const {
        return m_head.load(std::memory_order_acquire) == nullptr;
    }
};

#pragma once

#include <atomic>

template<typename T>
class AtomicQueue {
    struct Node {
        T data;
        std::atomic<Node*> next;
        explicit Node(T d = nullptr, Node* n = nullptr) : data(d), next(n) {}
    };

    std::atomic<Node*> m_head;
    std::atomic<Node*> m_tail;

public:
    AtomicQueue() {
        Node* dummy = new Node();
        m_head.store(dummy, std::memory_order_relaxed);
        m_tail.store(dummy, std::memory_order_relaxed);
    }

    ~AtomicQueue() {
        Node* node = m_head.load(std::memory_order_relaxed);
        while (node) {
            Node* next = node->next.load(std::memory_order_relaxed);
            delete node;
            node = next;
        }
    }

    AtomicQueue(const AtomicQueue&) = delete;
    AtomicQueue& operator=(const AtomicQueue&) = delete;
    AtomicQueue(AtomicQueue&&) = delete;
    AtomicQueue& operator=(AtomicQueue&&) = delete;

    void push(T valuePtr) {
        Node* newNode = new Node(valuePtr);
        Node* tail = nullptr;
        while (true) {
            tail = m_tail.load(std::memory_order_acquire);
            Node* next = tail->next.load(std::memory_order_acquire);
            if (tail == m_tail.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    if (tail->next.compare_exchange_weak(
                            next, newNode,
                            std::memory_order_release,
                            std::memory_order_relaxed)) {
                        m_tail.compare_exchange_strong(
                            tail, newNode,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        return;
                    }
                } else {
                    m_tail.compare_exchange_weak(
                        tail, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                }
            }
        }
    }

    T pop() {
        Node* head = nullptr;
        while (true) {
            head = m_head.load(std::memory_order_acquire);
            Node* tail = m_tail.load(std::memory_order_acquire);
            Node* next = head->next.load(std::memory_order_acquire);
            if (head == m_head.load(std::memory_order_acquire)) {
                if (head == tail) {
                    if (next == nullptr) {
                        return nullptr;
                    }
                    m_tail.compare_exchange_weak(
                        tail, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                } else {
                    T outPtr = next->data;
                    if (m_head.compare_exchange_weak(
                            head, next,
                            std::memory_order_release,
                            std::memory_order_relaxed)) {
                        delete head;
                        return outPtr;
                    }
                }
            }
        }
    }

    bool empty() const {
        Node* head = m_head.load(std::memory_order_acquire);
        Node* tail = m_tail.load(std::memory_order_acquire);
        Node* next = head->next.load(std::memory_order_acquire);
        return (head == tail) && (next == nullptr);
    }
};

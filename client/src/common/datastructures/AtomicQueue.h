#pragma once

#include <atomic>
#include <optional>
#include <memory>

template<typename T, typename Allocator = std::allocator<T>>
class AtomicQueue {
    Allocator m_allocator;

    struct Node {
        T data;
        std::atomic<Node*> next;
        Node() : data(), next(nullptr) {}
        explicit Node(const T& d) : data(d), next(nullptr) {}
    };

    using NodeAlloc = std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    using NodeTraits = std::allocator_traits<NodeAlloc>;

    std::atomic<Node*> m_head;
    std::atomic<Node*> m_tail;

public:
    AtomicQueue() {
        NodeAlloc nodeAlloc(m_allocator);
        Node* dummy = NodeTraits::allocate(nodeAlloc, 1);
        NodeTraits::construct(nodeAlloc, dummy);
        m_head.store(dummy, std::memory_order_relaxed);
        m_tail.store(dummy, std::memory_order_relaxed);
    }

    ~AtomicQueue() {
        NodeAlloc nodeAlloc(m_allocator);
        Node* node = m_head.load(std::memory_order_relaxed);
        while (node) {
            Node* next = node->next.load(std::memory_order_relaxed);
            NodeTraits::destroy(nodeAlloc, node);
            NodeTraits::deallocate(nodeAlloc, node, 1);
            node = next;
        }
    }

    AtomicQueue(const AtomicQueue&) = delete;
    AtomicQueue& operator=(const AtomicQueue&) = delete;
    AtomicQueue(AtomicQueue&&) = delete;
    AtomicQueue& operator=(AtomicQueue&&) = delete;

    void push(T value) {
        NodeAlloc nodeAlloc(m_allocator);
        Node* newNode = NodeTraits::allocate(nodeAlloc, 1);
        NodeTraits::construct(nodeAlloc, newNode, value);
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

    std::optional<T> tryPop() {
        NodeAlloc nodeAlloc(m_allocator);

        Node* head = nullptr;
        while (true) {
            head = m_head.load(std::memory_order_acquire);
            Node* tail = m_tail.load(std::memory_order_acquire);
            Node* next = head->next.load(std::memory_order_acquire);
            if (head == m_head.load(std::memory_order_acquire)) {
                if (head == tail) {
                    if (next == nullptr) {
                        return std::nullopt;
                    }
                    m_tail.compare_exchange_weak(
                        tail, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                } else {
                    T value = next->data;
                    if (m_head.compare_exchange_weak(
                            head, next,
                            std::memory_order_release,
                            std::memory_order_relaxed)) {
                        NodeTraits::destroy(nodeAlloc, head);
                        NodeTraits::deallocate(nodeAlloc, head, 1);
                        return value;
                    }
                }
            }
        }
    }

    [[nodiscard]] bool empty() const {
        Node* head = m_head.load(std::memory_order_acquire);
        Node* tail = m_tail.load(std::memory_order_acquire);
        Node* next = head->next.load(std::memory_order_acquire);
        return head == tail && next == nullptr;
    }
};

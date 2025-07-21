#pragma once

#ifdef __EMSCRIPTEN__
    #include <emscripten/emscripten.h>
    #ifdef __EMSCRIPTEN_PTHREADS__
    #include <emscripten/threading.h>
    #elifdef __EMSCRIPTEN_WASM_WORKERS__
    #include <emscripten/wasm_worker.h>
    #else
    #error "No threading model defined"
    #endif
#else
    #include <mutex>
    #include <thread>
#endif

namespace Threading {

    static void sleep(const int64_t milliseconds) {
#ifdef __EMSCRIPTEN__
        emscripten_thread_sleep(milliseconds);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
#endif
    }

    class Worker {
    public:
        Worker() = default;
        ~Worker() {
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
            pthread_join(ptid, nullptr);
#elif defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_WASM_WORKERS__)
            emscripten_terminate_wasm_worker(m_worker);
#else
            m_worker.join();
#endif
        }

        Worker(const Worker&) = delete;
        Worker& operator=(const Worker&) = delete;
        Worker(Worker&&) = delete;
        Worker& operator=(Worker&&) = delete;

        void start(void* (*funcPtr)(void*), void* arg) {
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
            pthread_create(&ptid, nullptr, funcPtr, arg);
#elif defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_WASM_WORKERS__)
            m_worker = emscripten_malloc_wasm_worker(1024 * 1024);
            static_assert(sizeof(arg) == sizeof(int), "Argument must be the same size as an int");
            emscripten_wasm_worker_post_function_vi(m_worker, reinterpret_cast<void(*)(int)>(funcPtr), reinterpret_cast<int>(arg));
#else
            m_worker = std::thread(funcPtr, arg);
#endif
        }

    private:
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
        pthread_t ptid;
#elif defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_WASM_WORKERS__)
        emscripten_wasm_worker_t m_worker;
#else
        std::thread m_worker;
#endif
    };

    class Lock {
    public:
        Lock() {
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
            pthread_mutex_init(&m_lock, nullptr);
#elif defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_WASM_WORKERS__)
            emscripten_lock_init(&m_lock);
#endif
        }
        ~Lock() = default;

        Lock(const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;
        Lock(Lock&&) = delete;
        Lock& operator=(Lock&&) = delete;

        void lock() {
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
            pthread_mutex_lock(&m_lock);
#elif defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_WASM_WORKERS__)
            emscripten_lock_busyspin_waitinf_acquire(&m_lock);
#else
            m_lock.lock();
#endif
        }

        void unlock() {
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
            pthread_mutex_unlock(&m_lock);
#elif defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_WASM_WORKERS__)
            emscripten_lock_release(&m_lock);
#else
            m_lock.unlock();
#endif
        }
    private:
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
        pthread_mutex_t m_lock = PTHREAD_MUTEX_INITIALIZER;
#elif defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_WASM_WORKERS__)
        emscripten_lock_t m_lock = EMSCRIPTEN_LOCK_T_STATIC_INITIALIZER;
#else
        std::mutex m_lock;
#endif
    };

    class ScopedLock {
    public:
        explicit ScopedLock(Lock* lock) : m_lock(lock) {
            m_lock->lock();
        }

        ~ScopedLock() {
            m_lock->unlock();
        }

    private:
        Lock* m_lock;
    };
}
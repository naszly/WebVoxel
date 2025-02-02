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

    static void Sleep(const int64_t milliseconds) {
#ifdef __EMSRIPTEN__
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
            emscripten_terminate_wasm_worker(m_Worker);
#else
            m_Worker.join();
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
            m_Worker = emscripten_malloc_wasm_worker(1024 * 1024);
            static_assert(sizeof(arg) == sizeof(int), "Argument must be the same size as an int");
            emscripten_wasm_worker_post_function_vi(m_Worker, reinterpret_cast<void(*)(int)>(funcPtr), reinterpret_cast<int>(arg));
#else
            m_Worker = std::thread(funcPtr, arg);
#endif
        }

    private:
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
        pthread_t ptid;
#elif defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_WASM_WORKERS__)
        emscripten_wasm_worker_t m_Worker;
#else
        std::thread m_Worker;
#endif
    };

    class Lock {
    public:
        Lock() {
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
            pthread_mutex_init(&m_Lock, nullptr);
#elif defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_WASM_WORKERS__)
            emscripten_lock_init(&m_Lock);
#endif
        }
        ~Lock() = default;

        Lock(const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;
        Lock(Lock&&) = delete;
        Lock& operator=(Lock&&) = delete;

        void lock() {
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
            pthread_mutex_lock(&m_Lock);
#elif defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_WASM_WORKERS__)
            emscripten_lock_busyspin_waitinf_acquire(&m_Lock);
#else
            m_Lock.lock();
#endif
        }

        void unlock() {
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
            pthread_mutex_unlock(&m_Lock);
#elif defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_WASM_WORKERS__)
            emscripten_lock_release(&m_Lock);
#else
            m_Lock.unlock();
#endif
        }
    private:
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
        pthread_mutex_t m_Lock = PTHREAD_MUTEX_INITIALIZER;
#elif defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_WASM_WORKERS__)
        emscripten_lock_t m_Lock = EMSCRIPTEN_LOCK_T_STATIC_INITIALIZER;
#else
        std::mutex m_Lock;
#endif
    };

    class ScopedLock {
    public:
        explicit ScopedLock(Lock* lock) : m_Lock(lock) {
            m_Lock->lock();
        }

        ~ScopedLock() {
            m_Lock->unlock();
        }

    private:
        Lock* m_Lock;
    };
}
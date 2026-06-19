/* -----------------------------------------------------------------------------
 * Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
 * See 'LICENSE' in the project root for license information.
 * -------------------------------------------------------------------------- */
#pragma once

#if !defined(__HIP_CPU_RT__)
    #error Private HIP-CPU RT implementation headers must not be included directly.
#endif

#include "event.hpp"
#include "helpers.hpp"
#include "stream.hpp"
#include "task.hpp"
#include "../../../../include/hip/hip_defines.h"
#include "../../../../include/hip/hip_enums.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <execution>
#include <forward_list>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable:4251) // TODO: temporary.
#endif

namespace hip
{
    namespace detail
    {
        // BEGIN CLASS RUNTIME
        class __HIP_API__ Runtime final {
            // DATA - STATICS
            inline static Stream internal_stream_{};
            inline static std::forward_list<Stream> streams_{};
            inline static std::atomic<bool> done_{false};
            inline static std::atomic<std::uint32_t> task_count_{0u};
            inline static std::condition_variable cv_{};
            inline static std::mutex cv_mutex_{};

            // IMPLEMENTATION - STATICS
            static
            hipError_t& last_error_() noexcept;
            static
            std::thread& processor_();
            static
            void wait_all_streams_();
        public:
            // STATICS
            static
            std::future<void> destroy_stream_async(Stream* s);
            static
            hipError_t last_error() noexcept;
            static
            std::future<Stream*> make_stream_async();
            static
            Stream* null_stream();
            static
            void push_task(Event* p, Stream* s);
            static
            hipError_t set_last_error(hipError_t e) noexcept;
            static
            void synchronize();
        };

        // IMPLEMENTATION - STATICS
        inline
        hipError_t& Runtime::last_error_() noexcept
        {
            static thread_local hipError_t r{hipSuccess};

            return r;
        }

        inline
        std::thread& Runtime::processor_()
        {
            static std::thread r{[]() {
                do {
                    using T = typename Stream::value_type;

                    T t{};
                    internal_stream_.apply([&t](auto&& ts) {
                        t = std::move(ts);
                    });

                    for (auto&& x : t) { bool nop; x(nop); }

                    bool backoff{true};
                    null_stream()->apply([&backoff](auto&& ts) {
                        backoff = std::empty(ts);
                    });

                    if (backoff) {
                        backoff = std::none_of(
                            std::begin(streams_),
                            std::end(streams_),
                            [](auto&& x) {
                            bool r{};
                            x.apply([&r](auto&& ts) { r = !std::empty(ts); });

                            return r;
                        });
                    }

                    if (!backoff) {
                        task_count_.store(0u, std::memory_order_release);
                        wait_all_streams_();
                    } else {
                        std::unique_lock<std::mutex> lck{cv_mutex_};
                        cv_.wait_for(lck, std::chrono::milliseconds{16}, []() {
                            return done_.load(std::memory_order_acquire) ||
                                task_count_.load(std::memory_order_acquire) != 0u;
                        });
                    }
                } while (!done_);
            }};
            static struct D { ~D() { done_ = true; cv_.notify_one(); r.join(); } } done{};

            return r;
        }

        inline
        void Runtime::wait_all_streams_()
        {
            using T = typename Stream::value_type;

            auto f{std::async(std::launch::async, []() {
                T t{};

                null_stream()->apply([&t](auto&& ts) { t = std::move(ts); });

                for (auto&& x : t) {
                    bool nop{};
                    x(nop);
                };
            })};

            std::for_each(
#if __cpp_lib_execution >= 201902L
                std::execution::par,
#else
                std::execution::seq,
#endif
                std::begin(streams_),
                std::end(streams_),
                [](auto&& x) {
                T t{};

                x.apply([&t](auto&& ts) { t = std::move(ts); });

                for (auto&& y : t) { bool nop{}; y(nop); }
            });

            return f.wait();
        }

        // STATICS
        inline
        std::future<void> Runtime::destroy_stream_async(Stream* s)
        {
            Task r{[=](auto&&) {
                streams_.remove_if([=](auto&& x) { return &x == s; });
            }};
            auto fut{r.get_future()};

            internal_stream_.apply([&r](auto&& ts) {
                ts.push_back(std::move(r));
            });

            return fut;
        }

        inline
        hipError_t Runtime::last_error() noexcept
        {
            return last_error_();
        }

        inline
        std::future<Stream*> Runtime::make_stream_async()
        {
            auto p{std::make_shared<std::promise<Stream*>>()};
            auto fut{p->get_future()};

            internal_stream_.apply([&p](auto&& ts) {
                ts.emplace_back([p = std::move(p)](auto&&) mutable {
                    p->set_value(&streams_.emplace_front());
                });
            });

            processor_();

            return fut;
        }

        inline
        Stream* Runtime::null_stream()
        {
            static auto& r{streams_.emplace_front()};

            processor_();

            return &r;
        }

        inline
        void Runtime::push_task(Event* p, Stream* s)
        {
            if (!s) {
                s = null_stream();
                mark_as_all_synchronising(*p);
            }

            Task r{[=](auto&&) { update_timestamp(*p); }};
            add_done_signal(*p, r.get_future());

            task_count_.fetch_add(1u, std::memory_order_release);
            s->apply([&r](auto&& ts) { ts.push_back(std::move(r)); });
            cv_.notify_one();
        }

        inline
        hipError_t Runtime::set_last_error(hipError_t e) noexcept
        {
            return std::exchange(last_error_(), e);
        }

        inline
        void Runtime::synchronize()
        {   // TODO: redo, this induces ordering requirements on the processor.
            Task r{[](auto&&) { wait_all_streams_(); }};
            auto f{r.get_future()};

            internal_stream_.apply([&r](auto&& ts) {
                ts.push_back(std::move(r));
            });

            processor_();

            return f.wait();
        }
        // END CLASS RUNTIME
    } // Namespace hip::detail.
} // Namespace hip.

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
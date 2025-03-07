// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "openvino/runtime/threading/cpu_streams_executor.hpp"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "dev/threading/parallel_custom_arena.hpp"
#include "dev/threading/thread_affinity.hpp"
#include "openvino/itt.hpp"
#include "openvino/runtime/system_conf.hpp"
#include "openvino/runtime/threading/executor_manager.hpp"
#include "openvino/runtime/threading/thread_local.hpp"
#include "threading/ie_cpu_streams_info.hpp"

using namespace InferenceEngine;

namespace ov {
namespace threading {
struct CPUStreamsExecutor::Impl {
    struct Stream {
#if OV_THREAD == OV_THREAD_TBB || OV_THREAD == OV_THREAD_TBB_AUTO
        struct Observer : public custom::task_scheduler_observer {
            CpuSet _mask;
            int _ncpus = 0;
            int _threadBindingStep = 0;
            int _offset = 0;
            int _cpuIdxOffset = 0;
            std::vector<int> _cpu_ids;
            Observer(custom::task_arena& arena,
                     CpuSet mask,
                     int ncpus,
                     const int streamId,
                     const int threadsPerStream,
                     const int threadBindingStep,
                     const int threadBindingOffset,
                     const int cpuIdxOffset = 0,
                     const std::vector<int> cpu_ids = {})
                : custom::task_scheduler_observer(arena),
                  _mask{std::move(mask)},
                  _ncpus(ncpus),
                  _threadBindingStep(threadBindingStep),
                  _offset{streamId * threadsPerStream + threadBindingOffset},
                  _cpuIdxOffset(cpuIdxOffset),
                  _cpu_ids(cpu_ids) {}
            void on_scheduler_entry(bool) override {
                pin_thread_to_vacant_core(_offset + tbb::this_task_arena::current_thread_index(),
                                          _threadBindingStep,
                                          _ncpus,
                                          _mask,
                                          _cpu_ids,
                                          _cpuIdxOffset);
            }
            void on_scheduler_exit(bool) override {
                pin_current_thread_by_mask(_ncpus, _mask);
            }
            ~Observer() override = default;
        };
#endif
        explicit Stream(Impl* impl) : _impl(impl) {
            {
                std::lock_guard<std::mutex> lock{_impl->_streamIdMutex};
                if (_impl->_streamIdQueue.empty()) {
                    _streamId = _impl->_streamId++;
                } else {
                    _streamId = _impl->_streamIdQueue.front();
                    _impl->_streamIdQueue.pop();
                }
            }
            _numaNodeId = _impl->_config._streams
                              ? _impl->_usedNumaNodes.at((_streamId % _impl->_config._streams) /
                                                         ((_impl->_config._streams + _impl->_usedNumaNodes.size() - 1) /
                                                          _impl->_usedNumaNodes.size()))
                              : _impl->_usedNumaNodes.at(_streamId % _impl->_usedNumaNodes.size());
#if OV_THREAD == OV_THREAD_TBB || OV_THREAD == OV_THREAD_TBB_AUTO
            if (is_cpu_map_available() && _impl->_config._streams_info_table.size() > 0) {
                init_stream();
            } else {
                init_stream_legacy();
            }
#elif OV_THREAD == OV_THREAD_OMP
            omp_set_num_threads(_impl->_config._threadsPerStream);
            if (!check_open_mp_env_vars(false) && (ThreadBindingType::NONE != _impl->_config._threadBindingType)) {
                CpuSet processMask;
                int ncpus = 0;
                std::tie(processMask, ncpus) = get_process_mask();
                if (nullptr != processMask) {
                    parallel_nt(_impl->_config._threadsPerStream, [&](int threadIndex, int threadsPerStream) {
                        int thrIdx = _streamId * _impl->_config._threadsPerStream + threadIndex +
                                     _impl->_config._threadBindingOffset;
                        pin_thread_to_vacant_core(thrIdx, _impl->_config._threadBindingStep, ncpus, processMask);
                    });
                }
            }
#elif OV_THREAD == OV_THREAD_SEQ
            if (ThreadBindingType::NUMA == _impl->_config._threadBindingType) {
                pin_current_thread_to_socket(_numaNodeId);
            } else if (ThreadBindingType::CORES == _impl->_config._threadBindingType) {
                CpuSet processMask;
                int ncpus = 0;
                std::tie(processMask, ncpus) = get_process_mask();
                if (nullptr != processMask) {
                    pin_thread_to_vacant_core(_streamId + _impl->_config._threadBindingOffset,
                                              _impl->_config._threadBindingStep,
                                              ncpus,
                                              processMask);
                }
            }
#endif
        }
        ~Stream() {
            {
                std::lock_guard<std::mutex> lock{_impl->_streamIdMutex};
                _impl->_streamIdQueue.push(_streamId);
            }
#if OV_THREAD == OV_THREAD_TBB || OV_THREAD == OV_THREAD_TBB_AUTO
            if (nullptr != _observer) {
                _observer->observe(false);
            }
#endif
        }

#if OV_THREAD == OV_THREAD_TBB || OV_THREAD == OV_THREAD_TBB_AUTO
        void init_stream() {
            std::lock_guard<std::mutex> lock{_impl->_cpumap_mutex};
            const auto stream_id = _streamId >= _impl->_config._streams ? _impl->_config._streams - 1 : _streamId;
            const auto concurrency =
                (_impl->_config._streams_info_table.size() > 0 && _impl->_config._stream_ids.size() > 0)
                    ? _impl->_config._streams_info_table[_impl->_config._stream_ids[stream_id]][THREADS_PER_STREAM]
                    : 0;
            const auto cpu_core_type =
                (_impl->_config._streams_info_table.size() > 0 && _impl->_config._stream_ids.size() > 0)
                    ? static_cast<ColumnOfProcessorTypeTable>(
                          _impl->_config._streams_info_table[_impl->_config._stream_ids[stream_id]][PROC_TYPE])
                    : static_cast<ColumnOfProcessorTypeTable>(0);
            if (concurrency <= 0) {
                return;
            }
            if (_impl->_config._orig_proc_type_table[0][EFFICIENT_CORE_PROC] > 0) {
                const auto selected_core_type =
                    (cpu_core_type == MAIN_CORE_PROC || cpu_core_type == HYPER_THREADING_PROC)
                        ? custom::info::core_types().back()
                        : custom::info::core_types().front();
                if (_impl->_config._cpu_pinning) {
#    if defined(_WIN32) || defined(__APPLE__)
                    _taskArena.reset(new custom::task_arena{custom::task_arena::constraints{}
                                                                .set_core_type(selected_core_type)
                                                                .set_max_concurrency(concurrency)});
#    else
                    _taskArena.reset(new custom::task_arena{concurrency});
#    endif
                } else {
                    if (cpu_core_type == ALL_PROC) {
                        _taskArena.reset(new custom::task_arena{concurrency});
                    } else {
                        _taskArena.reset(new custom::task_arena{custom::task_arena::constraints{}
                                                                    .set_core_type(selected_core_type)
                                                                    .set_max_concurrency(concurrency)});
                    }
                }
            } else if (_impl->_config._proc_type_table.size() > 1 && !_impl->_config._cpu_pinning) {
                _taskArena.reset(new custom::task_arena{custom::task_arena::constraints{_numaNodeId, concurrency}});
            } else {
                _taskArena.reset(new custom::task_arena{concurrency});
            }
            if (_impl->_config._cpu_pinning) {
                _cpu_ids = static_cast<int>(_impl->_config._stream_core_ids.size()) == _impl->_config._streams
                               ? _impl->_config._stream_core_ids[stream_id]
                               : _cpu_ids;
                if (_cpu_ids.size() > 0) {
                    CpuSet processMask;
                    int ncpus = 0;
                    std::tie(processMask, ncpus) = get_process_mask();
                    if (nullptr != processMask) {
                        _observer.reset(new Observer{*_taskArena,
                                                     std::move(processMask),
                                                     ncpus,
                                                     0,
                                                     concurrency,
                                                     0,
                                                     0,
                                                     0,
                                                     _cpu_ids});
                        _observer->observe(true);
                    }
                }
            }
        }

        void init_stream_legacy() {
            const auto concurrency = (0 == _impl->_config._threadsPerStream) ? custom::task_arena::automatic
                                                                             : _impl->_config._threadsPerStream;
            if (ThreadBindingType::HYBRID_AWARE == _impl->_config._threadBindingType) {
                if (Config::PreferredCoreType::ROUND_ROBIN != _impl->_config._threadPreferredCoreType) {
                    if (Config::PreferredCoreType::ANY == _impl->_config._threadPreferredCoreType) {
                        _taskArena.reset(new custom::task_arena{concurrency});
                    } else {
                        const auto selected_core_type =
                            Config::PreferredCoreType::BIG == _impl->_config._threadPreferredCoreType
                                ? custom::info::core_types().back()    // running on Big cores only
                                : custom::info::core_types().front();  // running on Little cores only
                        _taskArena.reset(new custom::task_arena{custom::task_arena::constraints{}
                                                                    .set_core_type(selected_core_type)
                                                                    .set_max_concurrency(concurrency)});
                    }
                } else {
                    // assigning the stream to the core type in the round-robin fashion
                    // wrapping around total_streams (i.e. how many streams all different core types can handle
                    // together). Binding priority: Big core, Logical big core, Small core
                    const auto total_streams = _impl->total_streams_on_core_types.back().second;
                    const auto big_core_streams = _impl->total_streams_on_core_types.front().second;
                    const auto hybrid_core = _impl->total_streams_on_core_types.size() > 1;
                    const auto phy_core_streams =
                        _impl->_config._big_core_streams == 0
                            ? 0
                            : _impl->num_big_core_phys / _impl->_config._threads_per_stream_big;
                    const auto streamId_wrapped = _streamId % total_streams;
                    const auto& selected_core_type =
                        std::find_if(
                            _impl->total_streams_on_core_types.cbegin(),
                            _impl->total_streams_on_core_types.cend(),
                            [streamId_wrapped](const decltype(_impl->total_streams_on_core_types)::value_type& p) {
                                return p.second > streamId_wrapped;
                            })
                            ->first;
                    const auto small_core = hybrid_core && selected_core_type == 0;
                    const auto logic_core = !small_core && streamId_wrapped >= phy_core_streams;
                    const auto small_core_skip = small_core && _impl->_config._threads_per_stream_small == 3 &&
                                                 _impl->_config._small_core_streams > 1;
                    const auto max_concurrency =
                        small_core ? _impl->_config._threads_per_stream_small : _impl->_config._threads_per_stream_big;
                    // Special handling of _threads_per_stream_small == 3
                    const auto small_core_id = small_core_skip ? 0 : streamId_wrapped - big_core_streams;
                    const auto stream_id =
                        hybrid_core
                            ? (small_core ? small_core_id
                                          : (logic_core ? streamId_wrapped - phy_core_streams : streamId_wrapped))
                            : streamId_wrapped;
                    const auto thread_binding_step = hybrid_core ? (small_core ? _impl->_config._threadBindingStep : 2)
                                                                 : _impl->_config._threadBindingStep;
                    // Special handling of _threads_per_stream_small == 3, need to skip 4 (Four cores share one L2
                    // cache on the small core), stream_id = 0, cpu_idx_offset cumulative plus 4
                    const auto small_core_offset =
                        small_core_skip ? _impl->_config._small_core_offset + (streamId_wrapped - big_core_streams) * 4
                                        : _impl->_config._small_core_offset;
                    const auto cpu_idx_offset =
                        hybrid_core
                            // Prevent conflicts with system scheduling, so default cpu id on big core starts from 1
                            ? (small_core ? small_core_offset : (logic_core ? 0 : 1))
                            : 0;
#    ifdef _WIN32
                    _taskArena.reset(new custom::task_arena{custom::task_arena::constraints{}
                                                                .set_core_type(selected_core_type)
                                                                .set_max_concurrency(max_concurrency)});
#    else
                    _taskArena.reset(new custom::task_arena{max_concurrency});
#    endif
                    CpuSet processMask;
                    int ncpus = 0;
                    std::tie(processMask, ncpus) = get_process_mask();
                    if (nullptr != processMask) {
                        _observer.reset(new Observer{*_taskArena,
                                                     std::move(processMask),
                                                     ncpus,
                                                     stream_id,
                                                     max_concurrency,
                                                     thread_binding_step,
                                                     _impl->_config._threadBindingOffset,
                                                     cpu_idx_offset});
                        _observer->observe(true);
                    }
                }
            } else if (ThreadBindingType::NUMA == _impl->_config._threadBindingType) {
                _taskArena.reset(new custom::task_arena{custom::task_arena::constraints{_numaNodeId, concurrency}});
            } else if ((0 != _impl->_config._threadsPerStream) ||
                       (ThreadBindingType::CORES == _impl->_config._threadBindingType)) {
                _taskArena.reset(new custom::task_arena{concurrency});
                if (ThreadBindingType::CORES == _impl->_config._threadBindingType) {
                    CpuSet processMask;
                    int ncpus = 0;
                    std::tie(processMask, ncpus) = get_process_mask();
                    if (nullptr != processMask) {
                        _observer.reset(new Observer{*_taskArena,
                                                     std::move(processMask),
                                                     ncpus,
                                                     _streamId,
                                                     _impl->_config._threadsPerStream,
                                                     _impl->_config._threadBindingStep,
                                                     _impl->_config._threadBindingOffset});
                        _observer->observe(true);
                    }
                }
            }
        }
#endif

        Impl* _impl = nullptr;
        int _streamId = 0;
        int _numaNodeId = 0;
        bool _execute = false;
        std::queue<Task> _taskQueue;
#if OV_THREAD == OV_THREAD_TBB || OV_THREAD == OV_THREAD_TBB_AUTO
        std::unique_ptr<custom::task_arena> _taskArena;
        std::unique_ptr<Observer> _observer;
        std::vector<int> _cpu_ids;
#endif
    };

    explicit Impl(const Config& config)
        : _config{config},
          _streams([this] {
              return std::make_shared<Impl::Stream>(this);
          }) {
        _exectorMgr = executor_manager();
        auto numaNodes = get_available_numa_nodes();
        if (_config._streams != 0) {
            std::copy_n(std::begin(numaNodes),
                        std::min(static_cast<std::size_t>(_config._streams), numaNodes.size()),
                        std::back_inserter(_usedNumaNodes));
        } else {
            _usedNumaNodes = numaNodes;
        }
        _config._streams = _config._streams == 0 ? 1 : _config._streams;
#if (OV_THREAD == OV_THREAD_TBB || OV_THREAD == OV_THREAD_TBB_AUTO)
        if (!is_cpu_map_available() && ThreadBindingType::HYBRID_AWARE == config._threadBindingType) {
            const auto core_types = custom::info::core_types();
            const auto num_core_phys = get_number_of_cpu_cores();
            num_big_core_phys = get_number_of_cpu_cores(true);
            const auto num_small_core_phys = num_core_phys - num_big_core_phys;
            int sum = 0;
            // reversed order, so BIG cores are first
            for (auto iter = core_types.rbegin(); iter < core_types.rend(); iter++) {
                const auto& type = *iter;
                // calculating the #streams per core type
                const int num_streams_for_core_type =
                    type == 0 ? std::max(1,
                                         std::min(config._small_core_streams,
                                                  config._threads_per_stream_small == 0
                                                      ? 0
                                                      : num_small_core_phys / config._threads_per_stream_small))
                              : std::max(1,
                                         std::min(config._big_core_streams,
                                                  config._threads_per_stream_big == 0
                                                      ? 0
                                                      : num_big_core_phys / config._threads_per_stream_big * 2));
                sum += num_streams_for_core_type;
                // prefix sum, so the core type for a given stream id will be deduced just as a upper_bound
                // (notice that the map keeps the elements in the descending order, so the big cores are populated
                // first)
                total_streams_on_core_types.push_back({type, sum});
            }
        }
#endif
        for (auto streamId = 0; streamId < _config._streams; ++streamId) {
            _threads.emplace_back([this, streamId] {
                openvino::itt::threadName(_config._name + "_" + std::to_string(streamId));
                for (bool stopped = false; !stopped;) {
                    Task task;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        _queueCondVar.wait(lock, [&] {
                            return !_taskQueue.empty() || (stopped = _isStopped);
                        });
                        if (!_taskQueue.empty()) {
                            task = std::move(_taskQueue.front());
                            _taskQueue.pop();
                        }
                    }
                    if (task) {
                        Execute(task, *(_streams.local()));
                    }
                }
            });
        }
    }

    void Enqueue(Task task) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _taskQueue.emplace(std::move(task));
        }
        _queueCondVar.notify_one();
    }

    void Execute(const Task& task, Stream& stream) {
#if OV_THREAD == OV_THREAD_TBB || OV_THREAD == OV_THREAD_TBB_AUTO
        auto& arena = stream._taskArena;
        if (nullptr != arena) {
            arena->execute(std::move(task));
        } else {
            task();
        }
#else
        task();
#endif
    }

    void Defer(Task task) {
        auto& stream = *(_streams.local());
        stream._taskQueue.push(std::move(task));
        if (!stream._execute) {
            stream._execute = true;
            try {
                while (!stream._taskQueue.empty()) {
                    Execute(stream._taskQueue.front(), stream);
                    stream._taskQueue.pop();
                }
            } catch (...) {
            }
            stream._execute = false;
        }
    }

    Config _config;
    std::mutex _streamIdMutex;
    int _streamId = 0;
    std::queue<int> _streamIdQueue;
    std::vector<std::thread> _threads;
    std::mutex _mutex;
    std::mutex _cpumap_mutex;
    std::condition_variable _queueCondVar;
    std::queue<Task> _taskQueue;
    bool _isStopped = false;
    std::vector<int> _usedNumaNodes;
    ThreadLocal<std::shared_ptr<Stream>> _streams;
#if (OV_THREAD == OV_THREAD_TBB || OV_THREAD == OV_THREAD_TBB_AUTO)
    // stream id mapping to the core type
    // stored in the reversed order (so the big cores, with the highest core_type_id value, are populated first)
    // every entry is the core type and #streams that this AND ALL EARLIER entries can handle (prefix sum)
    // (so mapping is actually just an upper_bound: core type is deduced from the entry for which the id < #streams)
    using StreamIdToCoreTypes = std::vector<std::pair<custom::core_type_id, int>>;
    StreamIdToCoreTypes total_streams_on_core_types;
    int num_big_core_phys;
#endif
    std::shared_ptr<ExecutorManager> _exectorMgr;
};

int CPUStreamsExecutor::get_stream_id() {
    auto stream = _impl->_streams.local();
    return stream->_streamId;
}

int CPUStreamsExecutor::get_numa_node_id() {
    auto stream = _impl->_streams.local();
    return stream->_numaNodeId;
}

CPUStreamsExecutor::CPUStreamsExecutor(const IStreamsExecutor::Config& config) : _impl{new Impl{config}} {}

CPUStreamsExecutor::~CPUStreamsExecutor() {
    {
        std::lock_guard<std::mutex> lock(_impl->_mutex);
        _impl->_isStopped = true;
    }
    _impl->_queueCondVar.notify_all();
    for (auto& thread : _impl->_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void CPUStreamsExecutor::execute(Task task) {
    _impl->Defer(std::move(task));
}

void CPUStreamsExecutor::run(Task task) {
    if (0 == _impl->_config._streams) {
        _impl->Defer(std::move(task));
    } else {
        _impl->Enqueue(std::move(task));
    }
}

}  // namespace threading
}  // namespace ov

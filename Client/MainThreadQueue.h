// =============================================================================
//  RakHook's on_receive_rpc fires on RakNet's own network thread, not the
//  game's main thread. CStreaming/CModelInfo/CHandlingDataMgr are absolutely
//  not safe to touch from there - same reasoning already applied to the
//  deferred queue in BrownTurbo-CHandling. Every RakHook callback in this
//  plugin does the minimum work needed to copy/parse its payload, then
//  Push()es a closure here; StreamingHooks drains the queue once per frame
//  from plugin::Events::gameProcessEvent, which runs on the main thread.
// =============================================================================
#pragma once

#include <functional>
#include <mutex>
#include <vector>

class MainThreadQueue {
public:
	static MainThreadQueue& Instance()
	{
		static MainThreadQueue instance;
		return instance;
	}

	void Push(std::function<void()> task)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_pending.push_back(std::move(task));
	}

	// Call once per frame from the main thread only (StreamingHooks wires
	// this to plugin::Events::gameProcessEvent). Swaps the pending vector
	// out under the lock so tasks that Push() *more* work while running
	// (e.g. a load-failure handler queuing a retry) don't get executed
	// within the same drain and don't deadlock on m_mutex either.
	void DrainOnMainThread()
	{
		std::vector<std::function<void()>> toRun;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			toRun.swap(m_pending);
		}
		for (auto& task : toRun) {
			task();
		}
	}

	std::size_t PendingCount() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_pending.size();
	}

private:
	MainThreadQueue() = default;

	mutable std::mutex m_mutex;
	std::vector<std::function<void()>> m_pending;
};

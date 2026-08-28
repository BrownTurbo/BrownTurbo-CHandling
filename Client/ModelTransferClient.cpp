
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include "ModelTransferClient.h"
#include "MainThreadQueue.h"
#include "ModelCache.h"

#include <windows.h>
#include <RakNet/BitStream.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#include <zlib.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>

#include "defs.h"

std::string Sha256HexOfBuffer(const uint8_t* data, size_t length)
{
	BCRYPT_ALG_HANDLE hAlg = nullptr;
	BCRYPT_HASH_HANDLE hHash = nullptr;
	DWORD cbHash = 0, cbData = 0;
	std::string result;

	if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
		return result;
	if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&cbHash), sizeof(DWORD), &cbData, 0) < 0) {
		BCryptCloseAlgorithmProvider(hAlg, 0);
		return result;
	}
	std::vector<uint8_t> hash(cbHash);
	if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) >= 0) {
		BCryptHashData(hHash, const_cast<PUCHAR>(data), static_cast<ULONG>(length), 0);
		if (BCryptFinishHash(hHash, hash.data(), cbHash, 0) >= 0) {
			char hex[65] = {};
			for (size_t i = 0; i < hash.size(); ++i)
				std::snprintf(hex + i * 2, 3, "%02x", hash[i]);
			result.assign(hex);
		}
		BCryptDestroyHash(hHash);
	}
	BCryptCloseAlgorithmProvider(hAlg, 0);
	return result;
}

std::string Sha256HexOfFile(const fs::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
		return "";
	std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return Sha256HexOfBuffer(data.data(), data.size());
}

ModelTransferClient::ModelTransferClient()
{
	// nothing else here; worker started lazily during first RequestFile call.
}

ModelTransferClient::~ModelTransferClient()
{
	Shutdown();
}

void ModelTransferClient::Shutdown()
{
	if (m_workerStarted && !m_stopWorker) {
		m_stopWorker = true;
		if (m_worker.joinable())
			m_worker.join();
		m_workerStarted = false;
	}
}

void ModelTransferClient::ManualRetry(uint32_t modelId, ModelFileKind kind)
{
	const uint64_t key = Key(modelId, kind);
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_active.find(key);
		if (it == m_active.end())
			return; // no in-flight transfer to retry (can't re-request without expectedSha etc.)

		InFlight& entry = it->second;
		// Reset attempt/backoff bookkeeping
		entry.attempts = 1;
		entry.progress.attempts = 1;
		entry.backoffMs = kInitialBackoffMs;
		entry.nextRetryTime = std::chrono::steady_clock::time_point::min();
		entry.progress.lastError.clear();
		entry.progress.failed = false;
		entry.progress.statusText = "manual retry";

		// reset progress bytes so UI and worker compute timeouts from now
		entry.compressedBuffer.clear();
		entry.progress.receivedBytes = 0;
		entry.progress.receivedChunks = 0;
		entry.progress.startTime = std::chrono::steady_clock::now();
	}

	// Send immediate request on the main thread to avoid any RakNet threading issues.
	MainThreadQueue::Instance().Push([modelId, kind]() {
		RakNet::BitStream* bs;
		bs->Write(static_cast<uint8_t>(PKT_CHANDLING));
		bs->Write(static_cast<uint8_t>(30)); // ACTION_REQUEST_FILE_TRANSFER
		bs->Write(modelId);
		bs->Write(static_cast<uint8_t>(kind));
		rakhook::send(bs, HIGH_PRIORITY, RELIABLE_ORDERED, kRequestChannel);
	});
}

void ModelTransferClient::EnsureWorkerStarted()
{
	bool expected = false;
	if (m_workerStarted.compare_exchange_strong(expected, true)) {
		m_stopWorker = false;
		m_worker = std::thread([this] {
			WorkerMain();
		});
	}
}

void ModelTransferClient::RequestFile(uint32_t modelId, ModelFileKind kind, const std::string& expectedSha256Hex,
	std::function<void(bool, const fs::path&)> onReady)
{
	if (auto cached = ModelCache::Instance().TryGet(modelId, static_cast<uint8_t>(kind), expectedSha256Hex)) {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			TransferProgress p;
			p.modelId = modelId;
			p.kind = kind;
			p.fromCache = true;
			p.statusText = "cached";
			p.attempts = 0;
			p.lastError.clear();
			m_active[Key(modelId, kind)].progress = p;
		}
		onReady(true, *cached);
		return;
	}

	EnsureWorkerStarted();

	InFlight entry;
	entry.progress.modelId = modelId;
	entry.progress.kind = kind;
	entry.progress.startTime = std::chrono::steady_clock::now();
	entry.progress.statusText = "requesting";
	entry.progress.attempts = 1;
	entry.attempts = 1;
	entry.backoffMs = kInitialBackoffMs;
	entry.expectedSha256 = expectedSha256Hex;
	entry.onReady = std::move(onReady);

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_active[Key(modelId, kind)] = std::move(entry);
	}

	// Build RakNet packet: ID_CHANDLING (251) + ACTION_REQUEST_FILE_TRANSFER (30) + modelId + kind
	// We enqueue the send on main thread to be safe: main-thread send avoids any RakNet thread-safety issues.
	MainThreadQueue::Instance().Push([modelId, kind]() {
		RakNet::BitStream* bs;
		bs->Write(static_cast<uint8_t>(PKT_CHANDLING));
		bs->Write(static_cast<uint8_t>(30)); // ACTION_REQUEST_FILE_TRANSFER
		bs->Write(modelId);
		bs->Write(static_cast<uint8_t>(kind));
		rakhook::send(bs, HIGH_PRIORITY, RELIABLE_ORDERED, kRequestChannel);
	});
}

std::vector<TransferProgress> ModelTransferClient::Snapshot() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	std::vector<TransferProgress> result;
	result.reserve(m_active.size());
	for (auto& [key, entry] : m_active)
		result.push_back(entry.progress);
	return result;
}

void ModelTransferClient::FailImmediately(std::unordered_map<uint64_t, InFlight>::iterator it, const std::string& err)
{
	it->second.progress.failed = true;
	it->second.progress.lastError = err;
	it->second.progress.statusText = "failed";
	auto onReady = it->second.onReady;
	uint64_t key = it->first;

	MainThreadQueue::Instance().Push([this, key, onReady] {
		if (onReady)
			onReady(false, {});
		std::lock_guard<std::mutex> lock(m_mutex);
		m_active.erase(key);
	});
}

void ModelTransferClient::OnTransferBegin(RakNet::BitStream* bs)
{
	uint32_t modelId;
	uint8_t kindByte;
	uint32_t compressedSize, uncompressedSize, totalChunks;
	char shaBuf[65] = {};

	if (!bs->Read(modelId) || !bs->Read(kindByte) || !bs->Read(compressedSize) || !bs->Read(uncompressedSize) || !bs->Read(totalChunks) || !bs->Read(shaBuf, 65))
		return;

	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_active.find(Key(modelId, static_cast<ModelFileKind>(kindByte)));
	if (it == m_active.end())
		return;

	it->second.progress.compressedSize = compressedSize;
	it->second.progress.uncompressedSize = uncompressedSize;
	it->second.progress.totalChunks = totalChunks;
	it->second.progress.statusText = "downloading";
	it->second.compressedBuffer.resize(compressedSize);
	it->second.expectedSha256 = std::string(shaBuf);

	// refresh start time so timeout counts from begin arrival
	it->second.progress.startTime = std::chrono::steady_clock::now();
	it->second.nextRetryTime = std::chrono::steady_clock::time_point::min();
	it->second.backoffMs = kInitialBackoffMs;
}

void ModelTransferClient::OnTransferChunk(RakNet::BitStream* bs)
{
	uint32_t modelId;
	uint8_t kindByte;
	uint32_t chunkIndex;
	uint16_t chunkLen;

	if (!bs->Read(modelId) || !bs->Read(kindByte) || !bs->Read(chunkIndex) || !bs->Read(chunkLen))
		return;

	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_active.find(Key(modelId, static_cast<ModelFileKind>(kindByte)));
	if (it == m_active.end()) {
		bs->IgnoreBits(chunkLen * 8);
		return;
	}

	auto& entry = it->second;
	const uint32_t offset = chunkIndex * 4096u; // kFileChunkSize, matches server
	if (offset + chunkLen > entry.compressedBuffer.size())
		return;

	if (!bs->Read(reinterpret_cast<char*>(entry.compressedBuffer.data() + offset), chunkLen))
		return;

	entry.progress.receivedChunks++;
	entry.progress.receivedBytes += chunkLen;

	// refresh startTime so the worker's timeout is relative to last activity
	entry.progress.startTime = std::chrono::steady_clock::now();
	entry.nextRetryTime = std::chrono::steady_clock::time_point::min();
}

void ModelTransferClient::OnTransferEnd(RakNet::BitStream* bs)
{
	uint32_t modelId;
	uint8_t kindByte;
	if (!bs->Read(modelId) || !bs->Read(kindByte))
		return;

	const uint64_t key = Key(modelId, static_cast<ModelFileKind>(kindByte));

	std::vector<uint8_t> compressed;
	uint32_t uncompressedSize = 0;
	std::string expectedSha;
	std::function<void(bool, const fs::path&)> onReady;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_active.find(key);
		if (it == m_active.end())
			return;

		compressed = std::move(it->second.compressedBuffer);
		uncompressedSize = it->second.progress.uncompressedSize;
		expectedSha = it->second.expectedSha256;
		onReady = it->second.onReady;
		// keep the entry in map - we'll either finish it or schedule retry
	}

	std::vector<uint8_t> decompressed(uncompressedSize);
	uLongf destLen = uncompressedSize;
	bool ok = true;
	if (uncompressedSize == 0 && compressed.empty())
		ok = false; // defensive
	else
		ok = (uncompress(decompressed.data(), &destLen, compressed.data(),
				  static_cast<uLong>(compressed.size()))
				== Z_OK
			&& destLen == uncompressedSize);

	if (ok)
		ok = (Sha256HexOfBuffer(decompressed.data(), decompressed.size()) == expectedSha);

	if (ok) {
		// store and finish
		bool stored = ModelCache::Instance().Store(modelId, static_cast<uint8_t>(kindByte), decompressed);
		fs::path finalPath = stored ? ModelCache::Instance().PathFor(modelId, static_cast<uint8_t>(kindByte)) : fs::path {};
		MainThreadQueue::Instance().Push([this, key, finalPath, onReady] {
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_active.erase(key);
			}
			if (onReady)
				onReady(true, finalPath);
		});
		return;
	}

	// Not OK -> schedule retry if allowed
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_active.find(key);
		if (it == m_active.end())
			return;
		ScheduleRetry(it, "hash/inflate mismatch");
	}
}

void ModelTransferClient::OnTransferCancel(RakNet::BitStream* bs)
{
	uint32_t modelId;
	uint8_t kindByte;
	if (!bs->Read(modelId) || !bs->Read(kindByte))
		return;

	const uint64_t key = Key(modelId, static_cast<ModelFileKind>(kindByte));
	std::function<void(bool, const fs::path&)> onReady;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_active.find(key);
		if (it == m_active.end())
			return;

		// server explicitly canceled - schedule retry if any attempts left
		// ScheduleRetry(it, "server canceled / file missing");
		FailImmediately(it, "server: file not found");
	}
}

void ModelTransferClient::ScheduleRetry(std::unordered_map<uint64_t, InFlight>::iterator it, const std::string& err)
{
	// called under lock
	InFlight& entry = it->second;
	entry.attempts++;
	entry.progress.attempts = entry.attempts;
	entry.progress.lastError = err;
	entry.progress.statusText = (entry.attempts <= kMaxAttempts)
		? ("retrying (" + std::to_string(entry.attempts) + "/" + std::to_string(kMaxAttempts) + ")")
		: "failed";

	if (entry.attempts > kMaxAttempts) {
		// permanent failure - call onReady(false) on main thread and erase entry
		auto onReady = entry.onReady;
		uint64_t key = it->first;
		MainThreadQueue::Instance().Push([this, key, onReady] {
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				auto it2 = m_active.find(key);
				if (it2 != m_active.end()) {
					it2->second.progress.failed = true;
					// keep diagnostic text in progress for snapshot / UI
				}
			}
			if (onReady)
				onReady(false, {});
			std::lock_guard<std::mutex> lock(m_mutex);
			m_active.erase(key);
		});
		return;
	}

	// schedule next retry with exponential backoff
	uint32_t useBackoff = entry.backoffMs > 0 ? entry.backoffMs : kInitialBackoffMs;
	entry.nextRetryTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(useBackoff);
	entry.backoffMs = std::min(static_cast<uint32_t>(entry.backoffMs ? entry.backoffMs * 2 : kInitialBackoffMs * 2), kMaxBackoffMs);

	// reset receive buffer/positions for next attempt
	entry.compressedBuffer.clear();
	entry.progress.receivedBytes = 0;
	entry.progress.receivedChunks = 0;
	entry.progress.startTime = std::chrono::steady_clock::now();
}

void ModelTransferClient::WorkerMain()
{
	while (!m_stopWorker) {
		auto now = std::chrono::steady_clock::now();
		std::vector<std::pair<uint64_t, std::function<void()>>> sendTasks; // key + send lambda
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			for (auto it = m_active.begin(); it != m_active.end(); ++it) {
				InFlight& entry = it->second;
				// 1) scheduled retry time hit?
				if (entry.nextRetryTime != std::chrono::steady_clock::time_point::min()
					&& entry.nextRetryTime <= now) {
					// prepare a send task (run on main thread)
					uint32_t modelId = entry.progress.modelId;
					ModelFileKind kind = entry.progress.kind;
					sendTasks.emplace_back(it->first, [modelId, kind]() {
						RakNet::BitStream* bs;
						bs->Write(static_cast<uint8_t>(PKT_CHANDLING)); // ID_CHANDLING
						bs->Write(static_cast<uint8_t>(30)); // ACTION_REQUEST_FILE_TRANSFER
						bs->Write(modelId);
						bs->Write(static_cast<uint8_t>(kind));
						rakhook::send(bs, HIGH_PRIORITY, RELIABLE_ORDERED, kRequestChannel);
					});

					// update state
					entry.progress.statusText = "requesting";
					entry.progress.startTime = std::chrono::steady_clock::now();
					entry.nextRetryTime = std::chrono::steady_clock::time_point::min();
					continue;
				}

				// 2) no activity timeout -> trigger a retry attempt proactively
				if (!entry.progress.fromCache && (entry.progress.statusText == "requesting" || entry.progress.statusText == "downloading")) {
					auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - entry.progress.startTime).count();
					if (static_cast<uint32_t>(elapsed) > kResponseTimeoutMs) {
						// schedule retry now (no error string available because we timed out)
						sendTasks.emplace_back(it->first, [modelId = entry.progress.modelId, kind = entry.progress.kind]() {
							RakNet::BitStream* bs;
							bs->Write(static_cast<uint8_t>(PKT_CHANDLING)); // ID_CHANDLING
							bs->Write(static_cast<uint8_t>(30)); // ACTION_REQUEST_FILE_TRANSFER
							bs->Write(modelId);
							bs->Write(static_cast<uint8_t>(kind));
							rakhook::send(bs, HIGH_PRIORITY, RELIABLE_ORDERED, kRequestChannel);
						});

						// update internally to show retry scheduled
						entry.attempts++;
						entry.progress.attempts = entry.attempts;
						entry.progress.lastError = "timeout";
						entry.progress.statusText = "retrying (timeout)";
						entry.backoffMs = std::min(entry.backoffMs ? entry.backoffMs * 2 : kInitialBackoffMs, kMaxBackoffMs);
						entry.nextRetryTime = now + std::chrono::milliseconds(entry.backoffMs);
						entry.compressedBuffer.clear();
						entry.progress.receivedBytes = 0;
						entry.progress.receivedChunks = 0;
						entry.progress.startTime = std::chrono::steady_clock::now();
					}
				}
			}
		}

		// dispatch sends on main thread outside of lock
		for (auto& p : sendTasks) {
			MainThreadQueue::Instance().Push(p.second);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(kWorkerSleepMs));
	}
}

void ModelTransferClient::FinishTransfer(uint64_t key, bool success, const std::string& err)
{
	// not used directly in these changes but kept for API parity - finishing is done inline in handlers
	if (!success) {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_active.find(key);
		if (it != m_active.end()) {
			ScheduleRetry(it, err);
		}
	}
}

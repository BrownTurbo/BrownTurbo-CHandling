#pragma once

#include <windows.h>
#include <bcrypt.h>
#include <wininet.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "bcrypt.lib")

// Represents the precise outcome of an asset acquisition operation
enum class DownloadStatus {
	Success_Downloaded,
	Success_Cached,
	Err_WinInetInitFailed,
	Err_UrlOpenFailed,
	Err_HttpError,
	Err_HashMismatch,
	Err_DiskWriteFailed,
	Err_DiskReadFailed,
	Err_Unknown
};

struct DownloadResult {
	DownloadStatus status { DownloadStatus::Err_Unknown };
	std::vector<uint8_t> data {};
	std::string calculatedHash {};
	std::string expectedSha256 {};
	std::string errorMessage {};
	uint32_t httpStatusCode { 0 };

	bool Success() const
	{
		return status == DownloadStatus::Success_Downloaded || status == DownloadStatus::Success_Cached;
	}
};

struct AssetDownloadTask {
	int32_t modelId { -1 };
	std::string url {};
	std::string expectedSha256 {};
	std::filesystem::path localCachePath {};
	std::function<void(DownloadResult)> onComplete { nullptr };
	std::function<void(size_t downloadedBytes, size_t totalBytes)> onProgress { nullptr };
};

class CryptoUtility {
public:
	static std::string BytesToHex(const uint8_t* buffer, size_t length)
	{
		std::ostringstream ss;
		ss << std::hex << std::setfill('0');
		for (size_t i = 0; i < length; ++i) {
			ss << std::setw(2) << static_cast<int>(buffer[i]);
		}
		return ss.str();
	}

	static bool ComputeFileSHA256(const std::filesystem::path& filePath, std::string& outHashStr)
	{
		std::ifstream file(filePath, std::ios::binary);
		if (!file.is_open())
			return false;

		BCRYPT_ALG_HANDLE hAlg = NULL;
		BCRYPT_HASH_HANDLE hHash = NULL;
		NTSTATUS status = 0;
		DWORD cbHash = 0, cbData = 0;

		status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
		if (status != 0)
			return false;

		status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&cbHash, sizeof(DWORD), &cbData, 0);
		if (status != 0) {
			BCryptCloseAlgorithmProvider(hAlg, 0);
			return false;
		}

		std::vector<uint8_t> hashBuffer(cbHash);

		status = BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0);
		if (status != 0) {
			BCryptCloseAlgorithmProvider(hAlg, 0);
			return false;
		}

		constexpr size_t CHUNK_SIZE = 64 * 1024;
		std::vector<char> buffer(CHUNK_SIZE);

		while (file.read(buffer.data(), CHUNK_SIZE) || file.gcount() > 0) {
			std::streamsize bytesRead = file.gcount();
			status = BCryptHashData(hHash, (PUCHAR)buffer.data(), static_cast<ULONG>(bytesRead), 0);
			if (status != 0) {
				BCryptDestroyHash(hHash);
				BCryptCloseAlgorithmProvider(hAlg, 0);
				return false;
			}
		}

		status = BCryptFinishHash(hHash, hashBuffer.data(), cbHash, 0);
		BCryptDestroyHash(hHash);
		BCryptCloseAlgorithmProvider(hAlg, 0);

		if (status != 0)
			return false;

		outHashStr = BytesToHex(hashBuffer.data(), hashBuffer.size());
		return true;
	}
};

class AssetDownloader {
private:
	static constexpr size_t MAX_DOWNLOAD_BYTES = 128u * 1024u * 1024u;
	std::vector<std::thread> m_workers;
	std::queue<AssetDownloadTask> m_taskQueue;
	std::mutex m_queueMutex;
	std::condition_variable m_cv;
	std::atomic<bool> m_stopPool { false };

	void WorkerLoop()
	{
		while (!m_stopPool) {
			AssetDownloadTask task;
			{
				std::unique_lock<std::mutex> lock(m_queueMutex);
				m_cv.wait(lock, [this] {
					return m_stopPool || !m_taskQueue.empty();
				});

				if (m_stopPool && m_taskQueue.empty())
					return;

				task = std::move(m_taskQueue.front());
				m_taskQueue.pop();
			}

			ExecuteTask(task);
		}
	}

	void ExecuteTask(const AssetDownloadTask& task)
	{
		DownloadResult result;
		result.expectedSha256 = task.expectedSha256;

		// 1. LOCAL DISK CACHE CHECK
		if (!task.localCachePath.empty() && std::filesystem::exists(task.localCachePath)) {
			std::string localHash;
			if (CryptoUtility::ComputeFileSHA256(task.localCachePath, localHash)) {
				if (_stricmp(localHash.c_str(), task.expectedSha256.c_str()) == 0) {
					// Valid local file found. Read into buffer and return cached success.
					std::ifstream file(task.localCachePath, std::ios::binary | std::ios::ate);
					if (file.is_open()) {
						std::streamsize fileSize = file.tellg();
						file.seekg(0, std::ios::beg);
						result.data.resize(static_cast<size_t>(fileSize));

						if (file.read(reinterpret_cast<char*>(result.data.data()), fileSize)) {
							result.status = DownloadStatus::Success_Cached;
							result.calculatedHash = localHash;
							if (task.onComplete)
								task.onComplete(result);
							return;
						}
					}
				}
			}
		}

		// 2. NETWORK DOWNLOAD SETUP (WinINet)
		if (task.url.rfind("https://", 0) != 0) {
			result.status = DownloadStatus::Err_UrlOpenFailed;
			result.errorMessage = "Only HTTPS asset URLs are allowed.";
			if (task.onComplete)
				task.onComplete(result);
			return;
		}

		HINTERNET hInternet = InternetOpenA("GTA_SA_ASI_AssetPipeline/2.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
		if (!hInternet) {
			result.status = DownloadStatus::Err_WinInetInitFailed;
			result.errorMessage = "Failed to initialize WinINet session.";
			if (task.onComplete)
				task.onComplete(result);
			return;
		}

		// Configure timeouts (15 seconds per stage)
		DWORD timeoutMs = 15000;
		InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
		InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
		InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

		DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE;
		if (task.url.rfind("https://", 0) == 0) {
			flags |= INTERNET_FLAG_SECURE;
		}

		HINTERNET hUrl = InternetOpenUrlA(hInternet, task.url.c_str(), NULL, 0, flags, 0);
		if (!hUrl) {
			DWORD err = GetLastError();
			InternetCloseHandle(hInternet);
			result.status = DownloadStatus::Err_UrlOpenFailed;
			result.errorMessage = "InternetOpenUrlA failed with Win32 Error: " + std::to_string(err);
			if (task.onComplete)
				task.onComplete(result);
			return;
		}

		// 3. HTTP STATUS & CONTENT LENGTH INSPECTION
		DWORD statusCode = 0;
		DWORD statusCodeLen = sizeof(statusCode);
		if (HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeLen, NULL)) {
			result.httpStatusCode = statusCode;
			if (statusCode != 200) {
				InternetCloseHandle(hUrl);
				InternetCloseHandle(hInternet);
				result.status = DownloadStatus::Err_HttpError;
				result.errorMessage = "HTTP Server returned failure status code: " + std::to_string(statusCode);
				if (task.onComplete)
					task.onComplete(result);
				return;
			}
		}

		DWORD contentLength = 0;
		DWORD contentLengthLen = sizeof(contentLength);
		bool hasContentLength = HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentLength, &contentLengthLen, NULL);
		if (hasContentLength && contentLength > MAX_DOWNLOAD_BYTES) {
			InternetCloseHandle(hUrl);
			InternetCloseHandle(hInternet);
			result.status = DownloadStatus::Err_HttpError;
			result.errorMessage = "Asset exceeds the maximum download size.";
			if (task.onComplete)
				task.onComplete(result);
			return;
		}

		// 4. PREPARE BCRYPT STREAMING HASH
		BCRYPT_ALG_HANDLE hAlg = NULL;
		BCRYPT_HASH_HANDLE hHash = NULL;
		bool streamingHashReady = false;

		if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0) {
			if (BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0) == 0) {
				streamingHashReady = true;
			}
		}

		// 5. CHUNKED DOWNLOAD LOOP
		constexpr DWORD CHUNK_SIZE = 16384;
		std::vector<uint8_t> chunkBuffer(CHUNK_SIZE);
		DWORD bytesRead = 0;
		size_t totalBytesDownloaded = 0;

		while (InternetReadFile(hUrl, chunkBuffer.data(), CHUNK_SIZE, &bytesRead) && bytesRead > 0) {
			if (totalBytesDownloaded > MAX_DOWNLOAD_BYTES - bytesRead) {
				result.status = DownloadStatus::Err_HttpError;
				result.errorMessage = "Asset exceeds the maximum download size.";
				break;
			}
			result.data.insert(result.data.end(), chunkBuffer.begin(), chunkBuffer.begin() + bytesRead);
			totalBytesDownloaded += bytesRead;

			if (streamingHashReady) {
				BCryptHashData(hHash, chunkBuffer.data(), bytesRead, 0);
			}

			if (task.onProgress) {
				task.onProgress(totalBytesDownloaded, hasContentLength ? static_cast<size_t>(contentLength) : 0);
			}
		}

		InternetCloseHandle(hUrl);
		InternetCloseHandle(hInternet);

		if (result.status == DownloadStatus::Err_HttpError) {
			if (task.onComplete)
				task.onComplete(result);
			return;
		}

		// 6. FINALIZE HASH CALCULATION
		if (streamingHashReady) {
			DWORD cbHash = 32;
			std::vector<uint8_t> hashBuffer(cbHash);
			if (BCryptFinishHash(hHash, hashBuffer.data(), cbHash, 0) == 0) {
				result.calculatedHash = CryptoUtility::BytesToHex(hashBuffer.data(), hashBuffer.size());
			}
			BCryptDestroyHash(hHash);
			BCryptCloseAlgorithmProvider(hAlg, 0);
		}

		// 7. HASH VALIDATION
		if (_stricmp(result.calculatedHash.c_str(), task.expectedSha256.c_str()) != 0) {
			result.status = DownloadStatus::Err_HashMismatch;
			result.errorMessage = "Downloaded file hash (" + result.calculatedHash + ") does not match expected hash (" + task.expectedSha256 + ").";
			if (task.onComplete)
				task.onComplete(result);
			return;
		}

		// 8. SAVE TO LOCAL DISK CACHE
		if (!task.localCachePath.empty()) {
			try {
				if (task.localCachePath.has_parent_path()) {
					std::filesystem::create_directories(task.localCachePath.parent_path());
				}
				std::ofstream outFile(task.localCachePath, std::ios::binary | std::ios::trunc);
				if (outFile.is_open()) {
					outFile.write(reinterpret_cast<const char*>(result.data.data()), result.data.size());
					outFile.close();
					result.status = DownloadStatus::Success_Downloaded;
				} else {
					result.status = DownloadStatus::Err_DiskWriteFailed;
					result.errorMessage = "Failed to open cache path for writing: " + task.localCachePath.string();
				}
			} catch (const std::exception& ex) {
				result.status = DownloadStatus::Err_DiskWriteFailed;
				result.errorMessage = std::string("Disk Exception: ") + ex.what();
			}
		} else {
			result.status = DownloadStatus::Success_Downloaded;
		}

		if (task.onComplete)
			task.onComplete(result);
	}

public:
	AssetDownloader(size_t threadCount = 4)
	{
		for (size_t i = 0; i < threadCount; ++i) {
			m_workers.emplace_back(&AssetDownloader::WorkerLoop, this);
		}
	}

	~AssetDownloader()
	{
		m_stopPool = true;
		m_cv.notify_all();
		for (std::thread& worker : m_workers) {
			if (worker.joinable()) {
				worker.join();
			}
		}
	}

	void Enqueue(const AssetDownloadTask& task)
	{
		{
			std::unique_lock<std::mutex> lock(m_queueMutex);
			m_taskQueue.push(task);
		}
		m_cv.notify_one();
	}

	void Shutdown()
	{
		m_stopPool = true;
		m_cv.notify_all();
		for (std::thread& worker : m_workers) {
			if (worker.joinable()) {
				worker.join();
			}
		}
	}
};

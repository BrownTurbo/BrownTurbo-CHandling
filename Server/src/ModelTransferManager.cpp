#include "ModelTransferManager.h"
#include "extendedveh.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include <bcrypt.h>
#include <zlib.h>
#pragma comment(lib, "bcrypt.lib")

namespace fs = std::filesystem;

namespace ModelTransferMgr {
namespace {
	std::string g_modelsDir = "models";

	struct CachedFile {
		std::vector<uint8_t> compressed;
		uint32_t uncompressedSize = 0;
		std::string
			sha256Hex; // hex-encoded, matches CustomVehicleDef's ...Hash fields
		bool valid = false;
	};

	// Keyed by (modelId << 2) | kind
	std::unordered_map<uint64_t, CachedFile> g_cache;

	struct ActiveTransfer {
		int playerId = -1;
		uint32_t modelId = 0;
		ModelFileKind kind {};
		uint32_t nextChunkIndex = 0;
		uint32_t totalChunks = 0;
	};

	std::deque<ActiveTransfer> g_activeTransfers;
	constexpr size_t kMaxActiveTransfersPerPlayer = 8;
	constexpr uint32_t kMaxModelFileSize = 128u * 1024u * 1024u;

	uint64_t CacheKey(uint32_t modelId, ModelFileKind kind)
	{
		return (static_cast<uint64_t>(modelId) << 2) | static_cast<uint64_t>(kind);
	}

	const char* FileExtensionFor(ModelFileKind kind)
	{
		switch (kind) {
		case ModelFileKind::Dff:
			return ".dff";
		case ModelFileKind::Txd:
			return ".txd";
		case ModelFileKind::Col:
			return ".col";
		}
		return "";
	}

	std::string Sha256Hex(const uint8_t* data, size_t length)
	{
		BCRYPT_ALG_HANDLE hAlg = nullptr;
		BCRYPT_HASH_HANDLE hHash = nullptr;
		DWORD cbHash = 0, cbData = 0;
		std::string result;

		if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
			return result;
		if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH,
				reinterpret_cast<PUCHAR>(&cbHash), sizeof(DWORD),
				&cbData, 0)
			< 0) {
			BCryptCloseAlgorithmProvider(hAlg, 0);
			return result;
		}
		std::vector<uint8_t> hash(cbHash);
		if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) >= 0) {
			BCryptHashData(hHash, const_cast<PUCHAR>(data), static_cast<ULONG>(length),
				0);
			if (BCryptFinishHash(hHash, hash.data(), cbHash, 0) >= 0) {
				char hex[2 * 32 + 1] = {};
				for (size_t i = 0; i < hash.size(); ++i)
					std::snprintf(hex + i * 2, 3, "%02x", hash[i]);
				result.assign(hex);
			}
			BCryptDestroyHash(hHash);
		}
		BCryptCloseAlgorithmProvider(hAlg, 0);
		return result;
	}

	// Loads + zlib-compresses a file on first request, caches the result.
	// Returns nullptr if the file doesn't exist or compression failed.
	const CachedFile* GetOrLoadCache(uint32_t modelId, ModelFileKind kind)
	{
		const uint64_t key = CacheKey(modelId, kind);
		auto it = g_cache.find(key);
		if (it != g_cache.end() && it->second.valid)
			return &it->second;

		fs::path path = fs::path(g_modelsDir) / (std::to_string(modelId) + FileExtensionFor(kind));
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file.is_open())
			return nullptr;

		std::streamsize size = file.tellg();
		if (size < 0)
			return nullptr;
		file.seekg(0, std::ios::beg);
		std::vector<uint8_t> raw(static_cast<size_t>(size));
		if (size > 0 && !file.read(reinterpret_cast<char*>(raw.data()), size))
			return nullptr;

		CachedFile entry;
		entry.uncompressedSize = static_cast<uint32_t>(raw.size());
		entry.sha256Hex = Sha256Hex(raw.data(), raw.size());

		uLongf compressedBound = compressBound(static_cast<uLong>(raw.size()));
		entry.compressed.resize(compressedBound);
		uLongf compressedSize = compressedBound;
		if (compress2(entry.compressed.data(), &compressedSize, raw.data(),
				static_cast<uLong>(raw.size()), Z_BEST_COMPRESSION)
			!= Z_OK) {
			return nullptr;
		}
		entry.compressed.resize(compressedSize);
		entry.valid = true;

		auto [insertedIt, _] = g_cache.insert_or_assign(key, std::move(entry));
		return &insertedIt->second;
	}
} // namespace

void Initialize(const std::string& modelsDirectory)
{
	g_modelsDir = modelsDirectory;
	fs::create_directories(g_modelsDir);
}

void InvalidateCache(uint32_t modelId, ModelFileKind kind)
{
	g_cache.erase(CacheKey(modelId, kind));
}

void OnRequestFile(IPlayer& player, uint32_t modelId, ModelFileKind kind)
{
	if (kind != ModelFileKind::Dff && kind != ModelFileKind::Txd && kind != ModelFileKind::Col)
		return;

	const int playerId = player.getID();
	if (std::any_of(g_activeTransfers.begin(), g_activeTransfers.end(),
			[&](const ActiveTransfer& transfer) {
				return transfer.playerId == playerId && transfer.modelId == modelId && transfer.kind == kind;
			}))
		return;

	const size_t playerTransferCount = static_cast<size_t>(std::count_if(
		g_activeTransfers.begin(), g_activeTransfers.end(),
		[&](const ActiveTransfer& transfer) {
			return transfer.playerId == playerId;
		}));
	if (playerTransferCount >= kMaxActiveTransfersPerPlayer)
		return;

	const CachedFile* cached = GetOrLoadCache(modelId, kind);
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core = compo->getCore();

	if (!cached) {
		CHandlingActionPacket cancel(ACTION_FILE_TRANSFER_CANCEL);
		cancel.data.Write(modelId);
		cancel.data.Write(static_cast<uint8_t>(kind));
		player.sendPacket(
			Span<uint8_t>(cancel.data.GetData(), cancel.data.GetNumberOfBytesUsed()),
			kFileTransferChannel, true);
		if (core)
			core->logLn(LogLevel::Warning,
				"[ModelTransfer] player %d requested modelId %u kind %u - "
				"file missing/unreadable.",
				player.getID(), modelId, static_cast<unsigned>(kind));
		return;
	}

	if (cached->uncompressedSize == 0 || cached->uncompressedSize > kMaxModelFileSize || cached->compressed.empty())
		return;

	const uint32_t totalChunks = (static_cast<uint32_t>(cached->compressed.size()) + kFileChunkSize - 1) / kFileChunkSize;

	CHandlingActionPacket begin(ACTION_FILE_TRANSFER_BEGIN);
	begin.data.Write(modelId);
	begin.data.Write(static_cast<uint8_t>(kind));
	begin.data.Write(static_cast<uint32_t>(cached->compressed.size()));
	begin.data.Write(cached->uncompressedSize);
	begin.data.Write(totalChunks);
	begin.data.Write(cached->sha256Hex.c_str(),
		static_cast<int>(cached->sha256Hex.size()) + 1); // NUL-terminated
	player.sendPacket(
		Span<uint8_t>(begin.data.GetData(), begin.data.GetNumberOfBytesUsed()),
		kFileTransferChannel, true);

	ActiveTransfer transfer;
	transfer.playerId = player.getID();
	transfer.modelId = modelId;
	transfer.kind = kind;
	transfer.totalChunks = totalChunks;
	g_activeTransfers.push_back(transfer);
}

void CancelTransfer(IPlayer& player, uint32_t modelId, ModelFileKind kind)
{
	const int playerId = player.getID();
	g_activeTransfers.erase(
		std::remove_if(g_activeTransfers.begin(), g_activeTransfers.end(),
			[&](const ActiveTransfer& t) {
				return t.playerId == playerId && t.modelId == modelId && t.kind == kind;
			}),
		g_activeTransfers.end());
}

void OnPlayerDisconnect(IPlayer& player)
{
	const int playerId = player.getID();
	g_activeTransfers.erase(std::remove_if(g_activeTransfers.begin(),
								g_activeTransfers.end(),
								[&](const ActiveTransfer& t) {
									return t.playerId == playerId;
								}),
		g_activeTransfers.end());
}

void ProcessTick()
{
	if (g_activeTransfers.empty())
		return;

	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core = compo->getCore();
	if (!core)
		return;

	const size_t count = g_activeTransfers.size();
	for (size_t i = 0; i < count; ++i) {
		ActiveTransfer transfer = g_activeTransfers.front();
		g_activeTransfers.pop_front();

		IPlayer* player = compo->GetPlayerByID(transfer.playerId);
		if (!player)
			continue;

		const CachedFile* cached = GetOrLoadCache(transfer.modelId, transfer.kind);
		if (!cached)
			continue;

		uint32_t sentThisTick = 0;
		while (sentThisTick < kChunksPerPlayerPerTick && transfer.nextChunkIndex < transfer.totalChunks) {
			const uint32_t offset = transfer.nextChunkIndex * kFileChunkSize;
			const uint32_t remaining = static_cast<uint32_t>(cached->compressed.size()) - offset;
			const uint16_t chunkLen = static_cast<uint16_t>(std::min<uint32_t>(remaining, kFileChunkSize));

			CHandlingActionPacket chunkPkt(ACTION_FILE_TRANSFER_CHUNK);
			chunkPkt.data.Write(transfer.modelId);
			chunkPkt.data.Write(static_cast<uint8_t>(transfer.kind));
			chunkPkt.data.Write(transfer.nextChunkIndex);
			chunkPkt.data.Write(chunkLen);
			chunkPkt.data.Write(
				reinterpret_cast<const char*>(cached->compressed.data() + offset),
				chunkLen);
			player->sendPacket(Span<uint8_t>(chunkPkt.data.GetData(),
								   chunkPkt.data.GetNumberOfBytesUsed()),
				kFileTransferChannel, true);

			++transfer.nextChunkIndex;
			++sentThisTick;
		}

		if (transfer.nextChunkIndex >= transfer.totalChunks) {
			CHandlingActionPacket end(ACTION_FILE_TRANSFER_END);
			end.data.Write(transfer.modelId);
			end.data.Write(static_cast<uint8_t>(transfer.kind));
			player->sendPacket(
				Span<uint8_t>(end.data.GetData(), end.data.GetNumberOfBytesUsed()),
				kFileTransferChannel, true);
			// done
		} else {
			g_activeTransfers.push_back(transfer);
		}
	}
}
}

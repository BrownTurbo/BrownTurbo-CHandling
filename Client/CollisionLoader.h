#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class CBaseModelInfo;
class CColModel;

namespace ExtendedVeh::Collision {
using CustomModelId = std::uint32_t;
using GtaModelId = std::int32_t;

enum class Version : std::uint8_t {
	Unknown = 0,
	COL1 = 1, // COLL
	COL2 = 2,
	COL3 = 3,
	COL4 = 4
};

enum class Result : std::uint8_t {
	Success = 0,

	AlreadyLoaded,
	NotLoaded,
	NotInitialized,

	FileNotFound,
	OpenFailed,
	EmptyFile,
	FileTooLarge,

	TruncatedHeader,
	BadMagic,
	UnsupportedVersion,
	InvalidRecordSize,
	RecordOutOfBounds,
	InvalidModelName,
	NoRecords,
	ModelNotFoundInFile,

	TargetModelNotFound,
	TargetWrongType,
	TargetSharedModel,
	TargetAlreadyOwned,

	AllocationFailed,
	GtaLoaderFailed,
	AttachFailed,
	DetachFailed,

	ReferenceStillActive,
	CollisionEmpty,

	InternalError
};

struct RecordInfo {
	Version version { Version::Unknown };

	std::uint64_t fileOffset {};
	std::uint64_t totalSize {};

	std::uint64_t bodyOffset {};
	std::uint64_t bodySize {};

	std::int16_t sourceModelId { -1 };

	char modelName[23] {};
};

struct Statistics {
	std::uint64_t fileSize {};

	std::uint32_t recordCount {};
	std::uint32_t selectedRecordIndex {};

	Version selectedVersion {
		Version::Unknown
	};

	std::uint32_t sphereCount {};
	std::uint32_t boxCount {};
	std::uint32_t lineCount {};
	std::uint32_t triangleCount {};

	std::uint32_t shadowVertexCount {};
	std::uint32_t shadowTriangleCount {};

	bool multipleRecords {};
	bool mixedVersions {};

	bool hasCollisionData {};
	bool hasShadowData {};
};

struct Handle {
	CustomModelId customModelId {};

	/*
	 * The GTA model-info to which this collision has been attached.
	 *
	 * IMPORTANT:
	 * This must be a dedicated/custom model-info target if you want
	 * per-custom-vehicle collision. Do NOT pass 411 here merely because
	 * the custom vehicle uses Infernus handling/physics.
	 */
	GtaModelId gtaModelId { -1 };

	CColModel* collision { nullptr };

	CColModel* previousCollision { nullptr };

	bool previousOwned { false };
	bool installed { false };

	std::uint32_t references {};

	Statistics statistics {};
};

class CollisionLoader final {
public:
	static CollisionLoader& Instance();

	bool Initialize();
	void Shutdown();

	/*
	 * Read and structurally inspect a .col without touching GTA state.
	 *
	 * This function is safe to call on the asset-validation path.
	 */
	Result InspectFile(
		const std::filesystem::path& path,
		Statistics& outStatistics,
		std::vector<RecordInfo>& outRecords,
		std::string& outError) const;

	/*
	 * Load one selected COL record into a real GTA CColModel.
	 *
	 * IMPORTANT:
	 * Call from the GTA main/game thread.
	 */
	Result Load(
		CustomModelId customModelId,
		GtaModelId targetModelId,
		const std::filesystem::path& path,
		const char* expectedModelName,
		Handle& outHandle,
		std::string& outError);

	/*
	 * Attach an already-loaded collision to a dedicated/custom model-info.
	 *
	 * Call from GTA main thread.
	 */
	Result Attach(
		Handle& handle,
		std::string& outError);

	/*
	 * Detach collision from the model-info and restore the previous one.
	 *
	 * Call from GTA main thread.
	 */
	Result Detach(
		Handle& handle,
		std::string& outError);

	/*
	 * Release our reference to a loaded custom collision.
	 */
	Result Release(
		CustomModelId customModelId,
		std::string& outError);

	/*
	 * Fully remove a custom collision after no users remain.
	 *
	 * Call from GTA main thread.
	 */
	Result Unload(
		CustomModelId customModelId,
		std::string& outError);

	bool AddReference(
		CustomModelId customModelId);

	bool RemoveReference(
		CustomModelId customModelId);

	bool IsLoaded(
		CustomModelId customModelId) const;

	std::uint32_t GetReferenceCount(
		CustomModelId customModelId) const;

	std::optional<Statistics>
	GetStatistics(
		CustomModelId customModelId) const;

	const char* ResultToString(
		Result result) const;

private:
	CollisionLoader() = default;
	~CollisionLoader() = default;

	CollisionLoader(
		const CollisionLoader&)
		= delete;

	CollisionLoader& operator=(
		const CollisionLoader&)
		= delete;

	struct FileBuffer {
		std::vector<std::uint8_t> bytes;
	};

	struct RuntimeEntry {
		Handle handle {};

		std::filesystem::path sourcePath;

		std::string expectedModelName;
	};

	static constexpr std::uint64_t
		kMaxFileSize
		= 32ull * 1024ull * 1024ull;

	static constexpr std::size_t
		kRecordHeaderSize
		= 32;

	mutable std::mutex mutex_;

	bool initialized_ { false };

	std::unordered_map<
		CustomModelId,
		RuntimeEntry>
		loaded_;

	bool ReadFile(
		const std::filesystem::path& path,
		FileBuffer& outFile,
		std::string& outError) const;

	Result ParseFile(
		const FileBuffer& file,
		Statistics& outStatistics,
		std::vector<RecordInfo>& outRecords,
		std::string& outError) const;

	static std::optional<Version>
	DecodeVersion(
		const std::uint8_t* fourcc);

	static std::uint32_t
	ReadU32LE(
		const std::uint8_t* p);

	static std::int16_t
	ReadI16LE(
		const std::uint8_t* p);

	static bool
	ReadModelName(
		const std::uint8_t* p,
		char outName[23]);

	static bool
	ModelNameEquals(
		const char* lhs,
		const char* rhs);

	static bool
	IsZeroTail(
		const std::uint8_t* data,
		std::size_t size);

	static bool
	IsValidModelNameByte(
		std::uint8_t value);

	static bool
	RangeWithin(
		std::size_t offset,
		std::size_t size,
		std::size_t total);

	static void
	PopulateStatistics(
		const CColModel& colModel,
		Statistics& statistics);

	const RecordInfo*
	SelectRecord(
		const std::vector<RecordInfo>& records,
		GtaModelId targetModelId,
		const char* expectedModelName) const;

	Result LoadRecordIntoGta(
		const FileBuffer& file,
		const RecordInfo& record,
		CColModel& outCollision,
		Statistics& statistics,
		std::string& outError) const;
};
}

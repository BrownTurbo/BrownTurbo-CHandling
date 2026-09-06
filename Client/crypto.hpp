#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>

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

#pragma once

#include <cstdint>
#include <vector>

class BinaryRwParser {
public:
    static std::vector<uint8_t> ExtractClump(const std::vector<uint8_t>& rawBuffer) {
        if (rawBuffer.size() < 12) return {};

        size_t offset = 0;
        while (offset + 12 <= rawBuffer.size()) {
            uint32_t chunkId = *reinterpret_cast<const uint32_t*>(&rawBuffer[offset]);
            uint32_t chunkSize = *reinterpret_cast<const uint32_t*>(&rawBuffer[offset + 4]);

            constexpr uint32_t RW_ID_CLUMP = 0x10;
            if (chunkSize > rawBuffer.size() - offset - 12) {
                if (chunkId == RW_ID_CLUMP && offset == 0) {
                    return rawBuffer;
                }
                break;
            }

            if (chunkId == RW_ID_CLUMP) {
                return std::vector<uint8_t>(
                    rawBuffer.begin() + offset,
                    rawBuffer.begin() + offset + 12 + chunkSize
                );
            }
            offset += 12 + chunkSize;
        }
        return {};
    }
};
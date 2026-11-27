// Copyright (C) 2026  ilobilo

module;

#include <physfs.h>

module minesweeper;

namespace minesweeper
{
    std::expected<std::unique_ptr<InputStream>, AssetError> InputStream::create(const std::string &path)
    {
        auto *file = PHYSFS_openRead(path.c_str());
        if (file == nullptr)
            return std::unexpected { AssetError::asset_not_found };
        return std::unique_ptr<InputStream> { new InputStream { file } };
    }

    InputStream::~InputStream()
    {
        close();
    }

    void InputStream::close()
    {
        if (file_ == nullptr)
            return;

        PHYSFS_close(static_cast<PHYSFS_File *>(file_));
        file_ = nullptr;
    }

    [[nodiscard]] std::optional<std::size_t> InputStream::read(void *data, std::size_t size)
    {
        const auto result = PHYSFS_readBytes(static_cast<PHYSFS_File *>(file_), data, size);
        if (result < 0)
            return std::nullopt;
        return result;
    }

    [[nodiscard]] std::optional<std::size_t> InputStream::seek(std::size_t position)
    {
        if (!PHYSFS_seek(static_cast<PHYSFS_File *>(file_), position))
            return std::nullopt;
        return position;
    }

    [[nodiscard]] std::optional<std::size_t> InputStream::tell()
    {
        const auto result = PHYSFS_tell(static_cast<PHYSFS_File *>(file_));
        if (result < 0)
            return std::nullopt;
        return result;
    }

    std::optional<std::size_t> InputStream::getSize()
    {
        const auto result = PHYSFS_fileLength(static_cast<PHYSFS_File *>(file_));
        if (result < 0)
            return std::nullopt;
        return result;
    }

    [[nodiscard]] std::optional<std::vector<std::byte>> InputStream::read_all()
    {
        const auto size = getSize();
        if (!size || !seek(0))
            return std::nullopt;

        std::vector<std::byte> buffer(*size);
        if (*size == 0)
            return buffer;

        const auto bytes_read = read(buffer.data(), buffer.size());
        if (!bytes_read || *bytes_read != buffer.size())
            return std::nullopt;

        return buffer;
    }
} // namespace minesweeper

// Copyright (C) 2026  ilobilo

module;

#include <physfs.h>

module minesweeper;

namespace minesweeper
{
    namespace
    {
        bool valid_theme_name(std::string_view name) noexcept
        {
            return !name.empty() && name.front() != '-' && name.back() != '-'
                && std::ranges::all_of(name, [](char chr) {
                    return std::isalnum(static_cast<unsigned char>(chr)) || chr == '_' || chr == '-';
                });
        }

        std::string_view detail(PHYSFS_ErrorCode code) noexcept
        {
            const char *str;
            if (code == PHYSFS_ERR_OK || !(str = PHYSFS_getErrorByCode(code)))
                return { };
            return str;
        }

        std::string_view error_name(Filesystem::Error error) noexcept
        {
            switch (error)
            {
                case Filesystem::Error::init_failed:
                    return "Failed to initialize PhysFS";
                case Filesystem::Error::mount_failed:
                    return "Failed to mount archive";
                case Filesystem::Error::theme_not_found:
                    return "Theme directory or archive not found";
                case Filesystem::Error::invalid_theme_name:
                    return "Invalid theme name";
                case Filesystem::Error::invalid_theme_layout:
                    return "Theme is missing 'theme.json'";
            }
            std::unreachable();
        }

        auto physfs_error(Filesystem::Error error, PHYSFS_ErrorCode code = PHYSFS_getLastErrorCode()) noexcept
        {
            return std::unexpected { Filesystem::ErrorInfo { .error = error, .detail = detail(code) } };
        }

        auto fs_error(Filesystem::Error error) noexcept
        {
            return std::unexpected { Filesystem::ErrorInfo { .error = error, .detail { } } };
        }
    } // namespace

    [[nodiscard]] std::string Filesystem::ErrorInfo::what() const
    {
        std::string err { error_name(error) };
        if (detail.empty())
            return err;
        return std::format("{}. PhysFS returned '{}'", std::move(err), detail);
    }

    auto Filesystem::create(const char *argv0) noexcept -> std::expected<Filesystem, ErrorInfo>
    {
        if (!PHYSFS_init(argv0))
            return physfs_error(Error::init_failed);

        PHYSFS_permitSymbolicLinks(0);
        return Filesystem { };
    }

    Filesystem::~Filesystem()
    {
        if (active_)
            PHYSFS_deinit();
    }

    auto Filesystem::mount_builtin(std::span<const std::byte> archive) noexcept -> std::expected<void, ErrorInfo>
    {
        if (!PHYSFS_mountMemory(archive.data(), archive.size(), nullptr, "builtin.zip", "/", 1))
            return physfs_error(Error::mount_failed);
        return { };
    }

    auto Filesystem::mount_theme(const fs::path &assets, std::string_view name) -> std::expected<void, ErrorInfo>
    {
        if (!valid_theme_name(name))
            return fs_error(Error::invalid_theme_name);

        const std::string stem { name };
        const std::array candidates { assets / stem, assets / (stem + ".zip") };

        std::optional<fs::path> found;
        for (const auto &path : candidates)
        {
            std::error_code ec;
            if (fs::is_directory(path, ec) || fs::is_regular_file(path, ec))
            {
                found = path;
                break;
            }
        }

        if (!found)
            return fs_error(Error::theme_not_found);

        std::error_code ec;
        const auto normalised = fs::weakly_canonical(*found, ec);
        const auto native = ec ? found->lexically_normal().string() : normalised.string();

        if (theme_mount_ == native)
            return { };

         if (!PHYSFS_mount(native.c_str(), "/", 0))
            return physfs_error(Error::mount_failed);

        const char *provider = PHYSFS_getRealDir("theme.json");
        if (provider == nullptr || native != provider)
        {
            PHYSFS_unmount(native.c_str());
            return physfs_error(Error::invalid_theme_layout);
        }

        if (theme_mount_)
            PHYSFS_unmount(theme_mount_->c_str());
        theme_mount_ = native;
        return { };
    }
} // namespace minesweeper

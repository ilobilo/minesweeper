// Copyright (C) 2026  ilobilo

module;

#include <physfs.h>

module minesweeper;

namespace minesweeper
{
    namespace
    {
        constexpr bool is_ascii_alnum(char chr) noexcept
        {
            return (chr >= 'a' && chr <= 'z')
                || (chr >= 'A' && chr <= 'Z')
                || (chr >= '0' && chr <= '9');
        }

        std::string_view physfs_detail(PHYSFS_ErrorCode code) noexcept
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
                    return "Failed to initialize virtual filesystem";
                case Filesystem::Error::mount_failed:
                    return "Failed to mount theme archive";
                case Filesystem::Error::unmount_failed:
                    return "Failed to unmount theme archive";
                case Filesystem::Error::theme_not_found:
                    return "Theme not found";
                case Filesystem::Error::invalid_theme_name:
                    return "Invalid theme name";
                case Filesystem::Error::invalid_theme_layout:
                    return "Invalid theme layout";
            }
            std::unreachable();
        }

        auto fs_error(Filesystem::Error error, std::string detail = { }) noexcept
        {
            return std::unexpected { Filesystem::ErrorInfo { .error = error, .detail = std::move(detail) } };
        }

        auto physfs_error(Filesystem::Error error, std::string context = { }, PHYSFS_ErrorCode code = PHYSFS_getLastErrorCode())
        {
            const auto reason = physfs_detail(code);
            if (context.empty())
                return fs_error(error, std::string { reason });
            if (reason.empty())
                return fs_error(error, std::move(context));
            return fs_error(error, std::format("{} (PhysFS: {})", std::move(context), reason));
        }

        auto resolve_theme_path(const fs::path &assets, std::string_view name)
            -> std::expected<std::string, Filesystem::ErrorInfo>
        {
            if (!Filesystem::valid_theme_name(name))
            {
                return fs_error(
                    Filesystem::Error::invalid_theme_name,
                    name.empty()
                        ? "name must not be empty"
                        : std::format("'{}' (must match '^\\w([\\w-]*\\w)?$)'", name)
                );
            }

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
            {
                return fs_error(
                    Filesystem::Error::theme_not_found,
                    std::format("'{0}' (expected '{0}/' or '{0}.zip' in '{1}')", name, util::to_utf8(assets))
                );
            }

            std::error_code ec;
            const auto normalised = fs::weakly_canonical(*found, ec);
            return util::to_utf8(ec ? found->lexically_normal() : normalised);
        }

        std::optional<std::string> read_physfs_file(const char *path)
        {
            auto *file = PHYSFS_openRead(path);
            if (file == nullptr)
                return std::nullopt;

            struct CloseGuard
            {
                PHYSFS_File *handle;
                ~CloseGuard() { PHYSFS_close(handle); }
            } guard { file };

            const auto size = PHYSFS_fileLength(file);
            if (size < 0)
                return std::nullopt;

            std::string contents(static_cast<std::size_t>(size), '\0');
            if (size == 0)
                return contents;

            const auto bytes_read = PHYSFS_readBytes(file, contents.data(), contents.size());
            if (bytes_read != size)
                return std::nullopt;

            return contents;
        }
    } // namespace

    [[nodiscard]] std::string Filesystem::ErrorInfo::what() const
    {
        std::string err { error_name(error) };
        if (detail.empty())
            return err;
        return std::format("{}: {}", std::move(err), detail);
    }

    [[nodiscard]] bool Filesystem::valid_theme_name(std::string_view name) noexcept
    {
        return !name.empty() && name.front() != '-' && name.back() != '-'
            && std::ranges::all_of(name, [](char chr) {
                return is_ascii_alnum(chr) || chr == '_' || chr == '-';
            });
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

    auto Filesystem::mount_builtin(std::span<const std::byte> archive) -> std::expected<void, ErrorInfo>
    {
        if (!PHYSFS_mountMemory(archive.data(), archive.size(), nullptr, "builtin.zip", "/", 1))
            return physfs_error(Error::mount_failed, "built-in theme archive");
        return { };
    }

    auto Filesystem::mount_theme(const fs::path &assets, std::string_view name) -> std::expected<void, ErrorInfo>
    {
        const auto native = resolve_theme_path(assets, name);
        if (!native)
            return std::unexpected { native.error() };

        if (theme_mount_ == *native)
            return { };

        if (!PHYSFS_mount(native->c_str(), "/", 0))
            return physfs_error(Error::mount_failed, std::format("'{}' ('{}')", name, *native));

        const char *provider = PHYSFS_getRealDir("theme.json");
        if (provider == nullptr || *native != provider)
        {
            PHYSFS_unmount(native->c_str());
            return fs_error(
                Error::invalid_theme_layout,
                std::format("'{}' ('{}') is missing 'theme.json' manifest", name, *native)
            );
        }

        if (theme_mount_ && !PHYSFS_unmount(theme_mount_->c_str()))
        {
            const auto err = physfs_error(Error::unmount_failed, std::format("'{}'", *theme_mount_));
            PHYSFS_unmount(native->c_str());
            return err;
        }

        theme_mount_ = *native;
        return { };
    }

    auto Filesystem::read_theme_manifest(const fs::path &assets, std::string_view name) const
        -> std::expected<std::string, ErrorInfo>
    {
        const auto native = resolve_theme_path(assets, name);
        if (!native)
            return std::unexpected { native.error() };

        if (theme_mount_ == *native)
        {
            auto contents = read_physfs_file("theme.json");
            if (!contents)
            {
                return fs_error(
                    Error::invalid_theme_layout,
                    std::format("'{}' ('{}') is missing 'theme.json' manifest", name, *native)
                );
            }
            return *contents;
        }

        if (!PHYSFS_mount(native->c_str(), "/_tmp", 0))
            return physfs_error(Error::mount_failed, std::format("'{}' ('{}')", name, *native));

        struct UnmountGuard
        {
            const char *path;
            ~UnmountGuard() { PHYSFS_unmount(path); }
        } guard { native->c_str() };

        const char *provider = PHYSFS_getRealDir("_tmp/theme.json");
        if (provider == nullptr || *native != provider)
        {
            return fs_error(
                Error::invalid_theme_layout,
                std::format("'{}' ('{}') is missing 'theme.json' manifest", name, *native)
            );
        }

        auto contents = read_physfs_file("_tmp/theme.json");
        if (!contents)
            return physfs_error(Error::invalid_theme_layout, std::format("'{}' ('{}')", name, *native));

        return *contents;
    }

    bool Filesystem::unmount_theme() noexcept
    {
        if (!theme_mount_)
            return true;

        if (!PHYSFS_unmount(theme_mount_->c_str()))
            return false;

        theme_mount_.reset();
        return true;
    }
} // namespace minesweeper

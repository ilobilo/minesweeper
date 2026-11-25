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

        bool valid_theme_name(std::string_view name) noexcept
        {
            return !name.empty() && name.front() != '-' && name.back() != '-'
                && std::ranges::all_of(name, [](char chr) {
                    return is_ascii_alnum(chr) || chr == '_' || chr == '-';
                });
        }

        std::string to_utf8(const fs::path &path)
        {
            const auto u8 = path.u8string();
            return { reinterpret_cast<const char *>(u8.data()), u8.size() };
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
    } // namespace

    [[nodiscard]] std::string Filesystem::ErrorInfo::what() const
    {
        std::string err { error_name(error) };
        if (detail.empty())
            return err;
        return std::format("{}: {}", std::move(err), detail);
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
        if (!valid_theme_name(name))
        {
            return fs_error(
                Error::invalid_theme_name,
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
                Error::theme_not_found,
                std::format("'{0}' (expected '{0}/' or '{0}.zip' in '{1}')", name, to_utf8(assets))
            );
        }

        std::error_code ec;
        const auto normalised = fs::weakly_canonical(*found, ec);
        const auto native = to_utf8(ec ? found->lexically_normal() : normalised);

        if (theme_mount_ == native)
            return { };

        if (!PHYSFS_mount(native.c_str(), "/", 0))
            return physfs_error(Error::mount_failed, std::format("'{}' ('{}')", name, native));

        const char *provider = PHYSFS_getRealDir("theme.json");
        if (provider == nullptr || native != provider)
        {
            PHYSFS_unmount(native.c_str());
            return fs_error(
                Error::invalid_theme_layout,
                std::format("'{}' ('{}') is missing 'theme.json' manifest", name, native)
            );
        }

        if (theme_mount_)
            PHYSFS_unmount(theme_mount_->c_str());
        theme_mount_ = native;
        return { };
    }

    void Filesystem::unmount_theme() noexcept
    {
        if (!theme_mount_)
            return;

        PHYSFS_unmount(theme_mount_->c_str());
        theme_mount_.reset();
    }
} // namespace minesweeper

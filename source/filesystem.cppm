// Copyright (C) 2026  ilobilo

export module minesweeper:filesystem;

import std;

import :utilities;

export namespace minesweeper
{
    class Filesystem
    {
        public:
        enum class Error
        {
            init_failed,
            mount_failed,
            theme_not_found,
            invalid_theme_name,
            invalid_theme_layout
        };

        struct ErrorInfo
        {
            Error error;
            std::string detail;

            [[nodiscard]] std::string what() const;
        };

        static std::expected<Filesystem, ErrorInfo> create(const char *argv0) noexcept;

        ~Filesystem();

        Filesystem(const Filesystem &) = delete;
        Filesystem(Filesystem &&other) noexcept
            : theme_mount_ { std::move(other.theme_mount_) },
              active_ { std::exchange(other.active_, false) } { }

        Filesystem &operator=(const Filesystem &) = delete;
        Filesystem &operator=(Filesystem &&) = delete;

        std::expected<void, ErrorInfo> mount_builtin(std::span<const std::byte> archive);
        std::expected<void, ErrorInfo> mount_theme(const fs::path &assets, std::string_view name);

        void unmount_theme() noexcept;

        private:
        std::optional<std::string> theme_mount_;
        bool active_ = true;

        Filesystem() = default;
    };
} // export namespace minesweeper

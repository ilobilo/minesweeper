// Copyright (C) 2026  ilobilo

export module minesweeper:utilities;

import std;

export
{
    namespace fs = std::filesystem;
    using namespace std::literals::chrono_literals;
} // export

export namespace minesweeper::util
{
    template<typename ...Funcs>
    struct overload : Funcs... { using Funcs::operator()...; };

    template<typename ...Funcs>
    overload(Funcs ...) -> overload<Funcs...>;

    template<typename Type, typename Error>
    using expected_ref = std::expected<std::reference_wrapper<Type>, Error>;

    inline constexpr void unused([[maybe_unused]] auto &&...) { }

    inline std::string to_utf8(const fs::path &path)
    {
        const auto u8 = path.u8string();
        return { reinterpret_cast<const char *>(u8.data()), u8.size() };
    }
} // export namespace minesweeper::util

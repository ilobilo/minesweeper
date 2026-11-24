// Copyright (C) 2026  ilobilo

export module minesweeper:log;

import std;

namespace minesweeper::log
{
    namespace detail
    {
        enum class Level
        {
            info,
            warn,
            error
        };

        consteval std::string_view prefix(Level lvl) noexcept
        {
            switch (lvl)
            {
                case Level::info:
                    return "\033[36minfo:\033[0m";
                case Level::warn:
                    return "\033[33mwarning:\033[0m";
                case Level::error:
                    return "\033[31merror:\033[0m";
                    break;
            }
            std::unreachable();
        }

        template<Level Lvl, typename ...Args>
        void println(std::format_string<Args...> fmt, Args &&...args)
        {
            std::println(
                Lvl == Level::info ? std::cout : std::cerr, "{} {}",
                prefix(Lvl), std::format(fmt, std::forward<Args>(args)...)
            );
        }
    } // namespace detail

    export template<typename ...Args>
    void info(std::format_string<Args...> fmt, Args &&...args)
    {
         detail::println<detail::Level::info>(fmt, std::forward<Args>(args)...);
    }

    export template<typename ...Args>
    void warn(std::format_string<Args...> fmt, Args &&...args)
    {
        detail::println<detail::Level::warn>(fmt, std::forward<Args>(args)...);
    }

    export template<typename ...Args>
    void error(std::format_string<Args...> fmt, Args &&...args)
    {
        detail::println<detail::Level::error>(fmt, std::forward<Args>(args)...);
    }
} // namespace minesweeper::log

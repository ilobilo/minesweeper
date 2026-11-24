// Copyright (C) 2026  ilobilo

export module minesweeper:utilities;

import std;

export namespace minesweeper::util
{
    template<typename ...Funcs>
    struct overload : Funcs... { using Funcs::operator()...; };

    template<typename ...Funcs>
    overload(Funcs ...) -> overload<Funcs...>;

    template<typename Type, typename Error>
    using expected_ref = std::expected<std::reference_wrapper<Type>, Error>;
} // export namespace minesweeper::util

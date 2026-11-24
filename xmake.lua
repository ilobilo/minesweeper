-- Copyright (C) 2026  ilobilo

set_xmakever("3.1.2")

set_project("minesweeper")
set_version("v0.1")
set_license("EUPL-1.2")

includes("sanitizers.lua")
add_rules("mode.release", "mode.releasedbg", "mode.debug", "mode.sanitize")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "$(builddir)" })

set_policy("run.autobuild", true)
set_rundir("$(projectdir)")

set_toolchains("clang")
set_languages("c++26")
set_runtimes("stdc++_shared")

set_policy("build.c++.modules", true)
set_policy("build.c++.modules.std", true)
set_policy("build.c++.modules.two_phases", true)
set_policy("build.c++.modules.clang.precompile_reduced_bmi", true)

set_defaultmode("release")
set_optimize("fastest")
set_warnings("all", "extra")

add_repositories("local-repo repo")

add_requires("sfml", { configs = { network = false } })
add_requires("cli11", { configs = { modules = true } })
add_requires("nlohmann_json 2026.11.18", { configs = { modules = true } })

local archivers = {
    ["zip"] = true,
    ["7z"] = false,
    ["grp"] = false,
    ["wad"] = false,
    ["hog"] = false,
    ["mvl"] = false,
    ["qpak"] = false,
    ["slb"] = false,
    ["iso9660"] = false,
    ["vdf"] = false,
}
add_requires("physfs", { configs = archivers})

target("minesweeper")
    set_kind("binary")
    add_packages("sfml", "cli11", "nlohmann_json", "physfs")

    add_embeddirs("assets")
    add_files("source/**.cpp", "source/**.cppm")

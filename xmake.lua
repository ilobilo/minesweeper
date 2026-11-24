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
add_requires("physfs", { configs = archivers })

rule("pack_assets")
    on_config(function (target)
        local autogendir = target:autogendir()
        os.mkdir(autogendir)
        target:add("cxflags", "--embed-dir=" .. autogendir, { force = true })
    end)

    before_build(function (target)
        import("core.project.depend")
        import("utils.archive")
        import("utils.progress")

        local assetsdir = path.join(os.projectdir(), "assets/default")
        local zipfile = path.absolute(path.join(target:autogendir(), "builtin.zip"))
        local files = os.files(path.join(assetsdir, "**"))

        depend.on_changed(function ()
            progress.show(
                opt and opt.progress or 0,
                "<%s> ${color.build.object}packing.$(mode) %s",
                target:name(), path.filename(zipfile)
            )

            os.tryrm(zipfile)
            local oldir = os.cd(assetsdir)
            local entries = os.filedirs("*")
            if #entries > 0 then
                archive.archive(zipfile, entries, { recurse = true })
            end
            os.cd(oldir)
        end, { dependfile = target:dependfile(zipfile), files = files })
    end)
rule_end()

target("minesweeper")
    set_kind("binary")
    add_packages("sfml", "cli11", "nlohmann_json", "physfs")

    add_cxflags("-Wno-c23-extensions")

    add_rules("pack_assets")
    add_files("source/**.cpp", "source/**.cppm")

-- The following file has been clanked out by a clanker
-- Based on https://github.com/xmake-io/xmake-repo/blob/9bf30fdddc25f58bf7c966da383c8659246e0a3d/packages/c/cli11/xmake.lua

package("cli11")
    set_kind("library", { moduleonly = true })
    set_homepage("https://github.com/CLIUtils/CLI11")
    set_description("CLI11 is a command line parser for C++11 and beyond that provides a rich feature set with a simple and intuitive interface.")
    set_license("BSD")

    add_urls("https://github.com/CLIUtils/CLI11/archive/refs/tags/$(version).tar.gz",
             "https://github.com/CLIUtils/CLI11.git")

    add_versions("v2.7.2", "46eef3101da70852ec7af026e09d485ccee81813331c8c6052d39344443b83da")
    add_versions("v2.6.2", "c6ea6b2e5608b3ea8617999bd5f47420c71b2ebdb8dc4767c1034d1da5785711")
    add_versions("v2.6.1", "377691f3fac2b340f12a2f79f523c780564578ba3d6eaf5238e9f35895d5ba95")
    add_versions("v2.5.0", "17e02b4cddc2fa348e5dbdbb582c59a3486fa2b2433e70a0c3bacb871334fd55")
    add_versions("v2.4.2", "f2d893a65c3b1324c50d4e682c0cdc021dd0477ae2c048544f39eed6654b699a")
    add_versions("v2.4.1", "73b7ec52261ce8fe980a29df6b4ceb66243bb0b779451dbd3d014cfec9fdbb58")
    add_versions("v2.3.2", "aac0ab42108131ac5d3344a9db0fdf25c4db652296641955720a4fbe52334e22")
    add_versions("v2.2.0", "d60440dc4d43255f872d174e416705f56ba40589f6eb07727f76376fb8378fd6")

    add_configs("modules", { description = "Build and export the experimental C++ named module", default = false, type = "boolean" })

    if not is_host("windows") then
        add_extsources("pkgconfig::CLI11")
    end

    if is_plat("windows", "mingw") then
        add_syslinks("shell32")
    end

    on_load(function (package)
        if package:config("modules") then
            if package:version():lt("2.6.2") then
                raise("cli11-modules: C++ module support is available from CLI11 2.6.2")
            end
        end
    end)

    on_install(function (package)
        if package:config("modules") then
            local cxx_standard = package:version():ge("2.7.2") and "c++20" or "c++23"

            local extra_sources = ""
            local public_defines = ""
            if package:version():ge("2.7.2") then
                extra_sources = '    add_files("src/Precompile.cpp")\n'
                public_defines = '    add_defines("CLI11_COMPILE", { public = true })\n'
            end

            local xmakefile = string.format([[
set_project("cli11-modules")
set_languages("%s")
set_policy("build.c++.modules", true)

target("cli11")
    set_kind("static")
%s    add_files("src/modules/CLI11.cppm", {public = true})
    add_includedirs("include", {public = true})
%s    if is_plat("windows", "mingw") then
        add_syslinks("shell32")
    end
]], cxx_standard, extra_sources, public_defines)

            io.writefile("xmake.lua", xmakefile)
            import("package.tools.xmake").install(package)
        end
        os.cp("include", package:installdir())
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({ test = [[
            CLI::App app{"Test", "test"};
        ]] }, { configs = { languages = "cxx11" }, includes = "CLI/CLI.hpp" }))
    end)

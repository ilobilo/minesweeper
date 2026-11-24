-- The following file has been clanked out by a clanker
-- Based on https://github.com/xmake-io/xmake-repo/blob/9bf30fdddc25f58bf7c966da383c8659246e0a3d/packages/n/nlohmann_json/xmake.lua

package("nlohmann_json")
    set_kind("library", { moduleonly = true })
    set_homepage("https://nlohmann.github.io/json/")
    set_description("JSON for Modern C++")
    set_license("MIT")

    add_urls("https://github.com/nlohmann/json/archive/refs/tags/$(version).tar.gz")
    add_urls("https://github.com/nlohmann/json.git", { alias = "git" })

    add_versions("git:2026.11.18", "098eebd412883ccb9d1a26523758e5ec457f58e0")

    add_versions("v3.12.0", "4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187")
    add_versions("v3.11.3", "0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406")
    add_versions("v3.11.2", "d69f9deb6a75e2580465c6c4c5111b89c4dc2fa94e3a85fcd2ffcd9a143d9273")
    add_versions("v3.10.5", "5daca6ca216495edf89d167f808d1d03c4a4d929cef7da5e10f135ae1540c7e4")
    add_versions("v3.10.0", "eb8b07806efa5f95b349766ccc7a8ec2348f3b2ee9975ad879259a371aea8084")
    add_versions("v3.9.1", "4cf0df69731494668bdd6460ed8cb269b68de9c19ad8c27abc24cd72605b2d5b")

    add_patches("v3.12.0", path.join(os.scriptdir(), "patches", "v3.12.0", "fix-ndk.patch"), "e1e6958da01c142fd72dcb13ead1624a19e22f88b7b602fd440db6ac984561cf")

    add_configs("modules", { description = "Build and export the upstream C++20 named module", default = false, type = "boolean" })

    on_load(function (package)
        if package:config("modules") then
            if package:version():lt("2026.11.18") then
                raise("nlohmann_json: C++ module support requires the pinned Git version 2026.11.18 or newer")
            end
        else
            package:add("includedirs", "include")

            if package:is_plat("mingw") and is_subhost("msys") then
                package:add("extsources", "pacman::nlohmann-json")
            elseif package:is_plat("linux") then
                package:add("extsources", "pacman::nlohmann-json", "apt::nlohmann-json3-dev")
            elseif package:is_plat("macosx") then
                package:add("extsources", "brew::nlohmann-json")
            end
        end
    end)

    on_install(function (package)
        if package:config("modules") then
            local modulefile = path.join("src", "modules", "json.cppm")
            if not os.isfile(modulefile) then
                raise("nlohmann_json: the selected source revision does not contain " .. modulefile)
            end

            io.writefile("xmake.lua", [[
set_project("nlohmann_json")
set_languages("c++20")
set_policy("build.c++.modules", true)

target("nlohmann_json")
    set_kind("moduleonly")
    add_files("src/modules/json.cppm")
    add_includedirs("include", { public = true })
    add_defines("NLOHMANN_JSON_BUILD_MODULES", { public = true })
]])
            import("package.tools.xmake").install(package)

            os.cp("include", package:installdir())
        else
            if os.isdir("include") then
                os.cp("include", package:installdir())
            else
                os.cp("*", package:installdir("include"))
            end
        end
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({ test = [[
            using json = nlohmann::json;
            void test() {
                json data;
                data["name"] = "world";
            }
        ]] }, { configs = { languages = "c++14" }, includes = { "nlohmann/json.hpp" }}))
    end)

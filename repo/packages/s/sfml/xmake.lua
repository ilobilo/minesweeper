-- The following file has been clanked out by a clanker
-- Based on https://github.com/xmake-io/xmake-repo/blob/9bf30fdddc25f58bf7c966da383c8659246e0a3d/packages/s/sfml/xmake.lua

package("sfml")
    set_homepage("https://www.sfml-dev.org")
    set_description("Simple and Fast Multimedia Library")
    set_license("zlib")

    add_urls("https://github.com/mikomikotaishi/SFML.git")
    add_versions("3.1.0-modules", "45871e653581bc7dc50bed945d6ca21c7698dca9")

    add_configs("graphics", { description = "Use the graphics module", default = true, type = "boolean" })
    add_configs("window",   { description = "Use the window module",   default = true, type = "boolean" })
    add_configs("audio",    { description = "Use the audio module",    default = true, type = "boolean" })
    add_configs("network",  { description = "Use the network module",  default = true, type = "boolean" })
    if is_plat("windows", "mingw") then
        add_configs("main", { description = "Link to the sfml-main library", default = true, type = "boolean" })
    end

    on_component("graphics", function (package, component)
        local e = package:config("shared") and "" or "-s"
        if package:debug() then e = e .. "-d" end
        component:add("links", "sfml-graphics" .. e)
        component:add("deps", "window", "system")
    end)

    on_component("window", function (package, component)
        local e = package:config("shared") and "" or "-s"
        if package:debug() then e = e .. "-d" end
        component:add("links", "sfml-window" .. e)
        component:add("deps", "system")
        if not package:config("shared") then
            if package:is_plat("windows", "mingw") then
                component:add("syslinks", "opengl32", "gdi32", "advapi32", "user32")
            elseif package:is_plat("linux") then
                component:add("syslinks", "dl")
            elseif package:is_plat("bsd") then
                component:add("syslinks", "usbhid")
            elseif package:is_plat("macosx") then
                component:add("frameworks", "Foundation", "AppKit", "IOKit", "Carbon")
            end
        end
    end)

    on_component("audio", function (package, component)
        local e = package:config("shared") and "" or "-s"
        if package:debug() then e = e .. "-d" end
        component:add("links", "sfml-audio" .. e)
        component:add("deps", "system")
        if not package:config("shared") and package:is_plat("windows", "mingw") then
            component:add("links", "flac", "vorbisenc", "vorbisfile", "vorbis", "ogg")
        end
    end)

    on_component("network", function (package, component)
        local e = package:config("shared") and "" or "-s"
        if package:debug() then e = e .. "-d" end
        component:add("links", "sfml-network" .. e)
        component:add("deps", "system")
        if not package:config("shared") and package:is_plat("windows", "mingw") then
            component:add("syslinks", "ws2_32")
        end
    end)

    on_component("system", function (package, component)
        local e = package:config("shared") and "" or "-s"
        if package:debug() then e = e .. "-d" end
        component:add("links", "sfml-system" .. e)
        if package:is_plat("windows", "mingw") and package:config("main") then
            component:add("deps", "main")
        end
        if not package:config("shared") then
            if package:is_plat("windows", "mingw") then
                component:add("syslinks", "winmm")
            elseif package:is_plat("linux") then
                component:add("syslinks", "rt", "pthread")
            elseif package:is_plat("bsd", "macosx") then
                component:add("syslinks", "pthread")
            end
        end
    end)

    on_component("main", function (package, component)
        if package:is_plat("windows", "mingw") then
            component:add("links", "sfml-main" .. (package:debug() and "-d" or ""))
        end
    end)

    on_load("windows", "linux", "macosx", "mingw", function (package)
        package:add("deps", "cmake >=3.28", { kind = "binary" })
        package:add("deps", "ninja", { kind = "binary" })

        if not package:config("shared") then
            package:add("defines", "SFML_STATIC")
        end

        if package:config("graphics") then
            package:add("deps", "freetype", "harfbuzz")
        end

        if package:is_plat("linux") then
            if package:config("window") or package:config("graphics") then
                import("lib.detect.find_package")
                local udev_dep = find_package("libudev") and "libudev" or "eudev"
                package:add("deps", "libx11", "libxcursor", "libxrandr", "libxrender", "libxfixes", "libxext", udev_dep, "libxi")
                package:add("deps", "opengl", "glx", { optional = true })
            end
        end

        if package:config("audio") then
            package:add("deps", "libogg", "libflac", "libvorbis")
        end

        if package:config("network") then
            package:add("deps", "mbedtls")
        end

        package:add("components", "system")
        for _, component in ipairs({ "graphics", "window", "audio", "network" }) do
            if package:config(component) then
                package:add("components", component)
            end
        end
        if package:is_plat("windows", "mingw") and package:config("main") then
            package:add("components", "main")
        end
    end)

    on_install("windows", "linux", "macosx", "mingw", function (package)
        import("core.base.json")
        assert(not package:has_tool("cxx", "gcc", "gxx"), "sfml-modules: GCC is not supported by SFML's modules CMake (use clang or msvc)")

        local configs = {
            "-DSFML_BUILD_DOC=OFF",
            "-DSFML_BUILD_EXAMPLES=OFF",
            "-DSFML_BUILD_TEST_SUITE=OFF",
            "-DSFML_BUILD_MODULES=ON",
            "-DCMAKE_CXX_STANDARD=20",
            "-DWARNINGS_AS_ERRORS=OFF"
        }
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))

        if package:config("shared") then
            table.insert(configs, "-DBUILD_SHARED_LIBS=ON")
            if package:config("graphics") then
                local freetype = package:dep("freetype")
                if freetype then
                    local fetchinfo = freetype:fetch()
                    if fetchinfo and not freetype:config("shared") then
                        local libfiles = { }
                        for _, dep in ipairs(freetype:librarydeps()) do
                            local depinfo = dep:fetch()
                            if depinfo then
                                table.join2(libfiles, depinfo.libfiles)
                            end
                        end
                        if #libfiles > 0 then
                            local libraries = { }
                            for _, libfile in ipairs(libfiles) do
                                table.insert(libraries, (libfile:gsub("\\", "/")))
                            end
                            local file = io.open("src/SFML/Graphics/CMakeLists.txt", "a")
                            file:print("target_link_libraries(freetype INTERFACE %s)", table.concat(libraries, " "))
                            file:close()
                        end
                    end
                end
            end
            if package:config("window") and package:is_plat("linux") then
                local libfiles = { }
                for _, name in ipairs({ "libx11", "libxcursor", "libxrandr", "libxrender", "libxfixes", "libxext" }) do
                    local dep = package:dep(name)
                    if dep then
                        local fetchinfo = dep:fetch()
                        if fetchinfo then
                            table.join2(libfiles, fetchinfo.libfiles)
                        end
                    end
                end
                if #libfiles > 0 then
                    libfiles = table.reverse_unique(libfiles)
                    local libraries = { }
                    for _, libfile in ipairs(libfiles) do
                        table.insert(libraries, (libfile:gsub("\\", "/")))
                    end
                    local file = io.open("src/SFML/Window/CMakeLists.txt", "a")
                    file:print("target_link_libraries(sfml-window PRIVATE " .. table.concat(libraries, " ") .. ")")
                    file:close()
                end
            end
        else
            table.insert(configs, "-DBUILD_SHARED_LIBS=OFF")
            table.insert(configs, "-DSFML_USE_STATIC_STD_LIBS=OFF")
        end

        table.insert(configs, "-DSFML_BUILD_AUDIO="    .. (package:config("audio")    and "ON" or "OFF"))
        table.insert(configs, "-DSFML_BUILD_GRAPHICS=" .. (package:config("graphics") and "ON" or "OFF"))
        table.insert(configs, "-DSFML_BUILD_WINDOW="   .. (package:config("window")   and "ON" or "OFF"))
        table.insert(configs, "-DSFML_BUILD_NETWORK="  .. (package:config("network")  and "ON" or "OFF"))
        if not package:is_plat("mingw") then
            table.insert(configs, "-DSFML_USE_SYSTEM_DEPS=TRUE")
        end

        local opt = { packagedeps = { } }
        if package:config("graphics") then
            table.join2(opt.packagedeps, { "freetype", "harfbuzz", "zlib" })
        end
        if package:config("network") then
            table.insert(opt.packagedeps, "mbedtls")
        end
        if not package:is_plat("windows") then
            opt.cmake_generator = "Ninja"
        end
        import("package.tools.cmake").install(package, configs, opt)

        local moddir = path.join(package:installdir(), "modules", "sfml")
        os.mkdir(moddir)
        local defines = { }
        for _, name in ipairs({ "graphics", "window", "audio", "network" }) do
            if package:config(name) then
                table.insert(defines, "BUILD_" .. name:upper() .. "_MODULE")
            end
        end
        for _, file in ipairs(os.files("src/SFML/Modules/*.cppm")) do
            local filename = path.filename(file)
            local skip = false
            for _, name in ipairs({ "Graphics", "Window", "Audio", "Network" }) do
                if filename == name .. ".cppm" and not package:config(name:lower()) then
                    skip = true
                end
            end
            if not skip then
                local modname
                for _, line in ipairs(io.readfile(file):split("\n", { plain = true })) do
                    modname = line:match("^%s*export%s+module%s+([%w_%.:]+)%s*;")
                    if modname then break end
                end
                assert(modname, "cannot find module name in " .. filename)
                os.cp(file, moddir)
                json.savefile(path.join(moddir, filename .. ".meta-info"), {
                    name = modname,
                    file = path.join("sfml", filename),
                    defines = defines
                })
            end
        end
    end)

    on_test(function (package)
        local configs = {languages = "c++20"}
        assert(package:check_cxxsnippets({ test = [[
            void test(int args, char** argv) {
                sf::Clock c;
                c.restart();
            }
        ]] }, { configs = configs, includes = "SFML/System.hpp" }))
        if package:config("graphics") then
            assert(package:check_cxxsnippets({ test = [[
                void test(const sf::Texture& texture, const sf::Color& color) {
                    sf::Sprite sprite(texture);
                    sprite.setColor(color);
                }
            ]] }, { configs = configs, includes = "SFML/Graphics.hpp" }))
        end
        local moddir = path.join(package:installdir(), "modules", "sfml")
        assert(os.isfile(path.join(moddir, "SFML.cppm")), "SFML.cppm was not installed")
        assert(os.isfile(path.join(moddir, "SFML.cppm.meta-info")), "module metadata is missing")
    end)

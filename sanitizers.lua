-- The following file has been clanked out by a clanker
--
-- sanitizers.lua
--
-- Adds:
--   * rule  "mode.sanitize" : a build mode that instruments targets with sanitizers
--   * option --sanitizers   : which sanitizers to enable (default: address,undefined)
--   * task  `xmake san`     : temporarily use sanitize mode, build and run,
--                             then restore the complete previous configuration
--
-- Usage in xmake.lua:
--
--   includes("sanitizers.lua")
--   add_rules("mode.debug", "mode.release", "mode.sanitize")
--
-- Then either:
--   xmake san                                  # build/run all targets, then restore the previous configuration
--   xmake san mytest                           # build/run one target, then restore the previous configuration
--   xmake san -s thread                        # use TSan for this run
--   xmake f -m sanitize --sanitizers=address,undefined,float && xmake run mytest
--   xmake san -k                               # leave the sanitize configuration in place afterwards
--
-- Supported sanitizers (comma-separated):
--   address    AddressSanitizer (+ pointer-compare/subtract checks; includes LeakSanitizer on Linux)
--   undefined  UBSan, aborts on first error (-fno-sanitize-recover)
--   leak       standalone LeakSanitizer
--   thread     ThreadSanitizer            (cannot be combined with address/leak/memory)
--   memory     MemorySanitizer, Clang only (cannot be combined with address/thread/leak;
--                                           needs an MSan-instrumented libc++ to be useful)
--   integer    Clang only: unsigned overflow, implicit conversion checks (noisy, opt-in)
--   float      float-divide-by-zero, float-cast-overflow

local DEFAULT_SANITIZERS = "address,undefined"

-- Keep the supported names in one place. The lookup set and display string
-- are derived from this array, so validation and help text cannot drift apart.
local KNOWN = {
    "address",
    "undefined",
    "leak",
    "thread",
    "memory",
    "integer",
    "float",
}

local KNOWN_SET = {}
for _, name in ipairs(KNOWN) do
    KNOWN_SET[name] = true
end

local KNOWN_STRING = table.concat(KNOWN, ",")

option("sanitizers")
    set_showmenu(true)
    set_default(DEFAULT_SANITIZERS)
    set_description(
        "Sanitizers for sanitize mode (comma-separated)",
        "  " .. KNOWN_STRING
    )
option_end()

rule("mode.sanitize")

    on_config(function (target)
        import("core.project.config")
        if config.mode() ~= "sanitize" then
            return
        end

        -- Parse the sanitizer list.
        local value = config.get("sanitizers")
        if type(value) ~= "string" or value == "" then
            value = DEFAULT_SANITIZERS
        end

        local san = {}
        for _, name in ipairs(value:split(",", {plain = true})) do
            name = name:trim()
            if name ~= "" then
                if not KNOWN_SET[name] then
                    raise("unknown sanitizer '%s' (known: %s)", name, KNOWN_STRING)
                end
                san[name] = true
            end
        end

        -- Toolchain detection.
        local is_msvc  = target:has_tool("cc", "cl") or target:has_tool("cxx", "cl")
        local is_clang = target:has_tool("cc", "clang") or target:has_tool("cxx", "clangxx")
                      or target:has_tool("cxx", "clang")

        -- Better stack traces and less optimisation noise.
        target:set("symbols", "debug")
        target:set("strip", "none")

        if is_msvc then
            -- MSVC only ships AddressSanitizer.
            for name in pairs(san) do
                if name ~= "address" then
                    raise("MSVC only supports the 'address' sanitizer (requested '%s')", name)
                end
            end
            if san.address then
                target:set("optimize", "none")
                target:add("cxflags", "/fsanitize=address", "/Zi")
                target:add("ldflags", "/INCREMENTAL:NO", {force = true})
                target:add("shflags", "/INCREMENTAL:NO", {force = true})
            end
            target:data_set("sanitizers", san)
            return
        end

        -- Check for incompatible sanitizers.
        if san.thread and (san.address or san.leak or san.memory) then
            raise("ThreadSanitizer cannot be combined with address/leak/memory")
        end
        if san.memory and (san.address or san.leak) then
            raise("MemorySanitizer cannot be combined with address/leak")
        end
        if (san.memory or san.integer) and not is_clang then
            raise("'memory' and 'integer' sanitizers require Clang")
        end

        -- Build the -fsanitize= list.
        local list = {}
        if san.address then
            table.insert(list, "address")
            table.insert(list, "pointer-compare")
            table.insert(list, "pointer-subtract")
        end
        if san.undefined then table.insert(list, "undefined") end
        if san.leak      then table.insert(list, "leak") end
        if san.thread    then table.insert(list, "thread") end
        if san.memory    then table.insert(list, "memory") end
        if san.integer   then table.insert(list, "integer") end
        if san.float then
            table.insert(list, "float-divide-by-zero")
            table.insert(list, "float-cast-overflow")
        end

        if #list > 0 then
            local fsan = "-fsanitize=" .. table.concat(list, ",")
            local flags = {
                fsan,
                "-fno-omit-frame-pointer",
                "-fno-optimize-sibling-calls",
            }

            -- Make UBSan findings fatal so tests actually fail.
            if san.undefined or san.integer or san.float then
                table.insert(flags, "-fno-sanitize-recover=all")
            end
            if san.memory then
                table.insert(flags, "-fsanitize-memory-track-origins=2")
            end

            target:set("optimize", "fast") -- -O1: usable speed, readable traces
            target:add("cxflags", flags, {force = true})
            target:add("ldflags", flags, {force = true})
            target:add("shflags", flags, {force = true})

            -- Add inexpensive hardening that is useful when testing.
            target:add("cxflags", "-fstack-protector-strong")
            target:add("defines", "_GLIBCXX_ASSERTIONS")

            -- Poison uninitialised locals (incompatible with MSan, so skip it there).
            if not san.memory and target:has_cxxflags("-ftrivial-auto-var-init=pattern") then
                target:add("cxflags", "-ftrivial-auto-var-init=pattern")
            end
        end

        target:data_set("sanitizers", san)
    end)

    -- Set sensible runtime options, but never override ones already exported by the user.
    before_run(function (target)
        local san = target:data("sanitizers")
        if not san then
            return
        end

        local function default(name, value)
            if not os.getenv(name) then
                os.setenv(name, value)
            end
        end

        local asan = "strict_string_checks=1:detect_stack_use_after_return=1" ..
                     ":check_initialization_order=1:strict_init_order=1" ..
                     ":detect_invalid_pointer_pairs=1:halt_on_error=1"
        if target:is_plat("linux") then
            asan = asan .. ":detect_leaks=1"
        end
        default("ASAN_OPTIONS",  asan)
        default("UBSAN_OPTIONS", "print_stacktrace=1:halt_on_error=1:report_error_type=1")
        default("TSAN_OPTIONS",  "second_deadlock_stack=1:halt_on_error=1")
        default("MSAN_OPTIONS",  "poison_in_dtor=1:halt_on_error=1")
    end)

rule_end()

task("san")
    set_category("plugin")
    set_menu {
        usage = "xmake san [options] [targets]",
        description = "Build with sanitizers and run the targets, then restore the previous configuration.",
        options = {
            {'s', "sanitizers", "kv", nil,
                "Sanitizers to enable (defaults to the configured value, or " .. DEFAULT_SANITIZERS .. ")."},
            {'k', "keep", "k", nil,
                "Keep the sanitize configuration afterwards."},
            {nil, "targets", "vs", nil,
                "Targets to run (default: all runnable targets)."}
        }
    }
    on_run(function ()
        import("core.base.option")
        import("core.project.config")

        -- Plugin tasks must load the project configuration before reading it.
        config.load()

        local xmake = os.programfile()
        local keep = option.get("keep")
        local configured_sanitizers = config.get("sanitizers")
        local sanitizers = option.get("sanitizers") or configured_sanitizers or DEFAULT_SANITIZERS
        local targets = option.get("targets")

        local function xm(...)
            os.execv(xmake, {...})
        end

        -- Export the entire configuration, not just the mode and sanitizer list.
        -- This preserves platform, architecture, toolchain, custom options, flags,
        -- and any other settings exactly as they were before this temporary run.
        local backup = os.tmpfile()
        local backup_created = false

        try
        {
            function ()
                if not keep then
                    xm("f", "--export=" .. backup)
                    backup_created = true
                end

                xm("f", "-m", "sanitize", "--sanitizers=" .. sanitizers)
                xm("build")

                if targets and #targets > 0 then
                    for _, name in ipairs(targets) do
                        xm("run", name)
                    end
                else
                    xm("run", "-a")
                end
            end,
            finally
            {
                function ()
                    if backup_created then
                        -- Ensure the temporary file is removed even if importing
                        -- the saved configuration itself fails.
                        try
                        {
                            function ()
                                xm("f", "--import=" .. backup)
                            end,
                            finally
                            {
                                function ()
                                    os.rm(backup)
                                end
                            }
                        }
                    else
                        -- Also remove a partially written export if exporting failed.
                        os.rm(backup)
                    end
                end
            }
        }
    end)
task_end()

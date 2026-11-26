// Copyright (C) 2026  ilobilo

import minesweeper;
import cli11;
import std;

namespace
{
    class CompactFormatter : public CLI::Formatter
    {
        public:
        std::string make_description(const CLI::App *app) const override
        {
            auto result = CLI::Formatter::make_description(app);
            if (!result.empty() && result.back() == '\n')
                result.pop_back();
            return result;
        }

        std::string make_usage(const CLI::App *app, std::string name) const override
        {
            auto result = CLI::Formatter::make_usage(app, name);
            if (!result.empty() && result.back() == '\n')
                result.pop_back();
            return result;
        }
    };
} // namespace

constexpr unsigned char builtin_theme[] {
#embed "builtin.zip"
};

int main(int argc, char **argv)
{
    using namespace minesweeper;

    CLI::App app { "A Minesweeper-like game written in C++", "minesweeper" };
    try {
        app.formatter(std::make_shared<CompactFormatter>());
        app.failure_message(CLI::FailureMessage::help);

        std::optional<fs::path> assets_dir { "./assets" };
        app.add_option("-a,--assets", *assets_dir, "Directory containing game themes")
            ->default_str(*assets_dir);

        std::string theme_path { "default" };
        app.add_option("-t,--theme", theme_path, "Name of the theme to use ({assets}/{theme}/)")
            ->default_str(theme_path);

        app.parse(argc, argv);

        if (!fs::is_directory(*assets_dir))
        {
            log::warn("Path '{}' does not exist or is not a directory. Using built-in theme", *assets_dir);
            assets_dir = std::nullopt;
        }

        auto fs = Filesystem::create(argv[0]);
        if (!fs)
        {
            log::error("{}", fs.error().what());
            return 1;
        }

        if (const auto res = fs->mount_builtin(std::as_bytes(std::span { builtin_theme })); !res)
        {
            log::error("{}", res.error().what());
            return 1;
        }

        if (assets_dir)
        {
            if (const auto res = fs->mount_theme(*assets_dir, theme_path); !res)
                log::warn("{}. Using built-in theme", res.error().what());
        }

        auto theme = Theme::create();
        if (!theme)
        {
            log::error("{}", theme.error().what());
            return 1;
        }

        Game game;
        game.run();
    }
    catch (const CLI::ParseError &err)
    {
        if (err.get_exit_code() == 0)
        {
            log::info("{}", app.help());
            return 0;
        }

        std::string_view message { err.what() };
        const auto prefix = app.get_name() + ": ";

        if (message.compare(0, prefix.size(), prefix) == 0)
            message.remove_prefix(prefix.size());

        log::error("{}\n\n{}", message, app.help());
        return err.get_exit_code();
    }
    catch (const std::exception &err)
    {
        log::error("{}", err.what());
        return 1;
    }
}

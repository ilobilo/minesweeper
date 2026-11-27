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

        fs::path assets_path { "./assets" };
        auto* assets_opt = app.add_option("-a,--assets", assets_path, "Directory containing game themes")
            ->default_str(assets_path);

        std::string theme_id { "default" };
        app.add_option("-t,--theme", theme_id, "Name of the theme to use ({assets}/{theme}/)")
            ->default_str(theme_id);

        app.parse(argc, argv);

        std::optional<fs::path> assets_dir;
        std::error_code ec;

        if (fs::is_directory(assets_path, ec))
        {
            assets_dir = std::move(assets_path);
        }
        else if (assets_opt->count() > 0)
        {
            log::error(
                "Assets path '{}' is not a valid directory{}",
                assets_path.string(), ec ? ": " + ec.message() : ""
            );
            return 1;
        }
        else
        {
            log::warn(
                "Default assets path '{}' does not exist or is not a directory. Using built-in theme",
                assets_path.string()
            );
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

        auto themes = ThemeManager::create(std::move(*fs), std::move(assets_dir), std::move(theme_id));
        if (!themes)
        {
            log::error("{}", ThemeManager::format_error(themes.error()));
            return 1;
        }

        Game game { std::move(*themes) };
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

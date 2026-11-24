// Copyright (C) 2026  ilobilo

import minesweeper;
import cli11;
import std;

namespace fs = std::filesystem;
namespace log = minesweeper::log;

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

int main(int argc, char **argv)
{
    CLI::App app { "A Minesweeper-like game written in C++", "minesweeper" };
    try {
        app.formatter(std::make_shared<CompactFormatter>());
        app.failure_message(CLI::FailureMessage::help);

        fs::path theme { "./assets/default" };
        app.add_option("-t,--theme", theme, "Directory containing theme assets")
            ->default_str(theme);

        app.parse(argc, argv);

        if (!fs::is_directory(theme))
            log::error("Path '{}' does not exist or is not a directory. Using built-in assets", theme);

        minesweeper::Game game;
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

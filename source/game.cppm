// Copyright (C) 2026  ilobilo

export module minesweeper:game;

import sfml;

import :board;

export namespace minesweeper
{
    class Game
    {
        static constexpr std::string_view window_title { "Minesweeper" };
        static constexpr std::size_t window_width = 600, window_height = 800;
        static constexpr std::size_t window_min_width = 300, window_min_height = 300;

        public:
        Game();

        void run();

        private:
        enum class State
        {
            menu,
            settings,
            playing,
            paused
        };

        State state_;
        std::optional<Board> board_;

        sf::RenderWindow window_;

        void handle_events();
        void update();
        void render();
    };
} // export namespace minesweeper

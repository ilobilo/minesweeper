// Copyright (C) 2026  ilobilo

export module minesweeper:game;

import sfml;

import :board;
import :theme;

export namespace minesweeper
{
    class Game
    {
        public:
        Game(ThemeManager themes);

        void run();

        private:
        enum class State
        {
            menu,
            settings,
            playing,
            paused
        };

        sf::RenderWindow window_;
        ThemeManager themes_;
        std::optional<Board> board_;
        State state_;

        void update_view(sf::Vector2u winsize);

        void handle_events();
        void update();
        void render();
    };
} // export namespace minesweeper

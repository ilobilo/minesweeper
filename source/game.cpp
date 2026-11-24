// Copyright (C) 2026  ilobilo

module minesweeper;

namespace minesweeper
{
    Game::Game() : window_ { sf::VideoMode({ window_width, window_height }), window_title }
    {
        window_.setMinimumSize(sf::Vector2u { window_min_width, window_min_height });
    }

    void Game::run()
    {
        while (window_.isOpen())
        {
            handle_events();
            update();
            render();
        }
    }

    void Game::handle_events()
    {
        while (const auto event = window_.pollEvent())
        {
            event->visit(util::overload {
                [&](const sf::Event::MouseButtonPressed &button) {
                    const bool reveal = button.button == sf::Mouse::Button::Left;
                    const bool flag = button.button == sf::Mouse::Button::Right;

                    if (reveal)
                        std::println("reveal");
                    if (flag)
                        std::println("flag");
                },
                [&](const sf::Event::KeyPressed &key) {
                    if (key.code == sf::Keyboard::Key::P)
                    {
                        std::println("pause");
                    }
                    else if (key.code == sf::Keyboard::Key::R)
                    {
                        std::println("restart");
                    }
                    else if (key.code == sf::Keyboard::Key::Escape)
                    {
                        std::println("exit to menu");
                    }
                },
                [&](const sf::Event::Closed &) {
                    window_.close();
                },
                [&](const auto &) { }
            });
        }
    }

    void Game::update()
    {
    }

    void Game::render()
    {
        window_.clear(sf::Color { 46, 46, 46 });



        window_.display();
    }
} // namespace minesweeper

// Copyright (C) 2026  ilobilo

module minesweeper;

namespace minesweeper
{
    namespace
    {
        constexpr std::string_view window_title { "Minesweeper" };

        constexpr std::size_t window_width = 600, window_height = 800;
        constexpr std::size_t window_min_width = 300, window_min_height = 300;
    } // namespace

    Game::Game(ThemeManager themes)
        : window_ { sf::VideoMode({ window_width, window_height }), window_title },
          themes_ { std::move(themes) }, board_ { }, state_ { State::menu }
    {
        window_.setMinimumSize(sf::Vector2u { window_min_width, window_min_height });
        update_view(window_.getSize());
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

    void Game::update_view(sf::Vector2u winsize)
    {
        // TODO
        log::info("view size: ({}, {})", winsize.x, winsize.y);
    }

    void Game::handle_events()
    {
        while (const auto event = window_.pollEvent())
        {
            event->visit(util::overload {
                [&](const sf::Event::Resized &resized) {
                    update_view(resized.size);
                },
                [&](const sf::Event::MouseButtonPressed &button) {
                    const auto world_pos = window_.mapPixelToCoords(button.position);
                    if (button.button == sf::Mouse::Button::Left)
                        log::info("reveal at {}, {}", world_pos.x, world_pos.y);
                    else if (button.button == sf::Mouse::Button::Right)
                        log::info("flag at {}, {}", world_pos.x, world_pos.y);
                },
                [&](const sf::Event::KeyPressed &key) {
                    if (key.code == sf::Keyboard::Key::P)
                    {
                        log::info("pause");
                    }
                    else if (key.code == sf::Keyboard::Key::R)
                    {
                        log::info("restart");
                    }
                    else if (key.code == sf::Keyboard::Key::Escape)
                    {
                        log::info("exit to menu");
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

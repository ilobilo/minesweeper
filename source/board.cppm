// Copyright (C) 2026  ilobilo

export module minesweeper:board;

import std;

export namespace minesweeper
{
    enum class Mark : std::uint8_t
    {
        none,
        flagged,
        question
    };

    struct Cell
    {
        bool has_mine : 1 = false;
        bool revealed : 1 = false;
        Mark mark : 2 = Mark::none;
        std::uint8_t adjacent : 4 = 0;

        [[nodiscard]] constexpr bool is_flagged() const noexcept { return mark == Mark::flagged; }
        [[nodiscard]] constexpr bool is_question() const noexcept { return mark == Mark::question; }
    };
    static_assert(sizeof(Cell) == 1);

    class Board
    {
        public:
        enum class Error
        {
            invalid_width,
            invalid_height,
            invalid_mine_count
        };

        enum class State
        {
            playing,
            won,
            lost
        };

        enum class Result
        {
            none,
            revealed,
            hit_mine,
            flagged,
            questioned,
            unmarked
        };

        static constexpr std::size_t min_width = 4, min_height = 4;
        static constexpr std::size_t max_width = 64, max_height = 64;
        static constexpr std::size_t min_mines = 1;

        std::size_t max_mines() const noexcept { return max_mines(width_, height_); }
        std::size_t width() const noexcept { return width_; }
        std::size_t height() const noexcept { return height_; }
        State state() const noexcept { return state_; }

        auto at(this auto &self, std::size_t x, std::size_t y) noexcept
        {
            using Ret = std::optional<decltype(std::forward_like<decltype(self)>(std::declval<Cell &>()))>;
            if (x >= self.width_ || y >= self.height_)
                return Ret { };
            return Ret { self.cells_[self.idx(x, y)] };
        }

        void reset();

        Result reveal(std::size_t x, std::size_t y);
        Result flag_toggle(std::size_t x, std::size_t y) noexcept;

        static std::expected<Board, Error> create(
            std::size_t width, std::size_t height, std::size_t mine_count,
            std::random_device::result_type seed = std::random_device { } ()
        );

        private:
        static constexpr std::array<std::pair<int, int>, 8> directions { {
            { -1, -1 }, { 0, -1 }, { 1, -1 },
            { -1,  0 },            { 1,  0 },
            { -1,  1 }, { 0,  1 }, { 1,  1 }
        } };

        static constexpr std::size_t max_mines(std::size_t width, std::size_t height) noexcept
        {
            return (width * height) / 4;
        }

        std::size_t width_, height_;
        std::size_t mine_count_, revealed_count_;
        std::vector<Cell> cells_;

        std::vector<std::size_t> positions_;
        std::mt19937_64 rng_;

        State state_;

        std::size_t idx(std::size_t x, std::size_t y) const noexcept
        {
            return y * width_ + x;
        }

        Result reveal_cell(std::size_t x, std::size_t y) noexcept;
        Result flood_reveal(std::size_t x, std::size_t y);

        using Callback = std::function_ref<bool (Cell &, std::size_t, std::size_t)>;
        void iterate_adjacent(std::size_t cx, std::size_t cy, Callback func);

        Board(std::size_t width, std::size_t height, std::size_t mine_count, std::random_device::result_type seed);
    };
} // export namespace minesweeper

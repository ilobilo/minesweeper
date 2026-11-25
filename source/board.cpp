// Copyright (C) 2026  ilobilo

module minesweeper;

namespace minesweeper
{
    std::optional<Cell &> Board::at(std::size_t x, std::size_t y) noexcept
    {
        if (x >= width_ || y >= height_)
            return std::nullopt;
        return cells_[idx(x, y)];
    }

    void Board::reset()
    {
        if (positions_.empty())
            positions_.resize(cells_.size());
        else
            std::ranges::fill(cells_, Cell { });

        std::ranges::iota(positions_, 0);
        std::ranges::shuffle(positions_, rng_);

        for (const auto i : positions_ | std::views::take(mine_count_))
            cells_[i].has_mine = true;

        for (auto [i, cell] : cells_ | std::views::enumerate)
        {
            const auto y = i / width_, x = i - y * width_;
            iterate_adjacent(x, y, [&cell](Cell &adjacent, auto, auto) {
                if (adjacent.has_mine)
                    cell.adjacent++;
                return true;
            });
        }

        revealed_count_ = 0;
        state_ = State::playing;
    }

    auto Board::reveal(std::size_t x, std::size_t y) -> Result
    {
        if (state_ != State::playing)
            return Result::none;

        const auto cell = at(x, y);
        if (!cell || cell->is_flagged())
            return Result::none;

        if (!cell->revealed)
            return flood_reveal(x, y);

        std::uint8_t flagged = 0;
        iterate_adjacent(x, y, [&flagged](Cell &adjacent, auto, auto) {
            flagged += adjacent.is_flagged();
            return true;
        });

        if (cell->adjacent != flagged)
            return Result::none;

        auto result = Result::none;
        iterate_adjacent(x, y, [&](Cell &adjacent, std::size_t cx, std::size_t cy) {
            if (adjacent.revealed || adjacent.is_flagged())
                return true;

            const auto res = flood_reveal(cx, cy);
            if (res == Result::hit_mine)
            {
                result = res;
                return false;
            }
            if (result == Result::none)
                result = res;

            return state_ == State::playing;
        });
        return result;
    }

    auto Board::flag_toggle(std::size_t x, std::size_t y) noexcept -> Result
    {
        if (state_ != State::playing)
            return Result::none;

        const auto cell = at(x, y);
        if (!cell || cell->revealed)
            return Result::none;

        switch (cell->mark)
        {
            case Mark::none:
                cell->mark = Mark::flagged;
                return Result::flagged;
            case Mark::flagged:
                cell->mark = Mark::question;
                return Result::questioned;
            case Mark::question:
                cell->mark = Mark::none;
                return Result::unmarked;
        }
        std::unreachable();
    }

    auto Board::reveal_cell(std::size_t x, std::size_t y) noexcept -> Result
    {
        auto cell = at(x, y);
        if (!cell || cell->revealed || cell->is_flagged())
            return Result::none;

        cell->mark = Mark::none;
        cell->revealed = true;

        if (cell->has_mine)
        {
            state_ = State::lost;
            return Result::hit_mine;
        }

        revealed_count_++;
        if (mine_count_ + revealed_count_ == cells_.size())
            state_ = State::won;

        return Result::revealed;
    }

    auto Board::flood_reveal(std::size_t x, std::size_t y) -> Result
    {
        const auto first = reveal_cell(x, y);
        if (first != Result::revealed)
            return first;

        const auto start = at(x, y);
        if (!start || start->adjacent != 0)
            return first;

        std::vector<std::pair<std::size_t, std::size_t>> pending { { x, y } };
        while (!pending.empty())
        {
            const auto [cx, cy] = pending.back();
            pending.pop_back();

            iterate_adjacent(cx, cy, [&](Cell &adjacent, std::size_t nx, std::size_t ny) {
                if (adjacent.has_mine || adjacent.revealed || adjacent.is_flagged())
                    return true;

                if (reveal_cell(nx, ny) == Result::revealed && adjacent.adjacent == 0)
                    pending.emplace_back(nx, ny);

                return true;
            });
        }
        return Result::revealed;
    }

    void Board::iterate_adjacent(std::size_t x, std::size_t y, Callback func)
    {
        const std::ptrdiff_t cx = x, cy = y;
        for (const auto [x, y] : directions | std::views::transform([cx, cy](const auto &dir) {
            return std::make_pair(cx + dir.first, cy + dir.second);
        }))
        {
            if (x < 0 || y < 0)
                continue;

            if (const auto adjacent = at(x, y))
            {
                if (!func(*adjacent, x, y))
                    break;
            }
        }
    }

    auto Board::create(
        std::size_t width, std::size_t height, std::size_t mine_count,
        std::random_device::result_type seed
    ) -> std::expected<Board, Error>
    {
        if (width < min_width || width > max_width)
            return std::unexpected { Error::invalid_width };
        if (height < min_height || height > max_height)
            return std::unexpected { Error::invalid_height };

        if (mine_count < min_mines || mine_count > max_mines(width, height))
            return std::unexpected { Error::invalid_mine_count };

        return Board { width, height, mine_count, seed };
    }

    Board::Board(std::size_t width, std::size_t height, std::size_t mine_count, std::random_device::result_type seed)
        : width_ { width }, height_ { height }, mine_count_ { mine_count }, revealed_count_ { 0 },
          cells_(width * height), positions_ { }, rng_ { seed }, state_ { State::playing }
    { reset(); }
} // namespace minesweeper

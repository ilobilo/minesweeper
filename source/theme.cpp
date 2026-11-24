// Copyright (C) 2026  ilobilo

module minesweeper;

import nlohmann.json;

namespace minesweeper
{
    namespace
    {
        using json = nlohmann::json;
        constexpr std::size_t supported_version = 1;

        template<typename Type>
        auto require(const json &obj, std::string_view key) -> std::expected<Type, Theme::Error>
        {
            const auto it = obj.find(key);
            if (it == obj.cend())
                return std::unexpected { Theme::Error::missing_field };

            if constexpr (std::same_as<Type, bool>)
            {
                if (!it->is_boolean())
                    return std::unexpected { Theme::Error::invalid_value };
            }
            else if constexpr (std::signed_integral<Type>)
            {
                if (!it->is_number_integer())
                    return std::unexpected { Theme::Error::invalid_value };
                const auto val = it->get<std::int64_t>();
                if (!std::in_range<Type>(val))
                    return std::unexpected { Theme::Error::invalid_value };
                return static_cast<Type>(val);
            }
            else if constexpr (std::unsigned_integral<Type>)
            {
                if (!it->is_number_unsigned())
                    return std::unexpected { Theme::Error::invalid_value };
                const auto val = it->get<std::uint64_t>();
                if (!std::in_range<Type>(val))
                    return std::unexpected { Theme::Error::invalid_value };
                return static_cast<Type>(val);
            }
            else if constexpr (std::floating_point<Type>)
            {
                if (!it->is_number())
                    return std::unexpected { Theme::Error::invalid_value };
            }
            else if constexpr (std::same_as<Type, std::string>)
            {
                if (!it->is_string())
                    return std::unexpected { Theme::Error::invalid_value };
            }
            else static_assert(false, "Unsupported JSON field type");

            return it->get<Type>();
        }

        auto require_object(const json &obj, std::string_view key)
            -> util::expected_ref<const json, Theme::Error>
        {
            const auto it = obj.find(key);
            if (it == obj.cend())
                return std::unexpected { Theme::Error::missing_field };
            if (!it->is_object())
                return std::unexpected { Theme::Error::invalid_value };
            return *it;
        }

        auto require_array(const json &obj, std::string_view key)
            -> util::expected_ref<const json, Theme::Error>
        {
            const auto it = obj.find(key);
            if (it == obj.cend())
                return std::unexpected { Theme::Error::missing_field };
            if (!it->is_array())
                return std::unexpected { Theme::Error::invalid_value };
            return *it;
        }

        std::expected<sf::IntRect, Theme::Error> parse_rect(const json &node)
        {
            if (node.size() != 4)
                return std::unexpected { Theme::Error::invalid_value };

            std::array<int, 4> vals { };
            for (const auto [i, entry] : node | std::views::enumerate)
            {
                if (!entry.is_number_unsigned())
                    return std::unexpected { Theme::Error::invalid_value };

                const auto val = entry.get<std::size_t>();
                if (!std::in_range<int>(val))
                    return std::unexpected { Theme::Error::invalid_value };

                vals[i] = val;
            }

            if (vals[2] <= 0 || vals[3] <= 0)
                return std::unexpected { Theme::Error::invalid_value };

            return sf::IntRect { { vals[0], vals[1] }, { vals[2], vals[3] } };
        }

        template<typename RegionEnum>
        std::expected<Atlas<RegionEnum>, Theme::Error> parse_atlas(const json &node, Assets &assets)
        {
            const auto file = require<std::string>(node, "file");
            if (!file)
                return std::unexpected { file.error() };

            const auto smooth = require<bool>(node, "smooth");
            if (!smooth)
                return std::unexpected { smooth.error() };

            const auto regions = require_object(node, "regions");
            if (!regions)
                return std::unexpected { regions.error() };

            auto asset = assets.textures().load(*file);
            if (!asset)
                return std::unexpected { Theme::Error::asset_load_failed };

            asset->get()->setSmooth(*smooth);

            std::array<sf::IntRect, magic_enum::enum_count<RegionEnum>()> rects { };

            std::expected<void, Theme::Error> status;
            magic_enum::enum_for_each<RegionEnum>([&](auto val) {
                if (!status)
                    return;

                constexpr RegionEnum entry = val;
                constexpr auto key = magic_enum::enum_name<entry>();

                auto region_obj = require_array(*regions, key);
                if (!region_obj)
                {
                    status = std::unexpected { region_obj.error() };
                    return;
                }

                auto rect = parse_rect(*region_obj);
                if (!rect)
                {
                    status = std::unexpected { rect.error() };
                    return;
                }

                rects[std::to_underlying(entry)] = *rect;
            });

            if (!status)
                return std::unexpected { status.error() };

            for (const auto texture_size = asset->get()->getSize(); const auto &rect : rects)
            {
                const auto right = static_cast<std::uint64_t>(rect.position.x) + rect.size.x;
                const auto bottom = static_cast<std::uint64_t>(rect.position.y) + rect.size.y;
                if (right > texture_size.x || bottom > texture_size.y)
                    return std::unexpected { Theme::Error::invalid_value };
            }

            return Atlas<RegionEnum> { *asset, rects };
        }

        std::expected<sf::Color, Theme::Error> parse_color(std::string_view hex)
        {
            if (!hex.starts_with('#') || (hex.size() != 7 && hex.size() != 9))
                return std::unexpected { Theme::Error::invalid_value };

            const auto parse_byte = [&](std::size_t offset) -> std::optional<std::uint8_t>
            {
                std::uint8_t byte { };
                const auto *begin = hex.data() + offset;
                const auto *end = begin + 2;
                const auto [ptr, ec] = std::from_chars(begin, end, byte, 16);
                if (ec != std::errc { } || ptr != end)
                    return std::nullopt;
                return byte;
            };

            const auto r = parse_byte(1);
            const auto g = parse_byte(3);
            const auto b = parse_byte(5);
            const auto a = hex.size() == 9
                ? parse_byte(7)
                : std::optional<std::uint8_t> { 0xFF };

            if (!r || !g || !b || !a)
                return std::unexpected { Theme::Error::invalid_value };

            return sf::Color { *r, *g, *b, *a };
        }
    } // namespace

    auto Theme::create() -> std::expected<Theme, Error>
    {
        auto res = InputStream::create("theme.json");
        if (!res)
            return std::unexpected { Error::manifest_not_found };
        auto stream = std::move(*res);

        const auto size = stream->getSize();
        if (!size)
            return std::unexpected { Error::invalid_manifest };

        std::string raw(*size, '\0');
        if (const auto read = stream->read(raw.data(), raw.size()); !read || *read != raw.size())
            return std::unexpected { Error::invalid_manifest };

        const auto manifest = json::parse(raw, nullptr, false);
        if (manifest.is_discarded() || !manifest.is_object())
            return std::unexpected { Error::invalid_manifest };

        const auto version = require<std::size_t>(manifest, "version");
        if (!version)
            return std::unexpected { version.error() };
        if (*version != supported_version)
            return std::unexpected { Error::unsupported_version };

        auto name = require<std::string>(manifest, "name");
        if (!name)
            return std::unexpected { name.error() };

        auto assets = std::make_unique<Assets>();

        Atlases::Pending atlases;
        Metrics::Pending metrics;
        Fonts::Pending fonts;
        Colors::Pending colors;
        Sounds::Pending sounds;

        for (const auto &[value, section_name] : magic_enum::enum_entries<Section>())
        {
            const auto section_res = require_object(manifest, section_name);
            if (!section_res)
                return std::unexpected { section_res.error() };

            const auto &section = section_res->get();
            switch (value)
            {
                case Section::atlases:
                {
                    std::expected<void, Error> status;
                    magic_enum::enum_for_each<AtlasRegion>([&](auto val) {
                        if (!status)
                            return;

                        constexpr AtlasRegion region = val;
                        constexpr auto key = magic_enum::enum_name<region>();

                        auto region_obj = require_object(section, key);
                        if (!region_obj)
                        {
                            status = std::unexpected { region_obj.error() };
                            return;
                        }

                        auto atlas = parse_atlas<AtlasRegions<region>>(*region_obj, *assets);
                        if (!atlas)
                        {
                            status = std::unexpected { atlas.error() };
                            return;
                        }

                        atlases.emplace(std::move(*atlas));
                    });

                    if (!status)
                        return std::unexpected { status.error() };
                    break;
                }
                case Section::metrics:
                {
                    for (const auto &[metric, key] : magic_enum::enum_entries<Metric>())
                    {
                        auto val = require<float>(section, key);
                        if (!val)
                            return std::unexpected { val.error() };

                        if (!std::isfinite(*val) || (metric == Metric::tile_scale ? *val <= 0.0f : *val < 0.0f))
                            return std::unexpected { Error::invalid_value };

                        metrics.emplace(metric, *val);
                    }
                    break;
                }
                case Section::fonts:
                {
                    for (const auto &[type, key] : magic_enum::enum_entries<Font::Type>())
                    {
                        auto font_obj = require_object(section, key);
                        if (!font_obj)
                            return std::unexpected { font_obj.error() };

                        auto file = require<std::string>(*font_obj, "file");
                        if (!file)
                            return std::unexpected { file.error() };

                        auto size = require<std::uint32_t>(*font_obj, "size");
                        if (!size)
                            return std::unexpected { size.error() };
                        if (*size == 0)
                            return std::unexpected { Error::invalid_value };

                        auto smooth = require<bool>(*font_obj, "smooth");
                        if (!smooth)
                            return std::unexpected { smooth.error() };

                        auto asset = assets->fonts().load(*file);
                        if (!asset)
                            return std::unexpected { Error::asset_load_failed };

                        asset->get()->setSmooth(*smooth);

                        fonts.emplace(type, Font { .asset = *asset, .size = *size });
                    }
                    break;
                }
                case Section::colors:
                {
                    for (const auto &[color, key] : magic_enum::enum_entries<Color>())
                    {
                        auto str = require<std::string>(section, key);
                        if (!str)
                            return std::unexpected { str.error() };

                        auto val = parse_color(*str);
                        if (!val)
                            return std::unexpected { val.error() };

                        colors.emplace(color, *val);
                    }
                    break;
                }
                case Section::sounds:
                {
                    for (const auto &[sound, key] : magic_enum::enum_entries<Sound>())
                    {
                        auto str = require<std::string>(section, key);
                        if (!str)
                            return std::unexpected { str.error() };

                        auto asset = assets->sounds().load(*str);
                        if (!asset)
                            return std::unexpected { Error::asset_load_failed };

                        sounds.emplace(sound, *asset);
                    }
                    break;
                }
            }
        }

        return Theme {
            std::move(assets),
            std::move(*name),
            std::move(atlases).build(),
            std::move(metrics).build(),
            std::move(fonts).build(),
            std::move(colors).build(),
            std::move(sounds).build()
        };
    }
} // namespace minesweeper

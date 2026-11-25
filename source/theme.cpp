// Copyright (C) 2026  ilobilo

module minesweeper;

import nlohmann.json;

namespace minesweeper
{
    namespace
    {
        using json = nlohmann::json;

        constexpr std::size_t supported_version = 1;

        std::string_view error_name(Theme::Error::Type error) noexcept
        {
            switch (error)
            {
                case Theme::Error::manifest_not_found:
                    return "Theme is missing 'theme.json' manifest file";
                case Theme::Error::invalid_manifest:
                    return "Invalid theme manifest 'theme.json'";
                case Theme::Error::unsupported_version:
                    return "Unsupported theme manifest version";
                case Theme::Error::missing_field:
                    return "Theme manifest is missing required field";
                case Theme::Error::invalid_value:
                    return "Invalid value in theme manifest";
                case Theme::Error::asset_not_found:
                    return "Theme asset not found";
                case Theme::Error::invalid_asset:
                    return "Failed to decode theme asset";
            }
            std::unreachable();
        }

        auto theme_error(Theme::Error::Type error, std::string detail = { }) noexcept
        {
            return std::unexpected { Theme::Error { error, std::move(detail) } };
        }

        auto missing_field(std::string_view path)
        {
            return theme_error(Theme::Error::missing_field, std::format("'{}'", path));
        }

        auto invalid_value(std::string_view path, std::string_view reason)
        {
            return theme_error(Theme::Error::invalid_value, std::format("'{}' ({})", path, reason));
        }

        auto asset_error(AssetError error, std::string_view asset_path, std::string_view field_path)
        {
            const auto type = error == AssetError::asset_not_found
                ? Theme::Error::asset_not_found
                : Theme::Error::invalid_asset;
            return theme_error(type, std::format("'{}' (referenced by '{}')", asset_path, field_path));
        }

        template<typename Type>
        auto require(const json &obj, std::string_view key, std::string_view path)
            -> std::expected<Type, Theme::Error>
        {
            const auto it = obj.find(key);
            if (it == obj.cend())
                return missing_field(path);

            if constexpr (std::same_as<Type, bool>)
            {
                if (!it->is_boolean())
                    return invalid_value(path, "expected boolean");
            }
            else if constexpr (std::signed_integral<Type>)
            {
                if (!it->is_number_integer())
                    return invalid_value(path, "expected integer");
                const auto val = it->get<std::int64_t>();
                if (!std::in_range<Type>(val))
                    return invalid_value(path, "integer out of range");
                return static_cast<Type>(val);
            }
            else if constexpr (std::unsigned_integral<Type>)
            {
                if (!it->is_number_unsigned())
                    return invalid_value(path, "expected non-negative integer");
                const auto val = it->get<std::uint64_t>();
                if (!std::in_range<Type>(val))
                    return invalid_value(path, "integer out of range");
                return static_cast<Type>(val);
            }
            else if constexpr (std::floating_point<Type>)
            {
                if (!it->is_number())
                    return invalid_value(path, "expected number");
            }
            else if constexpr (std::same_as<Type, std::string>)
            {
                if (!it->is_string())
                    return invalid_value(path, "expected string");
            }
            else static_assert(false, "Unsupported JSON field type");

            return it->get<Type>();
        }

        auto require_object(const json &obj, std::string_view key, std::string_view path)
            -> util::expected_ref<const json, Theme::Error>
        {
            const auto it = obj.find(key);
            if (it == obj.cend())
                return missing_field(path);
            if (!it->is_object())
                return invalid_value(path, "expected JSON object");
            return *it;
        }

        auto require_array(const json &obj, std::string_view key, std::string_view path)
            -> util::expected_ref<const json, Theme::Error>
        {
            const auto it = obj.find(key);
            if (it == obj.cend())
                return missing_field(path);
            if (!it->is_array())
                return invalid_value(path, "expected JSON array");
            return *it;
        }

        auto parse_rect(const json &obj, std::string_view key, std::string_view path)
            -> std::expected<sf::IntRect, Theme::Error>
        {
            const auto res = require_array(obj, key, path);
            if (!res)
                return std::unexpected { res.error() };

            const auto &node = res->get();
            if (node.size() != 4)
                return invalid_value(path, std::format("expected 4 elements, got {}", node.size()));

            std::array<int, 4> vals { };
            for (const auto [i, entry] : node | std::views::enumerate)
            {
                if (!entry.is_number_unsigned())
                    return invalid_value(path, std::format("element {} must be a non-negative integer", i));

                const auto val = entry.get<std::size_t>();
                if (!std::in_range<int>(val))
                    return invalid_value(path, std::format("element {} is out of range", i));

                vals[i] = static_cast<int>(val);
            }

            if (vals[2] == 0 || vals[3] == 0)
                return invalid_value(path, "width and height must be greater than 0");

            return sf::IntRect { { vals[0], vals[1] }, { vals[2], vals[3] } };
        }

        template<typename RegionEnum>
        auto parse_atlas(const json &obj, std::string_view key, std::string_view path, Assets &assets)
            -> std::expected<Atlas<RegionEnum>, Theme::Error>
        {
            const auto res = require_object(obj, key, path);
            if (!res)
                return std::unexpected { res.error() };

            const auto &node = res->get();

            const auto file_path = std::format("{}.file", path);
            const auto file = require<std::string>(node, "file", file_path);
            if (!file)
                return std::unexpected { file.error() };
            if (file->empty())
                return invalid_value(file_path, "must not be empty");

            const auto smooth = require<bool>(node, "smooth", std::format("{}.smooth", path));
            if (!smooth)
                return std::unexpected { smooth.error() };

            const auto regions_path = std::format("{}.regions", path);
            const auto regions = require_object(node, "regions", regions_path);
            if (!regions)
                return std::unexpected { regions.error() };

            auto asset = assets.textures().load(*file);
            if (!asset)
                return asset_error(asset.error(), *file, file_path);

            asset->get()->setSmooth(*smooth);

            const auto texture_size = asset->get()->getSize();

            std::array<sf::IntRect, magic_enum::enum_count<RegionEnum>()> rects { };
            std::expected<void, Theme::Error> status;

            magic_enum::enum_for_each<RegionEnum>([&](auto val) {
                if (!status)
                    return;

                constexpr RegionEnum entry = val;
                constexpr auto region_key = magic_enum::enum_name<entry>();
                const auto region_path = std::format("{}.{}", regions_path, region_key);

                auto rect = parse_rect(*regions, region_key, region_path);
                if (!rect)
                {
                    status = std::unexpected { rect.error() };
                    return;
                }

                const auto right = static_cast<std::uint64_t>(rect->position.x) + rect->size.x;
                const auto bottom = static_cast<std::uint64_t>(rect->position.y) + rect->size.y;
                if (right > texture_size.x || bottom > texture_size.y)
                {
                    status = invalid_value(
                        region_path,
                        std::format(
                            "region [{}, {}, {}, {}] exceeds texture bounds {}x{}",
                            rect->position.x, rect->position.y, rect->size.x, rect->size.y,
                            texture_size.x, texture_size.y
                        )
                    );
                    return;
                }

                rects[std::to_underlying(entry)] = *rect;
            });

            if (!status)
                return std::unexpected { status.error() };

            return Atlas<RegionEnum> { *asset, rects };
        }

        auto parse_color(const json &section, std::string_view key, std::string_view path)
            -> std::expected<sf::Color, Theme::Error>
        {
            const auto res = require<std::string>(section, key, path);
            if (!res)
                return std::unexpected { res.error() };

            const auto &hex = *res;

            if (!hex.starts_with('#') || (hex.size() != 7 && hex.size() != 9))
                return invalid_value(path, std::format("got '{}', expected '#RRGGBB' or '#RRGGBBAA' hex string", hex));

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
                return invalid_value(path, std::format("got '{}', contains non-hexadecimal digits", hex));

            return sf::Color { *r, *g, *b, *a };
        }
    } // namespace

    [[nodiscard]] std::string Theme::Error::what() const
    {
        std::string err { error_name(error) };
        if (detail.empty())
            return err;
        return std::format("{}: {}", std::move(err), detail);
    }

    auto Theme::create() -> std::expected<Theme, Error>
    {
        auto res = InputStream::create("theme.json");
        if (!res)
            return theme_error(Error::manifest_not_found);
        auto stream = std::move(*res);

        const auto size = stream->getSize();
        if (!size)
            return theme_error(Error::invalid_manifest, "could not determine file size");

        std::string raw(*size, '\0');
        if (const auto read = stream->read(raw.data(), raw.size()); !read || *read != raw.size())
            return theme_error(Error::invalid_manifest, "could not read file contents");

        const auto manifest = json::parse(raw, nullptr, false);
        if (manifest.is_discarded())
            return theme_error(Error::invalid_manifest, "JSON syntax error");
        if (!manifest.is_object())
            return theme_error(Error::invalid_manifest, "root value must be a JSON object");

        const auto version = require<std::size_t>(manifest, "version", "version");
        if (!version)
            return std::unexpected { version.error() };
        if (*version != supported_version)
            return theme_error(
                Error::unsupported_version,
                std::format("got {}, expected {}", *version, supported_version)
            );

        auto name = require<std::string>(manifest, "name", "name");
        if (!name)
            return std::unexpected { name.error() };
        if (name->empty())
            return invalid_value("name", "must not be empty");

        auto assets = std::make_unique<Assets>();

        Atlases::Pending atlases;
        Metrics::Pending metrics;
        Fonts::Pending fonts;
        Colors::Pending colors;
        Sounds::Pending sounds;

        for (const auto &[value, section_name] : magic_enum::enum_entries<Section>())
        {
            const auto section_res = require_object(manifest, section_name, section_name);
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

                        auto atlas = parse_atlas<AtlasRegions<region>>(
                            section, key, std::format("{}.{}", section_name, key), *assets
                        );
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
                        const auto metric_path = std::format("{}.{}", section_name, key);
                        auto val = require<float>(section, key, metric_path);
                        if (!val)
                            return std::unexpected { val.error() };

                        if (!std::isfinite(*val))
                            return invalid_value(metric_path, "must be a finite number");
                        if (metric == Metric::tile_scale ? *val <= 0.0f : *val < 0.0f)
                        {
                            return invalid_value(
                                metric_path,
                                metric == Metric::tile_scale
                                    ? "must be greater than 0"
                                    : "must be non-negative"
                            );
                        }

                        metrics.emplace(metric, *val);
                    }
                    break;
                }
                case Section::fonts:
                {
                    for (const auto &[type, key] : magic_enum::enum_entries<Font::Type>())
                    {
                        const auto font_path = std::format("{}.{}", section_name, key);
                        auto font_obj = require_object(section, key, font_path);
                        if (!font_obj)
                            return std::unexpected { font_obj.error() };

                        const auto file_path = std::format("{}.file", font_path);
                        auto file = require<std::string>(*font_obj, "file", file_path);
                        if (!file)
                            return std::unexpected { file.error() };
                        if (file->empty())
                            return invalid_value(file_path, "must not be empty");

                        const auto size_path = std::format("{}.size", font_path);
                        auto size = require<std::uint32_t>(*font_obj, "size", size_path);
                        if (!size)
                            return std::unexpected { size.error() };
                        if (*size == 0)
                            return invalid_value(size_path, "must be greater than 0");

                        auto smooth = require<bool>(*font_obj, "smooth", std::format("{}.smooth", font_path));
                        if (!smooth)
                            return std::unexpected { smooth.error() };

                        auto asset = assets->fonts().load(*file);
                        if (!asset)
                            return asset_error(asset.error(), *file, file_path);

                        asset->get()->setSmooth(*smooth);

                        fonts.emplace(type, Font { .asset = *asset, .size = *size });
                    }
                    break;
                }
                case Section::colors:
                {
                    for (const auto &[color, key] : magic_enum::enum_entries<Color>())
                    {
                        auto val = parse_color(section, key, std::format("{}.{}", section_name, key));
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
                        const auto sound_path = std::format("{}.{}", section_name, key);
                        auto str = require<std::string>(section, key, sound_path);
                        if (!str)
                            return std::unexpected { str.error() };
                        if (str->empty())
                            return invalid_value(sound_path, "must not be empty");

                        auto asset = assets->sounds().load(*str);
                        if (!asset)
                            return asset_error(asset.error(), *str, sound_path);

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

// Copyright (C) 2026  ilobilo

export module minesweeper:theme;

import magic_enum;
import std;

import :assets;

export namespace minesweeper
{
    enum class AtlasRegion : std::size_t
    {
        tiles,
        ui
    };

    enum class TilesRegion : std::size_t
    {
        covered, pressed, revealed, question,
        mine, exploded_mine, flag, wrong_flag,
        number_1, number_2, number_3, number_4,
        number_5, number_6, number_7, number_8
    };

    enum class UiRegion : std::size_t
    {
        restart, settings, close, pause,
        sound_on, sound_off, checkbox_unchecked, checkbox_checked,
        arrow_left, arrow_right, arrow_up, arrow_down,
        restart_caution, restart_win, restart_lose, trophy
    };

    template<AtlasRegion Region, typename ...Types>
        requires (sizeof...(Types) == magic_enum::enum_count<AtlasRegion>())
              && (static_cast<std::size_t>(Region) < sizeof...(Types))
    using AtlasRegionElement = Types...[static_cast<std::size_t>(Region)];

    template<AtlasRegion Region>
    using AtlasRegions = AtlasRegionElement<Region, TilesRegion, UiRegion>;

    // god i love this god-forsaken language
    static_assert([]<std::size_t ...Is>(std::index_sequence<Is...>) {
        constexpr auto norm = [](std::string_view str) {
            return str | std::views::filter([](char chr) { return chr != '_'; })
                       | std::views::transform([](char chr) -> char { return chr | 32; });
        };
        return ([&](auto idx) {
            constexpr auto reg = magic_enum::enum_values<AtlasRegion>()[idx];
            constexpr auto enum_name = magic_enum::enum_name<reg>();
            constexpr auto type_name = magic_enum::enum_type_name<AtlasRegions<reg>>();
            constexpr std::string_view suffix { "Region" };
            return std::to_underlying(reg) == idx && type_name.ends_with(suffix)
                && std::ranges::equal(norm(enum_name), norm(type_name.substr(0, type_name.size() - suffix.size())));
        } (std::integral_constant<std::size_t, Is> { }) && ...);
    } (std::make_index_sequence<magic_enum::enum_count<AtlasRegion>()> { }));

    enum class Metric : std::size_t
    {
        tile_scale,
        hud_height,
        board_padding,
        bevel_width
    };

    struct Font
    {
        enum class Type : std::size_t
        {
            menu,
            game
        };
        using enum Type;

        std::reference_wrapper<const Asset<sf::Font>> asset;
        std::uint32_t size;

        [[nodiscard]] sf::Text text(const sf::String &str = "") const
        {
            return asset.get().instantiate(str, size);
        }
    };

    enum class Color : std::size_t
    {
        background,
        panel,
        bevel_light,
        bevel_dark,
        text,
        counter_bg,
        counter_text,
        button,
        button_hover,
        button_pressed,
        accent,
        overlay
    };

    enum class Sound : std::size_t
    {
        reveal,
        flag,
        unflag,
        win,
        lose
    };

    template<typename Region>
    class Atlas
    {
        public:
        static constexpr std::size_t region_count = magic_enum::enum_count<Region>();

        Atlas(const Asset<sf::Texture> &texture, std::array<sf::IntRect, region_count> regions) noexcept
            : texture_ { texture }, regions_ { regions } { }

        [[nodiscard]] const sf::Texture &texture() const noexcept { return texture_.get().get(); }

        [[nodiscard]] sf::IntRect rect(Region region) const noexcept
        {
            return regions_[std::to_underlying(region)];
        }

        [[nodiscard]] sf::Sprite sprite(Region region) const
        {
            return texture_.get().instantiate(rect(region));
        }

        private:
        std::reference_wrapper<const Asset<sf::Texture>> texture_;
        std::array<sf::IntRect, region_count> regions_;
    };

    template<typename Enum, typename ...Types>
        requires std::is_enum_v<Enum>
            && (magic_enum::enum_count<Enum>() > 0)
            && (sizeof...(Types) == 1 || sizeof...(Types) == magic_enum::enum_count<Enum>())
    class EnumMap
    {
        friend class Theme;

        private:
        static constexpr std::size_t count = magic_enum::enum_count<Enum>();
        static constexpr bool is_homogeneous = (sizeof...(Types) == 1);

        template<typename Type>
        using Stored = std::conditional_t<
            std::is_reference_v<Type>,
            std::reference_wrapper<std::remove_reference_t<Type>>,
            std::remove_cv_t<Type>
        >;

        template<Enum Key>
        using ElementType = Types...[is_homogeneous ? 0 : std::to_underlying(Key)];

        using Storage = decltype([] {
            if constexpr (is_homogeneous)
                return std::type_identity<std::array<Stored<Types...[0]>, count>> { };
            else
                return std::type_identity<std::tuple<Stored<Types>...>> { };
        } ())::type;

        class Pending
        {
            public:
            template<Enum Key, typename ...Args>
                requires std::constructible_from<Stored<ElementType<Key>>, Args &&...>
            void emplace(Args &&...args)
            {
                constexpr std::size_t idx = std::to_underlying(Key);
                if constexpr (is_homogeneous)
                    data_[idx].emplace(std::forward<Args>(args)...);
                else
                    std::get<idx>(data_).emplace(std::forward<Args>(args)...);
            }

            template<typename ...Args>
                requires is_homogeneous && std::constructible_from<Stored<Types...[0]>, Args &&...>
            void emplace(Enum key, Args &&...args)
            {
                data_[std::to_underlying(key)].emplace(std::forward<Args>(args)...);
            }

            template<typename Arg>
                requires (!is_homogeneous)
                      && (!std::same_as<std::remove_cvref_t<Arg>, Enum>)
                      && ((std::constructible_from<Stored<Types>, Arg &&> + ...) == 1)
            void emplace(Arg &&arg)
            {
                constexpr std::size_t idx = []<std::size_t ...Is>(std::index_sequence<Is...>) {
                    return ((std::constructible_from<Stored<Types...[Is]>, Arg &&> ? Is : 0) + ...);
                } (std::make_index_sequence<count> { });

                std::get<idx>(data_).emplace(std::forward<Arg>(arg));
            }

            [[nodiscard]] bool ready() const noexcept
            {
                if constexpr (is_homogeneous)
                    return std::ranges::all_of(data_, &std::optional<Stored<Types...[0]>>::has_value);
                else
                    return std::apply([](const auto &...opts) { return (opts.has_value() && ...); }, data_);
            }

            [[nodiscard]] EnumMap build() &&
            {
                if constexpr (is_homogeneous)
                {
                    return [&]<std::size_t ...Is>(std::index_sequence<Is...>) {
                        return EnumMap { Storage { std::move(*data_[Is])... } };
                    } (std::make_index_sequence<count> { });
                }
                else
                {
                    return std::apply([](auto &...opts) {
                        return EnumMap { Storage { std::move(*opts)... } };
                    }, data_);
                }
            }

            private:
            using PendingStorage = decltype([] {
                if constexpr (is_homogeneous)
                    return std::type_identity<std::array<std::optional<Stored<Types...[0]>>, count>> { };
                else
                    return std::type_identity<std::tuple<std::optional<Stored<Types>>...>> { };
            } ())::type;

            PendingStorage data_;
        };

        Storage data_;

        explicit EnumMap(Storage &&data) noexcept
            : data_ { std::move(data) } { }

        public:
        template<Enum Key>
        using Element = ElementType<Key>;

        template<typename Self>
            requires is_homogeneous
        [[nodiscard]] decltype(auto) operator[](this Self &&self, Enum key) noexcept
        {
            if constexpr (std::is_reference_v<Types...[0]>)
                return self.data_[std::to_underlying(key)].get();
            else
                return (std::forward_like<Self>(self.data_[std::to_underlying(key)]));
        }

        template<Enum Key, typename Self>
        [[nodiscard]] decltype(auto) get(this Self &&self) noexcept
        {
            constexpr std::size_t idx = std::to_underlying(Key);
            if constexpr (is_homogeneous)
                return std::forward<Self>(self)[Key];
            else if constexpr (std::is_reference_v<Types...[idx]>)
                return std::get<idx>(self.data_).get();
            else
                return std::get<idx>(std::forward_like<Self>(self.data_));
        }
    };

    class Theme
    {
        public:
        struct Error
        {
            enum class Type
            {
                manifest_not_found,
                invalid_manifest,
                unsupported_version,
                missing_field,
                invalid_value,
                asset_not_found,
                invalid_asset
            };
            using enum Type;

            Type error;
            std::string detail;

            Error(Type error, std::string detail = { }) noexcept
                : error { error }, detail { std::move(detail) } { }

            [[nodiscard]] std::string what() const;
        };

        enum class Section
        {
            atlases,
            metrics,
            fonts,
            colors,
            sounds
        };

        using Atlases = decltype([]<std::size_t ...Is>(std::index_sequence<Is...>) {
            constexpr auto regions = magic_enum::enum_values<AtlasRegion>();
            return std::type_identity<EnumMap<AtlasRegion, Atlas<AtlasRegions<regions[Is]>>...>> { };
        } (std::make_index_sequence<magic_enum::enum_count<AtlasRegion>()> { }))::type;
        using Metrics = EnumMap<Metric, float>;
        using Fonts = EnumMap<Font::Type, Font>;
        using Colors = EnumMap<Color, sf::Color>;
        using Sounds = EnumMap<Sound, const Asset<sf::SoundBuffer> &>;

        [[nodiscard]] std::string_view name() const noexcept { return name_; }
        [[nodiscard]] const Atlases &atlases() const noexcept { return atlases_; }
        [[nodiscard]] const Metrics &metrics() const noexcept { return metrics_; }
        [[nodiscard]] const Fonts &fonts() const noexcept { return fonts_; }
        [[nodiscard]] const Colors &colors() const noexcept { return colors_; }
        [[nodiscard]] const Sounds &sounds() const noexcept { return sounds_; }

        static std::expected<Theme, Error> create();

        private:
        std::unique_ptr<Assets> assets_;
        std::string name_;
        Atlases atlases_;
        Metrics metrics_;
        Fonts fonts_;
        Colors colors_;
        Sounds sounds_;

        Theme(
            std::unique_ptr<Assets> assets, std::string name,
            Atlases atlases, Metrics metrics, Fonts fonts, Colors colors, Sounds sounds
        ) noexcept
            : assets_ { std::move(assets) }, name_ { std::move(name) },
              atlases_ { std::move(atlases) }, metrics_ { std::move(metrics) },
              fonts_ { std::move(fonts) }, colors_ { std::move(colors) },
              sounds_ { std::move(sounds) } { }
    };
} // export namespace minesweeper

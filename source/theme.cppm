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

    template<AtlasRegion Region, typename ...Types>
        requires (sizeof...(Types) == magic_enum::enum_count<AtlasRegion>())
                && (static_cast<std::size_t>(Region) < sizeof...(Types))
    using AtlasRegionElement = Types...[static_cast<std::size_t>(Region)];

    template<AtlasRegion Region>
    using AtlasRegions = AtlasRegionElement<Region, TilesRegion, UiRegion>;

    class Atlases
    {
        friend class Theme;

        public:
        template<AtlasRegion Reg, typename Self>
        [[nodiscard]] decltype(auto) atlas(this Self &&self) noexcept
        {
            return std::get<Atlas<AtlasRegions<Reg>>>(std::forward_like<Self>(self.data_));
        }

        private:
        using Tuple = decltype([]<std::size_t ...Is>(std::index_sequence<Is...>) {
            constexpr auto regions = magic_enum::enum_values<AtlasRegion>();
            return std::type_identity<std::tuple<Atlas<AtlasRegions<regions[Is]>>...>> { };
        } (std::make_index_sequence<magic_enum::enum_count<AtlasRegion>()> { }))::type;

        template<typename SubEnum>
        static constexpr bool is_valid_sub_enum = []<std::size_t ...Is>(std::index_sequence<Is...>) {
            return (std::same_as<Atlas<SubEnum>, std::tuple_element_t<Is, Tuple>> || ...);
        } (std::make_index_sequence<std::tuple_size_v<Tuple>> { });

        class Pending
        {
            public:
            template <typename SubEnum>
                requires is_valid_sub_enum<SubEnum>
            void emplace(Atlas<SubEnum> &&atlas)
            {
                std::get<std::optional<Atlas<SubEnum>>>(data_).emplace(std::move(atlas));
            }

            [[nodiscard]] bool ready() const noexcept
            {
                return std::apply([](const auto &...opts) {
                    return (opts.has_value() && ...);
                }, data_);
            }

            [[nodiscard]] Atlases build() &&
            {
                return std::apply([](auto &...opts) {
                    return Atlases { Tuple { std::move(*opts)... } };
                }, data_);
            }

            private:
            using OptionalTuple = decltype([]<std::size_t... Is>(std::index_sequence<Is...>) {
                return std::type_identity<std::tuple<std::optional<std::tuple_element_t<Is, Tuple>>...>> { };
            } (std::make_index_sequence<std::tuple_size_v<Tuple>> { }))::type;

            OptionalTuple data_;
        };

        Tuple data_;

        explicit Atlases(Tuple &&data) noexcept
            : data_ { std::move(data) } { }
    };

    template<typename Enum, typename Type>
        requires std::is_enum_v<Enum>
    struct EnumMap
    {
        friend class Theme;

        public:
        template<typename Self>
        [[nodiscard]] decltype(auto) operator[](this Self &&self, Enum key) noexcept
        {
            if constexpr (std::is_reference_v<Type>)
                return self.data_[std::to_underlying(key)].get();
            else
                return (std::forward_like<Self>(self.data_)[std::to_underlying(key)]);
        }

        private:
        static constexpr std::size_t count = magic_enum::enum_count<Enum>();

        using Stored = std::conditional_t<
            std::is_reference_v<Type>,
            std::reference_wrapper<std::remove_reference_t<Type>>,
            Type
        >;

        class Pending
        {
            public:
            template<typename ...Args>
                requires std::constructible_from<Stored, Args &&...>
            void emplace(Enum key, Args &&...args)
            {
                data_[std::to_underlying(key)].emplace(std::forward<Args>(args)...);
            }

            [[nodiscard]] bool ready() const noexcept
            {
                return std::ranges::all_of(data_, &std::optional<Stored>::has_value);
            }

            [[nodiscard]] EnumMap build() &&
            {
                return [&]<std::size_t ...Is>(std::index_sequence<Is...>) {
                    return EnumMap { std::array<Stored, count> { std::move(*data_[Is])... } };
                } (std::make_index_sequence<count> { });
            }

            private:
            std::array<std::optional<Stored>, count> data_;
        };

        std::array<Stored, count> data_;

        explicit EnumMap(std::array<Stored, count> &&data) noexcept
            : data_ { std::move(data) } { }
    };

    class Theme
    {
        public:
        enum class Error
        {
            manifest_not_found,
            invalid_manifest,
            unsupported_version,
            missing_field,
            invalid_value,
            asset_load_failed
        };

        enum class Section
        {
            atlases,
            metrics,
            fonts,
            colors,
            sounds
        };

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

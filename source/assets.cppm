// Copyright (C) 2026  ilobilo

export module minesweeper:assets;

import sfml;
import std;

import :utilities;

export namespace minesweeper
{
    enum class AssetError
    {
        asset_not_found,
        invalid_contents
    };

    class InputStream : public sf::InputStream
    {
        public:
        static std::expected<std::unique_ptr<InputStream>, AssetError> create(const std::string &path);

        ~InputStream() override;

        InputStream(const InputStream &) = delete;
        InputStream(InputStream &&other) noexcept
            : file_ { std::exchange(other.file_, nullptr) } { }

        InputStream &operator=(const InputStream &) = delete;
        InputStream &operator=(InputStream &&other) noexcept
        {
            if (this != &other)
            {
                close();
                file_ = std::exchange(other.file_, nullptr);
            }
            return *this;
        }

        [[nodiscard]] std::optional<std::size_t> read(void *data, std::size_t size) override;
        [[nodiscard]] std::optional<std::size_t> seek(std::size_t position) override;
        [[nodiscard]] std::optional<std::size_t> tell() override;
        std::optional<std::size_t> getSize() override;

        private:
        void *file_;

        InputStream(void *file) noexcept
            : file_ { file } { }

        void close();
    };

    template<typename Type>
    struct AssetLoader;

    template<>
    struct AssetLoader<sf::Texture>
    {
        using Instance = sf::Sprite;

        static bool load(sf::Texture &asset, sf::InputStream &stream)
        {
            return asset.loadFromStream(stream);
        }
    };

    template<>
    struct AssetLoader<sf::Font>
    {
        using Instance = sf::Text;

        static bool load(sf::Font &asset, sf::InputStream &stream)
        {
            return asset.openFromStream(stream);
        }
    };

    template<>
    struct AssetLoader<sf::SoundBuffer>
    {
        using Instance = sf::Sound;

        static bool load(sf::SoundBuffer &asset, sf::InputStream &stream)
        {
            return asset.loadFromStream(stream);
        }
    };

    template<typename Type>
    concept LoadableAsset = requires(Type &asset, sf::InputStream &stream) {
        typename AssetLoader<Type>::Instance;
        { AssetLoader<Type>::load(asset, stream) } -> std::same_as<bool>;
    };

    template<LoadableAsset Type>
    class Asset
    {
        using Loader = AssetLoader<Type>;

        public:
        static std::expected<Asset, AssetError> create(const std::string &path)
        {
            auto stream = InputStream::create(path);
            if (!stream.has_value())
                return std::unexpected { stream.error() };

            Asset result { std::move(*stream) };
            if (!Loader::load(result.object_, *result.stream_))
                return std::unexpected { AssetError::invalid_contents };

            return result;
        }

        Asset(const Asset &) = delete;
        Asset(Asset &&other) noexcept(std::is_nothrow_move_constructible_v<Type>)
            : stream_ { std::move(other.stream_) }, object_ { std::move(other.object_) } { }

        Asset &operator=(const Asset &) = delete;
        Asset &operator=(Asset &&) = delete;

        [[nodiscard]] auto &get(this auto &self) noexcept
        {
            return self.object_;
        }

        [[nodiscard]] auto &operator*(this auto &self) noexcept
        {
            return self.object_;
        }

        [[nodiscard]] auto operator->(this auto &self) noexcept
        {
            return std::addressof(self.object_);
        }

        template<typename ...Args>
            requires (!std::is_void_v<typename Loader::Instance>)
        [[nodiscard]] Loader::Instance instantiate(this auto &self, Args &&...args)
        {
            return { std::as_const(self.object_), std::forward<Args>(args)... };
        }

        private:
        std::unique_ptr<InputStream> stream_;
        Type object_;

        Asset(std::unique_ptr<InputStream> stream) noexcept
            : stream_ { std::move(stream) }, object_ { } { }
    };

    template<LoadableAsset Type>
    class AssetCache
    {
        public:
        [[nodiscard]] util::expected_ref<Asset<Type>, AssetError> load(const std::string &path)
        {
            if (auto it = assets_.find(path); it != assets_.end())
                return it->second;

            auto asset = Asset<Type>::create(path);
            if (!asset)
                return std::unexpected { asset.error() };

            return assets_.emplace(path, std::move(*asset)).first->second;
        }

        [[nodiscard]] std::optional<Asset<Type> &> find(const std::string &path) noexcept
        {
            auto it = assets_.find(path);
            if (it == assets_.end())
                return std::nullopt;
            return it->second;
        }

        private:
        std::unordered_map<std::string, Asset<Type>> assets_;
    };

    class Assets
    {
        public:
        [[nodiscard]] auto &textures() noexcept { return textures_; }
        [[nodiscard]] auto &fonts() noexcept { return fonts_; }
        [[nodiscard]] auto &sounds() noexcept { return sounds_; }

        private:
        AssetCache<sf::Texture> textures_;
        AssetCache<sf::Font> fonts_;
        AssetCache<sf::SoundBuffer> sounds_;
    };
} // export namespace minesweeper

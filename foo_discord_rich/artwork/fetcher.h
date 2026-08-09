#pragma once

#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <variant>

namespace drp
{

class ArtworkFetcher
{
public:
    struct MusicBrainzFetchRequest
    {
        qwr::u8string artist;
        qwr::u8string album;
        std::optional<qwr::u8string> userReleaseMbidOpt;

        auto operator<=>( const MusicBrainzFetchRequest& other ) const = default;
    };

    struct UploadRequest
    {
        qwr::u8string artPinId;
        metadb_handle_ptr handle;
        qwr::u8string uploadCommand;

        auto operator<=>( const UploadRequest& other ) const = default;
    };

    using FetchRequest = std::variant<MusicBrainzFetchRequest, UploadRequest>;

public:
    static ArtworkFetcher& Get();

    void Initialize();
    void Finalize();

    std::optional<qwr::u8string> GetArtUrl( const FetchRequest& request );

    void LoadCache( bool throwOnError = false );
    void SaveCache();
    void ClearCache();
    static std::filesystem::path GetCacheFilePath();

private:
    struct FetchOutcome
    {
        std::optional<qwr::u8string> artUrl;
        bool cacheable = false;
    };

    struct CacheEntry
    {
        std::optional<qwr::u8string> artUrl;
        int64_t fetchedAt = 0;
    };

    void StartThread();
    void StopThread();

    void ThreadMain( std::stop_token token );

    FetchOutcome ProcessFetchRequest( const MusicBrainzFetchRequest& request );
    FetchOutcome ProcessFetchRequest( const UploadRequest& request );

private:
    ArtworkFetcher() = default;

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::unique_ptr<std::jthread> pThread_;

    std::optional<FetchRequest> currentRequestOpt_;
    std::unordered_map<qwr::u8string, CacheEntry> artPinIdToArtUrl_;
};

} // namespace drp

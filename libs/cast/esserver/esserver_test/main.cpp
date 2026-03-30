#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

#include "ESServer.h"

namespace fs = std::filesystem;

namespace {

struct StreamDumpContext {
    uint32_t streamId = 0;
    std::string name;
    std::string ip;

    fs::path dir;
    std::ofstream videoFile;
    std::ofstream audioFile;

    uint64_t videoBytes = 0;
    uint64_t audioBytes = 0;
    uint64_t videoPackets = 0;
    uint64_t audioPackets = 0;
};

class TestCallback : public hhcast::IESServerCallback {
public:
    explicit TestCallback(const fs::path& baseDir)
        : m_baseDir(baseDir)
    {
        std::error_code ec;
        fs::create_directories(m_baseDir, ec);
        if (ec) {
            std::cout << "[TestCallback] create base dir failed: "
                      << m_baseDir.string()
                      << ", ec=" << ec.message() << std::endl;
        } else {
            std::cout << "[TestCallback] dump dir: "
                      << fs::absolute(m_baseDir).string() << std::endl;
        }
    }

    ~TestCallback() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& kv : m_streams) {
            CloseStreamLocked(kv.second);
        }
        m_streams.clear();
    }

    void OnConnect(uint32_t streamId, const std::string& name, const std::string& ip) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto ctx = GetOrCreateStreamLocked(streamId);
        ctx->name = name;
        ctx->ip = ip;

        WriteSessionInfoLocked(*ctx);

        std::cout << "[TestCallback] OnConnect streamId=" << streamId
                  << ", name=" << name
                  << ", ip=" << ip
                  << ", dir=" << ctx->dir.string()
                  << std::endl;
    }

    void OnDisconnect(uint32_t streamId) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_streams.find(streamId);
        if (it == m_streams.end()) {
            std::cout << "[TestCallback] OnDisconnect streamId=" << streamId
                      << ", stream context not found" << std::endl;
            return;
        }

        const auto& ctx = it->second;
        std::cout << "[TestCallback] OnDisconnect streamId=" << streamId
                  << ", videoPackets=" << ctx->videoPackets
                  << ", videoBytes=" << ctx->videoBytes
                  << ", audioPackets=" << ctx->audioPackets
                  << ", audioBytes=" << ctx->audioBytes
                  << std::endl;

        CloseStreamLocked(ctx);
        m_streams.erase(it);
    }

    void OnVideoData(uint32_t streamId, const uint8_t* data, size_t size) override
    {
        if (data == nullptr || size == 0) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        auto ctx = GetOrCreateStreamLocked(streamId);
        if (!ctx->videoFile.is_open()) {
            std::cout << "[TestCallback] video file not open, streamId="
                      << streamId << std::endl;
            return;
        }

        ctx->videoFile.write(reinterpret_cast<const char*>(data),
                             static_cast<std::streamsize>(size));
        ctx->videoFile.flush();

        ctx->videoBytes += static_cast<uint64_t>(size);
        ctx->videoPackets += 1;

        std::cout << "[TestCallback] OnVideoData streamId=" << streamId
                  << ", size=" << size
                  << ", totalVideoBytes=" << ctx->videoBytes
                  << std::endl;
    }

    void OnAudioData(uint32_t streamId, const uint8_t* data, size_t size) override
    {
        if (data == nullptr || size == 0) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        auto ctx = GetOrCreateStreamLocked(streamId);
        if (!ctx->audioFile.is_open()) {
            std::cout << "[TestCallback] audio file not open, streamId="
                      << streamId << std::endl;
            return;
        }

        // 4字节小端长度前缀
        const uint32_t payloadLen = static_cast<uint32_t>(size);
        char lenBuf[4];
        lenBuf[0] = static_cast<char>( payloadLen        & 0xFF);
        lenBuf[1] = static_cast<char>((payloadLen >> 8 ) & 0xFF);
        lenBuf[2] = static_cast<char>((payloadLen >> 16) & 0xFF);
        lenBuf[3] = static_cast<char>((payloadLen >> 24) & 0xFF);

        ctx->audioFile.write(lenBuf, 4);
        ctx->audioFile.write(reinterpret_cast<const char*>(data),
                             static_cast<std::streamsize>(size));
        ctx->audioFile.flush();

        ctx->audioBytes += static_cast<uint64_t>(size);
        ctx->audioPackets += 1;

        std::cout << "[TestCallback] OnAudioData streamId=" << streamId
                  << ", size=" << size
                  << ", totalAudioBytes=" << ctx->audioBytes
                  << ", audioPackets=" << ctx->audioPackets
                  << std::endl;
    }

private:
    std::shared_ptr<StreamDumpContext> GetOrCreateStreamLocked(uint32_t streamId)
    {
        auto it = m_streams.find(streamId);
        if (it != m_streams.end()) {
            return it->second;
        }

        auto ctx = std::make_shared<StreamDumpContext>();
        ctx->streamId = streamId;
        ctx->dir = m_baseDir / BuildStreamDirName(streamId);

        std::error_code ec;
        fs::create_directories(ctx->dir, ec);
        if (ec) {
            std::cout << "[TestCallback] create stream dir failed: "
                      << ctx->dir.string()
                      << ", ec=" << ec.message() << std::endl;
        }

        const fs::path videoPath = ctx->dir / "video.h264";
        const fs::path audioPath = ctx->dir / "audio_len.aac-eld";

        ctx->videoFile.open(videoPath, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!ctx->videoFile.is_open()) {
            std::cout << "[TestCallback] open video file failed: "
                      << videoPath.string() << std::endl;
        }

        ctx->audioFile.open(audioPath, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!ctx->audioFile.is_open()) {
            std::cout << "[TestCallback] open audio file failed: "
                      << audioPath.string() << std::endl;
        }

        WriteSessionInfoLocked(*ctx);

        m_streams[streamId] = ctx;

        std::cout << "[TestCallback] stream files ready, streamId=" << streamId
                  << ", dir=" << ctx->dir.string() << std::endl;

        return ctx;
    }

    void WriteSessionInfoLocked(const StreamDumpContext& ctx)
    {
        const fs::path infoPath = ctx.dir / "session.txt";
        std::ofstream infoFile(infoPath, std::ios::out | std::ios::trunc);
        if (!infoFile.is_open()) {
            std::cout << "[TestCallback] open session info failed: "
                      << infoPath.string() << std::endl;
            return;
        }

        infoFile << "streamId=" << ctx.streamId << "\n";
        infoFile << "name=" << ctx.name << "\n";
        infoFile << "ip=" << ctx.ip << "\n";
        infoFile << "videoFile=video.h264\n";
        infoFile << "audioFile=audio_len.aac-eld\n";
        infoFile << "audioFormat=[4-byte little-endian length][aac-eld payload]\n";
        infoFile.close();
    }

    void CloseStreamLocked(const std::shared_ptr<StreamDumpContext>& ctx)
    {
        if (!ctx) {
            return;
        }

        if (ctx->videoFile.is_open()) {
            ctx->videoFile.flush();
            ctx->videoFile.close();
        }

        if (ctx->audioFile.is_open()) {
            ctx->audioFile.flush();
            ctx->audioFile.close();
        }
    }

    static std::string BuildStreamDirName(uint32_t streamId)
    {
        std::ostringstream oss;
        oss << "stream_" << streamId;
        return oss.str();
    }

private:
    fs::path m_baseDir;
    std::mutex m_mutex;
    std::unordered_map<uint32_t, std::shared_ptr<StreamDumpContext>> m_streams;
};

} // namespace

int main()
{
    std::cout << "hello from esserver_test" << std::endl;

    hhcast::ESServer server;
    auto callback = std::make_shared<TestCallback>("esserver_dump");
    server.SetCallback(callback);

    const int ret = server.StartServer();
    std::cout << "StartServer ret = " << ret << std::endl;
    std::cout << "server running: " << server.IsRunning() << std::endl;
    std::cout << "input q to stop server" << std::endl;

    std::string cmd;
    while (std::getline(std::cin, cmd)) {
        if (cmd == "q" || cmd == "Q") {
            break;
        }
    }

    server.StopServer();
    return 0;
}
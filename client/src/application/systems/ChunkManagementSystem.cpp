#include "ChunkManagementSystem.h"

#include <algorithm>

#include "RendererSystem.h"
#include "application/Application.h"
#include "common/Log.h"
#include "common/FileSystem.h"

#include <charconv>
#include <optional>
#include <string_view>
#include <sstream>

namespace {

std::string resolveWorldGeneratorParamsFileName(const std::string& path) {
    if (path.empty()) {
        return "worldGeneratorParams.json";
    }

    if (path.size() >= 5 && path.substr(path.size() - 5) == ".json") {
        return path;
    }

    if (!path.empty() && (path.back() == '/' || path.back() == '\\')) {
        return path + "worldGeneratorParams.json";
    }

    return path + "/worldGeneratorParams.json";
}

std::string serializeWorldGeneratorParams(const WorldGeneratorParams& params) {
    return "{\n"
           "  \"seed\": " + std::to_string(params.seed) + ",\n"
           "  \"cavesEnabled\": " + std::string(params.cavesEnabled ? "true" : "false") + "\n"
           "}\n";
}

bool extractIntValue(const std::string& json, const std::string_view key, int& outValue) {
    const std::string keyPattern = "\"" + std::string(key) + "\"";
    const auto keyPos = json.find(keyPattern);
    if (keyPos == std::string::npos) {
        return false;
    }

    const auto colonPos = json.find(':', keyPos + keyPattern.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    const auto valuePos = json.find_first_of("-0123456789", colonPos + 1);
    if (valuePos == std::string::npos) {
        return false;
    }

    const char* begin = json.data() + valuePos;
    const char* end = json.data() + json.size();
    const auto [_, ec] = std::from_chars(begin, end, outValue);
    return ec == std::errc{};
}

bool extractBoolValue(const std::string& json, const std::string_view key, bool& outValue) {
    const std::string keyPattern = "\"" + std::string(key) + "\"";
    const auto keyPos = json.find(keyPattern);
    if (keyPos == std::string::npos) {
        return false;
    }

    const auto colonPos = json.find(':', keyPos + keyPattern.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    const auto valuePos = json.find_first_not_of(" \t\r\n", colonPos + 1);
    if (valuePos == std::string::npos) {
        return false;
    }

    if (json.compare(valuePos, 4, "true") == 0 || json.compare(valuePos, 1, "1") == 0) {
        outValue = true;
        return true;
    }

    if (json.compare(valuePos, 5, "false") == 0 || json.compare(valuePos, 1, "0") == 0) {
        outValue = false;
        return true;
    }

    return false;
}

std::optional<WorldGeneratorParams> parseWorldGeneratorParams(const std::string& json) {
    WorldGeneratorParams params{};

    if (!extractIntValue(json, "seed", params.seed)) {
        return std::nullopt;
    }

    if (!extractBoolValue(json, "cavesEnabled", params.cavesEnabled)) {
        return std::nullopt;
    }

    return params;
}

} // namespace

void ChunkManagementSystem::initialize() {

    const auto hardwareConcurrency = Threading::hardwareConcurrency();

    m_chunkWorkersCount = 1;

    if (hardwareConcurrency > 2) {
        m_chunkWorkersCount = m_chunkWorkersCount = std::min(hardwareConcurrency / 2, 5u);
    }

    for (size_t i = 0; i < m_chunkWorkersCount; ++i) {
        m_chunkWorkers.push_back(std::make_unique<Threading::Worker>());
        m_chunkWorkers[i]->start(worker, this);
    }

    setLoadDistance(500);
}

void ChunkManagementSystem::update(const float dt) {
    const Camera& camera = getCamera();
    World& world = getWorld();

    processChunkManagement(camera, world);

    m_generator->pruneCacheByDistance(camera.getPosition(), m_unloadZoneRadiusXz);
}

void ChunkManagementSystem::processChunkManagement(const Camera& camera, World& world) {
    Threading::ScopedLock lock(&m_lock);

    integrateLoadedChunks(world);
    integrateCompressedChunks(world);

    static int turn = 0;
    turn = (turn + 1) % 4;
    if (turn == 0) {
        scheduleChunksForLoading(camera, world);
    } else if (turn == 1) {
        scheduleChunksForSaving(world);
    } else if (turn == 2) {
        scheduleChunksForUnloading(camera, world);
    } else if (turn == 3) {
        scheduleChunksForCompression(world, camera);
    }
}

void ChunkManagementSystem::integrateLoadedChunks(World &world) {
    for (auto& chunk : m_loadedChunks) {
        world.insertChunkByMove(*chunk);
        m_loadingChunks.erase(chunk->getPosition());
    }
    m_loadedChunks.clear();
}

void ChunkManagementSystem::integrateCompressedChunks(World& world) {
    for (auto& task : m_compressedChunks) {
        if (auto* chunk = world.tryGetChunkPtr(task.position)) {
            if (chunk->getLastEdit() == task.lastAccess) {
                *chunk = std::move(*task.chunk);
            } else {
                LogApp::warning("Chunk at ({}, {}, {}) was modified after compression, skipping integration",
                                task.position.x, task.position.y, task.position.z);
            }
        } else {
            LogApp::warning("Chunk at ({}, {}, {}) not found for integration after compression",
                            task.position.x, task.position.y, task.position.z);
        }
        m_compressingChunks.erase(task.position);
    }
    m_compressedChunks.clear();
}

std::vector<glm::ivec3> ChunkManagementSystem::generateChunkOffsets() const {
    std::vector<glm::ivec3> offsets;
    const float yCorrection = static_cast<float>(m_loadZoneRadiusXz) / m_loadZoneRadiusY;
    for (int x = -m_loadZoneRadiusXz; x <= m_loadZoneRadiusXz; ++x) {
        for (int y = -m_loadZoneRadiusY; y <= m_loadZoneRadiusY; ++y) {
            const auto cy = static_cast<float>(y) * yCorrection;
            for (int z = -m_loadZoneRadiusXz; z <= m_loadZoneRadiusXz; ++z) {
                if (x * x + z * z + cy * cy <= m_loadZoneRadiusXz * m_loadZoneRadiusXz) {
                    offsets.emplace_back(x, y, z);
                }
            }
        }
    }
    std::sort(offsets.begin(), offsets.end(), [yCorrection](const glm::ivec3& a, const glm::ivec3& b) {
        const float cay = static_cast<float>(a.y) * yCorrection;
        const float cby = static_cast<float>(b.y) * yCorrection;
        const float da = a.x * a.x + a.z * a.z + cay * cay;
        const float db = b.x * b.x + b.z * b.z + cby * cby;
        return da < db;
    });
    return offsets;
}

void ChunkManagementSystem::scheduleChunksForLoading(const Camera& camera, const World& world) {
    const glm::vec3 playerPosition = camera.getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();

    std::vector<glm::ivec3> chunksToLoad;
    const size_t maxChunksToLoad = m_chunkWorkersCount * 32;

    for (const auto& offset : m_chunkOffsets) {
        const auto chunkPos = playerChunk + offset;

        if (world.hasChunk(chunkPos) || m_loadingChunks.contains(chunkPos)) {
            continue;
        }

        chunksToLoad.push_back(chunkPos);

        if (chunksToLoad.size() >= maxChunksToLoad) {
            break;
        }
    }

    m_chunksToLoad = std::queue(chunksToLoad.begin(), chunksToLoad.end());
}

void ChunkManagementSystem::scheduleChunksForSaving(World& world) {
    for (auto &chunkRef : world.getChunks()) {
        auto& chunk = chunkRef.get();
        if (chunk.isSaveFileDirty()) {
            if (!m_savingChunks.contains(chunk.getPosition())) {
                m_chunksToSave.push(ChunkHandle::makeCopy(chunk));
                m_savingChunks.insert(chunk.getPosition());
                chunk.resetSaveFileDirty();
            }
        }
    }
}

void ChunkManagementSystem::scheduleChunksForUnloading(const Camera &camera, World &world) {
    const glm::vec3 playerPosition = camera.getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();
    const float yCorrection = static_cast<float>(m_unloadZoneRadiusXz) / m_unloadZoneRadiusY;

    std::vector<glm::ivec3> chunksToUnload;

    for (const auto &chunkRef : world.getChunks()) {
        auto& chunk = chunkRef.get();
        auto chunkPos = chunk.getPosition();
        const glm::ivec3 offset = chunkPos - playerChunk;
        const float correctedOffestY = static_cast<float>(offset.y) * yCorrection;
        const float distanceSq = offset.x * offset.x + offset.z * offset.z + correctedOffestY * correctedOffestY;
        const float unloadRadiusSq = static_cast<float>(m_unloadZoneRadiusXz * m_unloadZoneRadiusXz);
        if (distanceSq > unloadRadiusSq) {
            chunksToUnload.push_back(chunkPos);
        }
    }

    for (const auto &chunkPos : chunksToUnload) {
        m_chunksToUnload.emplace(world.extractChunkByMove(chunkPos));
    }
}

void ChunkManagementSystem::scheduleChunksForCompression(World& world, const Camera& camera) {
    const glm::vec3 playerPosition = camera.getPosition();

    constexpr size_t maxChunksToSchedule = 2;

    if (m_compressingChunks.size() >= maxChunksToSchedule) {
        return;
    }

    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::vector<Chunk*> candidates;
    for (auto& chunkRef : world.getChunks()) {
        auto& chunk = chunkRef.get();
        const glm::ivec3 chunkPos = chunk.getPosition();
        if (chunk.isCompressed()) continue;
        if (chunk.isGpuBufferDirty() || chunk.isSaveFileDirty()) continue;
        if (chunk.getLastEdit() - now < std::chrono::seconds(15)) continue;
        if (getChunkDistance(playerPosition, chunkPos) <= m_fastAccessRadius) continue;
        if (m_compressingChunks.contains(chunkPos)) continue;

        const auto& chunkNeighborhood = world.getChunkNeighborhoodPtrs(chunkPos);
        if (!chunkNeighborhood.hasAllNeighbours()) continue;
        if (chunkNeighborhood.anyNeighbourDirty()) continue;

        candidates.push_back(&chunk);
    }

    std::ranges::sort(candidates, [](const Chunk* a, const Chunk* b) {
        return a->getLastEdit() < b->getLastEdit();
    });

    size_t count = 0;
    for (const auto* chunk : candidates) {
        if (count >= maxChunksToSchedule) break;
        const glm::ivec3 chunkPos = chunk->getPosition();
        CompressionTask task{chunkPos, ChunkHandle::makeCopy(*chunk), chunk->getLastEdit()};
        m_chunksToCompress.push(std::move(task));
        m_compressingChunks.insert(chunkPos);
        ++count;
    }
}

float ChunkManagementSystem::getChunkDistance(const glm::vec3 playerPosition, const glm::ivec3 chunkPos) {
    const auto transformedPlayerPosition = playerPosition * glm::vec3(1.0f, 1.0f, 1.0f);
    const auto transformedChunkPos = glm::vec3(chunkPos) * glm::vec3(1.0f, 1.0f, 1.0f);
    return glm::distance(glm::vec3(transformedChunkPos), glm::vec3(WorldCoordinate(transformedPlayerPosition).chunkPosition()));
}

void ChunkManagementSystem::handleChunkSave(ChunkHandle& chunkToSave) {
    const auto chunkPos = chunkToSave->getPosition();
    chunkToSave->save(m_savePath);

    Threading::ScopedLock lock(&m_lock);
    m_savingChunks.erase(chunkPos);
}

void ChunkManagementSystem::handleChunkLoad(const glm::ivec3& chunkToLoad) {
    Chunk chunk(chunkToLoad);
    if (chunk.fileExists(m_savePath)) {
        chunk.load(m_savePath);
    } else {
        chunk.generate(*m_generator);
    }

    Threading::ScopedLock lock(&m_lock);
    m_loadedChunks.emplace_back(std::move(chunk));
}

void ChunkManagementSystem::handleChunkCompression(CompressionTask& task) {
    task.chunk->compress();
    Threading::ScopedLock lock(&m_lock);
    m_compressedChunks.push_back(std::move(task));
}

void* ChunkManagementSystem::worker(void *arg) {
    auto& system = *static_cast<ChunkManagementSystem*>(arg);

    Work work;

    while (system.fetchWork(work)) {

        if (!work.hasPendingWork()) {
            Threading::sleep(20);
            continue;
        }

        while (!work.chunksToUnload.empty()) {
            work.chunksToUnload.pop();
        }

        if (work.chunkToSave.has_value()) {
            system.handleChunkSave(*work.chunkToSave);
            work.chunkToSave.reset();
        }

        if (work.chunkToLoad.has_value()) {
            system.handleChunkLoad(*work.chunkToLoad);
            work.chunkToLoad.reset();
        }

        if (work.compressionTask.has_value()) {
            system.handleChunkCompression(*work.compressionTask);
            work.compressionTask.reset();
        }

        Threading::sleep(1);
    }

    return nullptr;
}

bool ChunkManagementSystem::fetchWork(Work& work) {
    Threading::ScopedLock lock(&m_lock);

    if (m_shouldExit && m_chunksToSave.empty()) {
        return false;
    }

    if (!m_chunksToSave.empty()) {
        work.chunkToSave = std::move(m_chunksToSave.front());
        m_chunksToSave.pop();
        m_savingChunks.emplace(work.chunkToSave.value()->getPosition());
    }

    if (!m_chunksToLoad.empty()) {
        work.chunkToLoad = m_chunksToLoad.front();
        m_chunksToLoad.pop();
        m_loadingChunks.emplace(work.chunkToLoad.value());
    }

    while (!m_chunksToUnload.empty()) {
        auto& chunk = m_chunksToUnload.front();
        work.chunksToUnload.push(std::move(chunk));
        m_chunksToUnload.pop();
    }

    if (!m_chunksToCompress.empty()) {
        work.compressionTask = std::move(m_chunksToCompress.front());
        m_chunksToCompress.pop();
        m_compressingChunks.emplace(work.compressionTask->position);
    }

    return true;
}

void ChunkManagementSystem::saveWorldGeneratorParams(const std::string &path, const WorldGeneratorParams worldGeneratorParams) {
    const auto fileName = resolveWorldGeneratorParamsFileName(path);
    const auto parentPathPos = fileName.find_last_of("/\\");
    if (parentPathPos != std::string::npos) {
        FileSystem::ensureDirectory(fileName.substr(0, parentPathPos));
    }

    const auto json = serializeWorldGeneratorParams(worldGeneratorParams);
    FileSystem::writeFile(fileName, json.data(), json.size());
}

WorldGeneratorParams ChunkManagementSystem::loadWorldGeneratorParams(const std::string &path) {
    const auto fileName = resolveWorldGeneratorParamsFileName(path);
    if (!FileSystem::fileExists(fileName)) {
        LogApp::warning("World generator params file not found: {}", fileName);
        return {};
    }

    const std::vector<char> buffer = FileSystem::readFile(fileName);
    if (buffer.empty()) {
        LogApp::warning("World generator params file is empty: {}", fileName);
        return {};
    }

    const std::string json(buffer.begin(), buffer.end());
    const auto params = parseWorldGeneratorParams(json);
    if (!params.has_value()) {
        LogApp::error("Failed to parse world generator params from file: {}", fileName);
        return {};
    }

    return params.value();
}

// Helper: extract array of 3 floats from simple JSON like: "key": [x, y, z]
static bool extractFloatArray(const std::string& json, const std::string_view key, glm::vec3& outValue) {
    const std::string keyPattern = "\"" + std::string(key) + "\"";
    const auto keyPos = json.find(keyPattern);
    if (keyPos == std::string::npos) {
        return false;
    }

    const auto bracketPos = json.find('[', keyPos + keyPattern.size());
    if (bracketPos == std::string::npos) return false;
    const auto endBracket = json.find(']', bracketPos + 1);
    if (endBracket == std::string::npos) return false;

    const std::string arrayContent = json.substr(bracketPos + 1, endBracket - bracketPos - 1);
    std::istringstream ss(arrayContent);
    float a, b, c;
    char comma;
    if (!(ss >> a)) return false;
    // consume optional commas/spaces
    ss >> std::ws;
    if (ss.peek() == ',') ss >> comma;
    ss >> std::ws;
    if (!(ss >> b)) return false;
    ss >> std::ws;
    if (ss.peek() == ',') ss >> comma;
    ss >> std::ws;
    if (!(ss >> c)) return false;

    outValue = glm::vec3(a, b, c);
    return true;
}

void ChunkManagementSystem::savePlayerState(const Camera& camera) {
    const auto fileName = m_savePath.empty() ? "player.json" : (m_savePath + "/player.json");
    const auto parentPathPos = fileName.find_last_of("/\\");
    if (parentPathPos != std::string::npos) {
        FileSystem::ensureDirectory(fileName.substr(0, parentPathPos));
    }

    const auto pos = camera.getPosition();
    const auto dir = camera.getDirection();

    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"position\": [" << pos.x << ", " << pos.y << ", " << pos.z << "],\n";
    oss << "  \"direction\": [" << dir.x << ", " << dir.y << ", " << dir.z << "]\n";
    oss << "}\n";

    const auto json = oss.str();
    FileSystem::writeFile(fileName, json.data(), json.size());
}

bool ChunkManagementSystem::loadPlayerState(Camera& camera) {
    const auto fileName = m_savePath.empty() ? "player.json" : (m_savePath + "/player.json");
    if (!FileSystem::fileExists(fileName)) {
        LogApp::warning("Player state file not found: {}", fileName);
        return false;
    }

    const std::vector<char> buffer = FileSystem::readFile(fileName);
    if (buffer.empty()) {
        LogApp::warning("Player state file is empty: {}", fileName);
        return false;
    }

    const std::string json(buffer.begin(), buffer.end());
    glm::vec3 position;
    glm::vec3 direction;
    if (!extractFloatArray(json, "position", position) || !extractFloatArray(json, "direction", direction)) {
        LogApp::error("Failed to parse player state from file: {}", fileName);
        return false;
    }

    camera.setPosition(position);
    camera.setDirection(direction);
    return true;
}

#pragma once

#include <cstddef>
#include <vector>

class Terrain
{
public:
    static constexpr int kDefaultDivisions = 32;
    static constexpr int kMinDivisions = 5;
    static constexpr int kMaxDivisions = 320;
    static constexpr float kDefaultMapSize = 32.f;
    static constexpr float kMinMapSize = 5.f;
    static constexpr float kMaxMapSize = 320.f;
    static constexpr float kDefaultAggressiveness = 3.f;
    static constexpr float kMinAggressiveness = 0.2f;
    static constexpr float kMaxAggressiveness = 10.f;

public:
    Terrain();

    void generate();
    void makeFlat(float value = 1.f);
    void setNoiseRange(float low, float high);
    void setDivisions(int divisions);
    void setMapSize(float mapSize);
    void setAggressiveness(float aggressiveness);

    float heightAt(int x, int z) const;
    int divisions() const;
    float mapSize() const;
    bool isGenerated() const;
    float noiseLowLimit() const;
    float noiseHighLimit() const;
    float aggressiveness() const;

    void setSpawnCell(int x, int z);
    void setDestinationCell(int x, int z);
    bool hasSpawnCell() const;
    bool hasDestinationCell() const;
    int spawnCellX() const;
    int spawnCellZ() const;
    int destinationCellX() const;
    int destinationCellZ() const;

private:
    std::size_t index(int x, int z) const;
    int clampCell(int value) const;

private:
    int mDivisions = kDefaultDivisions;
    float mMapSize = kDefaultMapSize;
    std::vector<float> mHeights;
    bool mGenerated = false;
    bool mHasSpawnCell = false;
    bool mHasDestinationCell = false;
    int mSpawnCellX = 0;
    int mSpawnCellZ = 0;
    int mDestinationCellX = 0;
    int mDestinationCellZ = 0;
    float mNoiseLowLimit = -0.17f;
    float mNoiseHighLimit = 0.17f;
    float mAggressiveness = kDefaultAggressiveness;
};

#include "Terrain.hpp"
#include <algorithm>
#include <cmath>
#include <random>

Terrain::Terrain()
    : mDivisions(kDefaultDivisions)
    , mMapSize(kDefaultMapSize)
    , mHeights(static_cast<std::size_t>((mDivisions + 1) * (mDivisions + 1)), 1.f)
{
}

void Terrain::generate()
{
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_real_distribution<float> centerDist(-0.65f, 0.65f);
    static std::uniform_real_distribution<float> radiusDist(0.18f, 0.48f);
    static std::uniform_real_distribution<float> hillAmpDist(0.09f, 0.28f);
    static std::uniform_real_distribution<float> holeAmpDist(0.08f, 0.25f);
    static std::uniform_int_distribution<int> hillCountDist(3, 6);
    static std::uniform_int_distribution<int> holeCountDist(1, 4);
    std::uniform_real_distribution<float> coarseNoise(mNoiseLowLimit, mNoiseHighLimit);
    const int divisions = mDivisions;
    auto flattenBorders = [this, divisions]() {
        for (int i = 0; i <= divisions; ++i)
        {
            mHeights[index(i, 0)] = 1.f;
            mHeights[index(i, divisions)] = 1.f;
            mHeights[index(0, i)] = 1.f;
            mHeights[index(divisions, i)] = 1.f;
        }
    };

    const std::size_t expectedCount = static_cast<std::size_t>((divisions + 1) * (divisions + 1));
    if (mHeights.size() != expectedCount)
        mHeights.assign(expectedCount, 1.f);

    const int coarseSize = 6;
    std::vector<float> coarse(static_cast<std::size_t>((coarseSize + 1) * (coarseSize + 1)));
    auto coarseIndex = [coarseSize](int x, int z) -> std::size_t {
        return static_cast<std::size_t>(z * (coarseSize + 1) + x);
    };

    for (int z = 0; z <= coarseSize; ++z)
        for (int x = 0; x <= coarseSize; ++x)
            coarse[coarseIndex(x, z)] = coarseNoise(rng);

    auto smoothStep = [](float t) -> float {
        return t * t * (3.f - 2.f * t);
    };

    auto sampleCoarseNoise = [&](float u, float v) -> float {
        const float gx = u * static_cast<float>(coarseSize);
        const float gz = v * static_cast<float>(coarseSize);

        const int x0 = std::clamp(static_cast<int>(std::floor(gx)), 0, coarseSize - 1);
        const int z0 = std::clamp(static_cast<int>(std::floor(gz)), 0, coarseSize - 1);
        const int x1 = x0 + 1;
        const int z1 = z0 + 1;

        const float tx = smoothStep(gx - static_cast<float>(x0));
        const float tz = smoothStep(gz - static_cast<float>(z0));

        const float a = coarse[coarseIndex(x0, z0)];
        const float b = coarse[coarseIndex(x1, z0)];
        const float c = coarse[coarseIndex(x0, z1)];
        const float d = coarse[coarseIndex(x1, z1)];

        const float ab = a + (b - a) * tx;
        const float cd = c + (d - c) * tx;
        return ab + (cd - ab) * tz;
    };

    struct Feature
    {
        float cx;
        float cz;
        float radius;
        float amplitude;
    };

    std::vector<Feature> hills;
    std::vector<Feature> holes;
    hills.reserve(4);
    holes.reserve(2);

    const int hillCount = hillCountDist(rng);
    const int holeCount = holeCountDist(rng);
    for (int i = 0; i < hillCount; ++i)
        hills.push_back({centerDist(rng), centerDist(rng), radiusDist(rng), hillAmpDist(rng)});
    for (int i = 0; i < holeCount; ++i)
        holes.push_back({centerDist(rng), centerDist(rng), radiusDist(rng), holeAmpDist(rng)});

    for (int z = 0; z <= divisions; ++z)
    {
        for (int x = 0; x <= divisions; ++x)
        {
            if (x == 0 || z == 0 || x == divisions || z == divisions)
            {
                mHeights[index(x, z)] = 1.f;
                continue;
            }

            const float u = static_cast<float>(x) / static_cast<float>(divisions);
            const float v = static_cast<float>(z) / static_cast<float>(divisions);
            const float nx = u * 2.f - 1.f;
            const float nz = v * 2.f - 1.f;

            constexpr float coarseGain = 1.55f;
            constexpr float hillGain = 1.20f;
            constexpr float holeGain = 1.30f;
            float h = 1.f + sampleCoarseNoise(u, v) * coarseGain;

            for (const auto& hill : hills)
            {
                const float dx = nx - hill.cx;
                const float dz = nz - hill.cz;
                const float d2 = dx * dx + dz * dz;
                const float r2 = hill.radius * hill.radius;
                h += hill.amplitude * hillGain * std::exp(-d2 / (2.f * r2));
            }

            for (const auto& hole : holes)
            {
                const float dx = nx - hole.cx;
                const float dz = nz - hole.cz;
                const float d2 = dx * dx + dz * dz;
                const float r2 = hole.radius * hole.radius;
                h -= hole.amplitude * holeGain * std::exp(-d2 / (2.f * r2));
            }

            mHeights[index(x, z)] = std::clamp(h, 0.62f, 1.58f);
        }
    }

    std::vector<float> scratch = mHeights;
    for (int pass = 0; pass < 1; ++pass)
    {
        for (int z = 1; z < divisions; ++z)
        {
            for (int x = 1; x < divisions; ++x)
            {
                const float c = mHeights[index(x, z)] * 3.f;
                const float n = mHeights[index(x, z - 1)];
                const float s = mHeights[index(x, z + 1)];
                const float w = mHeights[index(x - 1, z)];
                const float e = mHeights[index(x + 1, z)];
                scratch[index(x, z)] = (c + n + s + w + e) / 7.f;
            }
        }
        mHeights.swap(scratch);
    }

    constexpr float maxStep = 0.12f;
    for (int iter = 0; iter < 2; ++iter)
    {
        for (int z = 1; z < divisions; ++z)
        {
            for (int x = 1; x < divisions; ++x)
            {
                float h = mHeights[index(x, z)];
                const float neighbors[] = {
                    mHeights[index(x - 1, z)],
                    mHeights[index(x + 1, z)],
                    mHeights[index(x, z - 1)],
                    mHeights[index(x, z + 1)]
                };

                for (float n : neighbors)
                {
                    if (h > n + maxStep)
                        h = n + maxStep;
                    if (h < n - maxStep)
                        h = n - maxStep;
                }

                mHeights[index(x, z)] = std::clamp(h, 0.64f, 1.56f);
            }
        }
    }

    flattenBorders();

    // Amplify relief around the baseline height. This is controlled by the editor slider.
    const float reliefScale = std::clamp(mAggressiveness, kMinAggressiveness, kMaxAggressiveness);
    const float amplifiedMin = 1.f - 0.22f * reliefScale;
    const float amplifiedMax = 1.f + 0.38f * reliefScale;
    for (int z = 1; z < divisions; ++z)
    {
        for (int x = 1; x < divisions; ++x)
        {
            const float h = mHeights[index(x, z)];
            const float amplified = 1.f + (h - 1.f) * reliefScale;
            mHeights[index(x, z)] = std::clamp(amplified, amplifiedMin, amplifiedMax);
        }
    }

    // One light blend pass to reduce harsh peaks/holes and make the surface look organic.
    std::vector<float> naturalized = mHeights;
    const float finalMinHeight = 1.f - 0.18f * reliefScale;
    const float finalMaxHeight = 1.f + 0.32f * reliefScale;
    for (int z = 1; z < divisions; ++z)
    {
        for (int x = 1; x < divisions; ++x)
        {
            const float c = mHeights[index(x, z)] * 5.f;
            const float n = mHeights[index(x, z - 1)];
            const float s = mHeights[index(x, z + 1)];
            const float w = mHeights[index(x - 1, z)];
            const float e = mHeights[index(x + 1, z)];
            const float nw = mHeights[index(x - 1, z - 1)] * 0.5f;
            const float ne = mHeights[index(x + 1, z - 1)] * 0.5f;
            const float sw = mHeights[index(x - 1, z + 1)] * 0.5f;
            const float se = mHeights[index(x + 1, z + 1)] * 0.5f;
            const float smoothed = (c + n + s + w + e + nw + ne + sw + se) / 11.f;
            naturalized[index(x, z)] = std::clamp(smoothed, finalMinHeight, finalMaxHeight);
        }
    }
    mHeights.swap(naturalized);

    flattenBorders();

    mGenerated = true;
}

void Terrain::makeFlat(float value)
{
    std::fill(mHeights.begin(), mHeights.end(), value);
    mGenerated = false;
}

void Terrain::setNoiseRange(float low, float high)
{
    low = std::clamp(low, -0.8f, 0.8f);
    high = std::clamp(high, -0.8f, 0.8f);


    if (low > high)
        std::swap(low, high);

    mNoiseLowLimit = low;
    mNoiseHighLimit = high;
}

void Terrain::setDivisions(int divisions)
{
    const int clamped = std::clamp(divisions, kMinDivisions, kMaxDivisions);
    if (clamped == mDivisions)
        return;

    mDivisions = clamped;
    mHeights.assign(static_cast<std::size_t>((mDivisions + 1) * (mDivisions + 1)), 1.f);
    mGenerated = false;

    if (mHasSpawnCell)
    {
        mSpawnCellX = clampCell(mSpawnCellX);
        mSpawnCellZ = clampCell(mSpawnCellZ);
    }

    if (mHasDestinationCell)
    {
        mDestinationCellX = clampCell(mDestinationCellX);
        mDestinationCellZ = clampCell(mDestinationCellZ);
    }
}

void Terrain::setMapSize(float mapSize)
{
    mMapSize = std::clamp(mapSize, kMinMapSize, kMaxMapSize);
}

void Terrain::setAggressiveness(float aggressiveness)
{
    mAggressiveness = std::clamp(aggressiveness, kMinAggressiveness, kMaxAggressiveness);
}

float Terrain::heightAt(int x, int z) const
{
    x = std::clamp(x, 0, mDivisions);
    z = std::clamp(z, 0, mDivisions);
    return mHeights[index(x, z)];
}

int Terrain::divisions() const
{
    return mDivisions;
}

float Terrain::mapSize() const
{
    return mMapSize;
}

bool Terrain::isGenerated() const
{
    return mGenerated;
}

float Terrain::noiseLowLimit() const
{
    return mNoiseLowLimit;
}

float Terrain::noiseHighLimit() const
{
    return mNoiseHighLimit;
}

float Terrain::aggressiveness() const
{
    return mAggressiveness;
}

void Terrain::setSpawnCell(int x, int z)
{
    mSpawnCellX = clampCell(x);
    mSpawnCellZ = clampCell(z);
    mHasSpawnCell = true;
}

void Terrain::setDestinationCell(int x, int z)
{
    mDestinationCellX = clampCell(x);
    mDestinationCellZ = clampCell(z);
    mHasDestinationCell = true;
}

bool Terrain::hasSpawnCell() const
{
    return mHasSpawnCell;
}

bool Terrain::hasDestinationCell() const
{
    return mHasDestinationCell;
}

int Terrain::spawnCellX() const
{
    return mSpawnCellX;
}

int Terrain::spawnCellZ() const
{
    return mSpawnCellZ;
}

int Terrain::destinationCellX() const
{
    return mDestinationCellX;
}

int Terrain::destinationCellZ() const
{
    return mDestinationCellZ;
}

std::size_t Terrain::index(int x, int z) const
{
    return static_cast<std::size_t>(z * (mDivisions + 1) + x);
}

int Terrain::clampCell(int value) const
{
    return std::clamp(value, 0, mDivisions - 1);
}

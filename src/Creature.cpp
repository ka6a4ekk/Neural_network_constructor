#include "Creature.hpp"
#include <algorithm>
#include <utility>

void Creature::clear()
{
    mBlocks.clear();
    mConnections.clear();
}

void Creature::setData(std::vector<Block> blocks, std::vector<Connection> connections)
{
    mBlocks = std::move(blocks);
    mConnections = std::move(connections);
}

const std::vector<Creature::Block>& Creature::blocks() const
{
    return mBlocks;
}

const std::vector<Creature::Connection>& Creature::connections() const
{
    return mConnections;
}

bool Creature::empty() const
{
    return mBlocks.empty();
}

std::size_t Creature::limbConnectionCount() const
{
    return static_cast<std::size_t>(std::count_if(
        mConnections.begin(),
        mConnections.end(),
        [this](const Connection& connection) {
            if (connection.type != ConnectionType::Limb)
                return false;
            if (connection.from >= mBlocks.size() || connection.to >= mBlocks.size())
                return false;
            return connection.from != connection.to;
        }
    ));
}

std::size_t Creature::recommendedOutputCount() const
{
    if (mBlocks.empty())
        return 0;
    return std::max<std::size_t>(1, limbConnectionCount());
}

std::size_t Creature::recommendedInputCount() const
{
    const std::size_t outputCount = recommendedOutputCount();
    if (outputCount == 0)
        return 0;
    return kBaseSensorInputCount + outputCount;
}

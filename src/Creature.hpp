#pragma once

#include <cstddef>
#include <vector>

class Creature
{
public:
    static constexpr std::size_t kBaseSensorInputCount = 6;

    enum class ConnectionType
    {
        Torso,
        Limb
    };

    struct Block
    {
        int gx = 0;
        int gy = 0;
    };

    struct Connection
    {
        std::size_t from = 0;
        std::size_t to = 0;
        ConnectionType type = ConnectionType::Limb;
    };

public:
    void clear();
    void setData(std::vector<Block> blocks, std::vector<Connection> connections);
    const std::vector<Block>& blocks() const;
    const std::vector<Connection>& connections() const;

    bool empty() const;
    std::size_t limbConnectionCount() const;
    std::size_t recommendedOutputCount() const;
    std::size_t recommendedInputCount() const;
private:
    std::vector<Block> mBlocks;
    std::vector<Connection> mConnections;
};

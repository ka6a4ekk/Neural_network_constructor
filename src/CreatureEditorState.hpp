#pragma once

#include "State.hpp"
#include "Creature.hpp"
#include <SFML/Graphics.hpp>
#include <optional>
#include <vector>

class CreatureEditorState : public State
{
public:
    CreatureEditorState(StateStack& stack, Context context);

    void handleEvent(const sf::Event& event) override;
    void update(sf::Time dt) override;
    void render() override;

private:
    struct Block
    {
        sf::RectangleShape shape;
        sf::Vector2i grid;
    };

    struct Connection
    {
        std::size_t from = 0;
        std::size_t to = 0;
        Creature::ConnectionType type = Creature::ConnectionType::Limb;
    };

private:
    sf::Vector2i worldToGrid(sf::Vector2f point) const;
    sf::Vector2f gridToWorldCenter(sf::Vector2i grid) const;
    bool isGridInsideCanvas(sf::Vector2i grid) const;
    std::optional<std::size_t> findBlockAt(sf::Vector2f point) const;
    std::optional<std::size_t> findBlockByGrid(sf::Vector2i grid) const;
    void addBlockAt(sf::Vector2f point);
    void addConnection(std::size_t from, std::size_t to, Creature::ConnectionType type);
    bool hasConnection(std::size_t from, std::size_t to, Creature::ConnectionType type) const;
    void removeSelectedBlock();
    sf::Color connectionColor(Creature::ConnectionType type) const;
    void setupBlockVisual(Block& block) const;
    void syncFromSharedCreature();
    void syncToSharedCreature() const;

private:
    static constexpr float kCellSize = 40.f;
    static constexpr float kBlockSize = 30.f;
    static constexpr float kGridOriginX = 40.f;
    static constexpr float kGridOriginY = 140.f;

    std::vector<Block> mBlocks;
    std::vector<Connection> mConnections;
    std::optional<std::size_t> mSelectedBlock;
    std::optional<std::size_t> mDragFromBlock;
    sf::Vector2f mDragMousePos{0.f, 0.f};
    Creature::ConnectionType mDragConnectionType = Creature::ConnectionType::Limb;
    bool mShiftPressed = false;

    sf::Font mFont;
    sf::Text mTitleText;
};

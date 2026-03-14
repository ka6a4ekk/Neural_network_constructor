#include "CreatureEditorState.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

CreatureEditorState::CreatureEditorState(StateStack& stack, Context context)
    : State(stack, context)
    , mFont()
    , mTitleText(mFont)
{
    if (!mFont.openFromFile("res/fonts/font.ttf"))
        throw std::runtime_error("Failed to load font");

    mTitleText.setString("Creature Designer");
    mTitleText.setCharacterSize(34);
    mTitleText.setFillColor(sf::Color::White);
    mTitleText.setPosition({24.f, 24.f});

    syncFromSharedCreature();
}

void CreatureEditorState::handleEvent(const sf::Event& event)
{
    const auto toWorld = [](sf::Vector2i pixel) -> sf::Vector2f {
        return sf::Vector2f(static_cast<float>(pixel.x), static_cast<float>(pixel.y));
    };

    if (event.is<sf::Event::KeyPressed>())
    {
        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            if (key->code == sf::Keyboard::Key::Escape)
            {
                requestPop();
                return;
            }

            if (key->code == sf::Keyboard::Key::LShift || key->code == sf::Keyboard::Key::RShift)
            {
                mShiftPressed = true;
                return;
            }

            if (key->code == sf::Keyboard::Key::Delete || key->code == sf::Keyboard::Key::Backspace)
            {
                removeSelectedBlock();
                return;
            }
        }
    }

    if (event.is<sf::Event::KeyReleased>())
    {
        if (const auto* key = event.getIf<sf::Event::KeyReleased>())
        {
            if (key->code == sf::Keyboard::Key::LShift || key->code == sf::Keyboard::Key::RShift)
            {
                mShiftPressed = false;
                return;
            }
        }
    }

    if (event.is<sf::Event::MouseButtonPressed>())
    {
        if (const auto* mouse = event.getIf<sf::Event::MouseButtonPressed>())
        {
            const sf::Vector2f mousePos = toWorld(mouse->position);
            const auto clickedBlock = findBlockAt(mousePos);

            if (mouse->button == sf::Mouse::Button::Left)
            {
                if (clickedBlock.has_value())
                {
                    mSelectedBlock = clickedBlock;
                }
                else
                {
                    addBlockAt(mousePos);
                }
                return;
            }

            if (mouse->button == sf::Mouse::Button::Right)
            {
                if (clickedBlock.has_value())
                {
                    mDragFromBlock = clickedBlock;
                    mSelectedBlock = clickedBlock;
                    mDragMousePos = mousePos;
                    mDragConnectionType = mShiftPressed
                        ? Creature::ConnectionType::Torso
                        : Creature::ConnectionType::Limb;
                }
                else
                {
                    mDragFromBlock.reset();
                }
                return;
            }
        }
    }

    if (event.is<sf::Event::MouseMoved>())
    {
        if (const auto* mouse = event.getIf<sf::Event::MouseMoved>())
        {
            if (mDragFromBlock.has_value())
                mDragMousePos = toWorld(mouse->position);
        }
    }

    if (event.is<sf::Event::MouseButtonReleased>())
    {
        if (const auto* mouse = event.getIf<sf::Event::MouseButtonReleased>())
        {
            if (mouse->button != sf::Mouse::Button::Right)
                return;

            if (!mDragFromBlock.has_value())
                return;

            const sf::Vector2f mousePos = toWorld(mouse->position);
            const auto targetBlock = findBlockAt(mousePos);
            if (targetBlock.has_value() && targetBlock.value() != mDragFromBlock.value())
                addConnection(mDragFromBlock.value(), targetBlock.value(), mDragConnectionType);

            mDragFromBlock.reset();
        }
    }
}

void CreatureEditorState::update(sf::Time)
{
}

void CreatureEditorState::render()
{
    const sf::Vector2u size = mContext.window->getSize();
    (void)size;
    sf::VertexArray grid(sf::PrimitiveType::Lines);
    for (float x = kGridOriginX; x < static_cast<float>(size.x); x += kCellSize)
    {
        sf::Vertex v1;
        v1.position = {x, kGridOriginY};
        v1.color = sf::Color(25, 25, 26);
        grid.append(v1);

        sf::Vertex v2;
        v2.position = {x, static_cast<float>(size.y)};
        v2.color = sf::Color(25, 25, 26);
        grid.append(v2);
    }
    for (float y = kGridOriginY; y < static_cast<float>(size.y); y += kCellSize)
    {
        sf::Vertex v1;
        v1.position = {kGridOriginX, y};
        v1.color = sf::Color(25, 25, 26);
        grid.append(v1);

        sf::Vertex v2;
        v2.position = {static_cast<float>(size.x), y};
        v2.color = sf::Color(25, 25, 26);
        grid.append(v2);
    }
    mContext.window->draw(grid);

    for (const auto& connection : mConnections)
    {
        const sf::Vector2f from = mBlocks[connection.from].shape.getPosition();
        const sf::Vector2f to = mBlocks[connection.to].shape.getPosition();
        sf::Vertex line[2];
        line[0].position = from;
        line[0].color = connectionColor(connection.type);
        line[1].position = to;
        line[1].color = connectionColor(connection.type);
        mContext.window->draw(line, 2, sf::PrimitiveType::Lines);
    }

    if (mDragFromBlock.has_value())
    {
        const sf::Vector2f from = mBlocks[mDragFromBlock.value()].shape.getPosition();
        const sf::Color color = connectionColor(mDragConnectionType);
        sf::Vertex line[2];
        line[0].position = from;
        line[0].color = color;
        line[1].position = mDragMousePos;
        line[1].color = color;
        mContext.window->draw(line, 2, sf::PrimitiveType::Lines);
    }

    for (std::size_t i = 0; i < mBlocks.size(); ++i)
    {
        auto block = mBlocks[i].shape;
        if (mSelectedBlock.has_value() && mSelectedBlock.value() == i)
            block.setOutlineColor(sf::Color::Yellow);
        mContext.window->draw(block);
    }

    mContext.window->draw(mTitleText);
}

sf::Vector2i CreatureEditorState::worldToGrid(sf::Vector2f point) const
{
    const float gx = (point.x - kGridOriginX) / kCellSize;
    const float gy = (point.y - kGridOriginY) / kCellSize;
    return sf::Vector2i(
        static_cast<int>(std::floor(gx)),
        static_cast<int>(std::floor(gy))
    );
}

sf::Vector2f CreatureEditorState::gridToWorldCenter(sf::Vector2i grid) const
{
    return sf::Vector2f(
        kGridOriginX + (static_cast<float>(grid.x) + 0.5f) * kCellSize,
        kGridOriginY + (static_cast<float>(grid.y) + 0.5f) * kCellSize
    );
}

bool CreatureEditorState::isGridInsideCanvas(sf::Vector2i grid) const
{
    if (grid.x < 0 || grid.y < 0)
        return false;

    const sf::Vector2u size = mContext.window->getSize();
    const float maxX = kGridOriginX + static_cast<float>(grid.x + 1) * kCellSize;
    const float maxY = kGridOriginY + static_cast<float>(grid.y + 1) * kCellSize;
    return maxX <= static_cast<float>(size.x) && maxY <= static_cast<float>(size.y);
}

std::optional<std::size_t> CreatureEditorState::findBlockAt(sf::Vector2f point) const
{
    for (std::size_t i = 0; i < mBlocks.size(); ++i)
    {
        if (mBlocks[i].shape.getGlobalBounds().contains(point))
            return i;
    }
    return std::nullopt;
}

std::optional<std::size_t> CreatureEditorState::findBlockByGrid(sf::Vector2i grid) const
{
    for (std::size_t i = 0; i < mBlocks.size(); ++i)
    {
        if (mBlocks[i].grid == grid)
            return i;
    }
    return std::nullopt;
}

void CreatureEditorState::addBlockAt(sf::Vector2f point)
{
    const sf::Vector2i grid = worldToGrid(point);
    if (!isGridInsideCanvas(grid))
        return;
    if (findBlockByGrid(grid).has_value())
        return;

    Block block;
    block.grid = grid;
    setupBlockVisual(block);
    mBlocks.push_back(block);
    mSelectedBlock = mBlocks.size() - 1;
    syncToSharedCreature();
}

void CreatureEditorState::addConnection(std::size_t from, std::size_t to, Creature::ConnectionType type)
{
    if (from == to || hasConnection(from, to, type))
        return;

    Connection connection;
    connection.from = from;
    connection.to = to;
    connection.type = type;
    mConnections.push_back(connection);
    syncToSharedCreature();
}

bool CreatureEditorState::hasConnection(std::size_t from, std::size_t to, Creature::ConnectionType type) const
{
    return std::any_of(mConnections.begin(), mConnections.end(),
                       [from, to, type](const Connection& c) {
                           const bool samePair = (c.from == from && c.to == to)
                               || (c.from == to && c.to == from);
                           return samePair && c.type == type;
                       });
}

void CreatureEditorState::setupBlockVisual(Block& block) const
{
    block.shape = sf::RectangleShape({kBlockSize, kBlockSize});
    block.shape.setOrigin({kBlockSize / 2.f, kBlockSize / 2.f});
    block.shape.setPosition(gridToWorldCenter(block.grid));
    block.shape.setFillColor(sf::Color(25, 25, 26));
    block.shape.setOutlineThickness(2.f);
    block.shape.setOutlineColor(sf::Color(54, 54, 54));
}

void CreatureEditorState::removeSelectedBlock()
{
    if (!mSelectedBlock.has_value())
        return;

    const std::size_t removeIndex = mSelectedBlock.value();
    mSelectedBlock.reset();
    mDragFromBlock.reset();

    mConnections.erase(
        std::remove_if(mConnections.begin(), mConnections.end(),
                       [removeIndex](const Connection& c) {
                           return c.from == removeIndex || c.to == removeIndex;
                       }),
        mConnections.end()
    );

    for (auto& c : mConnections)
    {
        if (c.from > removeIndex)
            --c.from;
        if (c.to > removeIndex)
            --c.to;
    }

    mBlocks.erase(mBlocks.begin() + static_cast<std::ptrdiff_t>(removeIndex));
    syncToSharedCreature();
}

sf::Color CreatureEditorState::connectionColor(Creature::ConnectionType type) const
{
    if (type == Creature::ConnectionType::Torso)
        return sf::Color(80, 150, 255); // Blue torso/meat connection.
    return sf::Color(255, 140, 40); // Orange limb connection.
}

void CreatureEditorState::syncFromSharedCreature()
{
    if (mContext.creature == nullptr)
        return;

    mBlocks.clear();
    mConnections.clear();
    mSelectedBlock.reset();
    mDragFromBlock.reset();

    const auto& sharedBlocks = mContext.creature->blocks();
    const auto& sharedConnections = mContext.creature->connections();

    mBlocks.reserve(sharedBlocks.size());
    for (const auto& blockData : sharedBlocks)
    {
        Block block;
        block.grid = sf::Vector2i(blockData.gx, blockData.gy);
        setupBlockVisual(block);
        mBlocks.push_back(block);
    }

    mConnections.reserve(sharedConnections.size());
    for (const auto& c : sharedConnections)
    {
        if (c.from >= mBlocks.size() || c.to >= mBlocks.size())
            continue;

        Connection connection;
        connection.from = c.from;
        connection.to = c.to;
        connection.type = c.type;
        mConnections.push_back(connection);
    }

}

void CreatureEditorState::syncToSharedCreature() const
{
    if (mContext.creature == nullptr)
        return;

    std::vector<Creature::Block> blocks;
    blocks.reserve(mBlocks.size());
    for (const auto& block : mBlocks)
    {
        Creature::Block out;
        out.gx = block.grid.x;
        out.gy = block.grid.y;
        blocks.push_back(out);
    }

    std::vector<Creature::Connection> connections;
    connections.reserve(mConnections.size());
    for (const auto& connection : mConnections)
    {
        Creature::Connection out;
        out.from = connection.from;
        out.to = connection.to;
        out.type = connection.type;
        connections.push_back(out);
    }

    mContext.creature->setData(std::move(blocks), std::move(connections));
}

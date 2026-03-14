#include "NNConfigState.hpp"
#include "Creature.hpp"
#include "NeuralNetwork.hpp"
#include <algorithm>
#include <limits>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>

NNConfigState::NNConfigState(StateStack& stack, Context context)
    : State(stack, context)
    , mFont()
    , mTitleText(mFont)
    , mCompileStatusText(mFont)
    , mStatusText(mFont)
{
    if (!mFont.openFromFile("res/fonts/font.ttf"))
        throw std::runtime_error("Failed to load font");

    mTitleText.setString("Neural Network Layout Editor");
    mTitleText.setCharacterSize(34);
    mTitleText.setFillColor(sf::Color::White);
    mTitleText.setPosition({26.f, 24.f});

    mCompileStatusText.setCharacterSize(17);
    mCompileStatusText.setFillColor(sf::Color::White);
    mCompileStatusText.setPosition({26.f, 72.f});

    mStatusText.setCharacterSize(16);
    mStatusText.setFillColor(sf::Color::White);
    mStatusText.setPosition({26.f, 118.f});
    mStatusText.setString("Status: not compiled");

    mCompileButton.emplace(mFont, "Compile", "compile_layout");
    mCompileButton->setSize({160.f, 40.f});
    // mCompileButton->setPosition({420.f, 84.f});
    mCompileButton->setPosition({420.f, 100.f});
    mCompileButton->setNormalColor(sf::Color(25, 25, 26));
    mCompileButton->setHoverColor(sf::Color(54, 54, 54));
    mCompileButton->setPressedColor(sf::Color(54, 54, 54));
    mCompileButton->setOutline(0.f, sf::Color(54, 54, 54));
    mCompileButton->setCharacterSize(22);
    mCompileButton->setOnActivate([this]() {
    const bool ok = compileLayout();
    if (ok==true)
    {
        mStatusText.setString("Status: compiled");
    }
    else mStatusText.setString("Status: compile failed");
    updateHudText();
    });

    loadLayoutFromShared();
    updateHudText();
}

const char* NNConfigState::roleName(Neuron::Role role)
{
    switch (role)
    {
    case Neuron::Role::Input:
        return "Input";
    case Neuron::Role::Output:
        return "Output";
    case Neuron::Role::Bias:
        return "Bias";
    case Neuron::Role::Hidden:
    default:
        return "Hidden";
    }
}

const char* NNConfigState::roleShortName(Neuron::Role role)
{
    switch (role)
    {
    case Neuron::Role::Input:
        return "I";
    case Neuron::Role::Output:
        return "O";
    case Neuron::Role::Bias:
        return "B";
    case Neuron::Role::Hidden:
    default:
        return "H";
    }
}

sf::Color NNConfigState::roleFillColor(Neuron::Role role)
{
    switch (role)
    {
    case Neuron::Role::Input:
        return sf::Color(42, 92, 42);
    case Neuron::Role::Output:
        return sf::Color(118, 40, 40);
    case Neuron::Role::Bias:
        return sf::Color(130, 104, 26);
    case Neuron::Role::Hidden:
    default:
        return sf::Color(35, 35, 45);
    }
}

NeuralNetwork::NodeRole NNConfigState::toNetworkRole(Neuron::Role role)
{
    switch (role)
    {
    case Neuron::Role::Input:
        return NeuralNetwork::NodeRole::Input;
    case Neuron::Role::Output:
        return NeuralNetwork::NodeRole::Output;
    case Neuron::Role::Bias:
        return NeuralNetwork::NodeRole::Bias;
    case Neuron::Role::Hidden:
    default:
        return NeuralNetwork::NodeRole::Hidden;
    }
}

NNConfigState::Neuron::Role NNConfigState::fromNetworkRole(NeuralNetwork::NodeRole role)
{
    switch (role)
    {
    case NeuralNetwork::NodeRole::Input:
        return Neuron::Role::Input;
    case NeuralNetwork::NodeRole::Output:
        return Neuron::Role::Output;
    case NeuralNetwork::NodeRole::Bias:
        return Neuron::Role::Bias;
    case NeuralNetwork::NodeRole::Hidden:
    default:
        return Neuron::Role::Hidden;
    }
}

void NNConfigState::handleEvent(const sf::Event& event)
{
    const auto toWorld = [](sf::Vector2i pixel) -> sf::Vector2f {
        return sf::Vector2f(static_cast<float>(pixel.x), static_cast<float>(pixel.y));
    };

    if (mCompileButton.has_value())
        mCompileButton->handleEvent(event);

    if (event.is<sf::Event::KeyPressed>())
    {
        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            if (key->code == sf::Keyboard::Key::Escape)
            {
                saveLayoutToShared();
                compileLayout();
                requestPop();
                return;
            }

            if (key->code == sf::Keyboard::Key::Delete || key->code == sf::Keyboard::Key::Backspace)
            {
                removeSelectedNeuron();
                updateHudText();
                return;
            }

            if (key->code == sf::Keyboard::Key::C)
            {
                mNeurons.clear();
                mWires.clear();
                mSelectedNeuron.reset();
                mDragFromNeuron.reset();
                clearLayerSelections();
                invalidateCompiled("Compile: layout changed");
                updateHudText();
                return;
            }

            if (key->code == sf::Keyboard::Key::Enter)
            {
                autoWireSelectedLayers();
                updateHudText();
                return;
            }

            if (key->code == sf::Keyboard::Key::F)
            {
                compileLayout();
                updateHudText();
                return;
            }

            if (key->code == sf::Keyboard::Key::I)
            {
                assignRoleToSelected(Neuron::Role::Input);
                updateHudText();
                return;
            }

            if (key->code == sf::Keyboard::Key::O)
            {
                assignRoleToSelected(Neuron::Role::Output);
                updateHudText();
                return;
            }

            if (key->code == sf::Keyboard::Key::B)
            {
                assignRoleToSelected(Neuron::Role::Bias);
                updateHudText();
                return;
            }

            if (key->code == sf::Keyboard::Key::H)
            {
                assignRoleToSelected(Neuron::Role::Hidden);
                updateHudText();
                return;
            }
        }
    }

    if (event.is<sf::Event::MouseButtonPressed>())
    {
        if (const auto* mouse = event.getIf<sf::Event::MouseButtonPressed>())
        {
            const sf::Vector2f mousePos = toWorld(mouse->position);
            const auto clickedNeuron = findNeuronAt(mousePos);

            if (mouse->button == sf::Mouse::Button::Left)
            {
                if (mCompileButton.has_value() && mCompileButton->contains(mousePos))
                    return;

                if (!clickedNeuron.has_value())
                {
                    addNeuron(mousePos);
                    mSelectedNeuron = mNeurons.empty()
                        ? std::nullopt
                        : std::optional<std::size_t>(mNeurons.size() - 1);
                    clearLayerSelections();
                }
                else
                {
                    mSelectedNeuron = clickedNeuron;
                }
                updateHudText();
                return;
            }

            if (mouse->button == sf::Mouse::Button::Right)
            {
                if (clickedNeuron.has_value())
                {
                    mDragFromNeuron = clickedNeuron;
                    mDragMousePos = mousePos;
                    mSelectedNeuron = clickedNeuron;
                }
                else
                {
                    mDragFromNeuron.reset();
                }
                updateHudText();
                return;
            }

            if (mouse->button == sf::Mouse::Button::Middle)
            {
                mLayerSelecting = true;
                mLayerSelectStart = mousePos;
                mLayerSelectCurrent = mousePos;
                return;
            }
        }
    }

    if (event.is<sf::Event::MouseMoved>())
    {
        if (const auto* mouse = event.getIf<sf::Event::MouseMoved>())
        {
            if (mDragFromNeuron.has_value())
            {
                mDragMousePos = toWorld(mouse->position);
            }

            if (mLayerSelecting)
                mLayerSelectCurrent = toWorld(mouse->position);
        }
    }

    if (event.is<sf::Event::MouseButtonReleased>())
    {
        if (const auto* mouse = event.getIf<sf::Event::MouseButtonReleased>())
        {
            if (mouse->button == sf::Mouse::Button::Right)
            {
                if (!mDragFromNeuron.has_value())
                    return;

                const sf::Vector2f mousePos = toWorld(mouse->position);
                const auto targetNeuron = findNeuronAt(mousePos);

                if (targetNeuron.has_value() && targetNeuron.value() != mDragFromNeuron.value())
                    addWire(mDragFromNeuron.value(), targetNeuron.value());

                mDragFromNeuron.reset();
                updateHudText();
                return;
            }

            if (mouse->button != sf::Mouse::Button::Middle || !mLayerSelecting)
                return;

            mLayerSelecting = false;
            mLayerSelectCurrent = toWorld(mouse->position);
            addLayerSelection(makeSelectionRect(mLayerSelectStart, mLayerSelectCurrent));
            updateHudText();
        }
    }
}

void NNConfigState::update(sf::Time dt)
{
    if (mCompileButton.has_value())
        mCompileButton->update(dt);
}

void NNConfigState::render()
{
    for (const auto& wire : mWires)
    {
        const sf::Vector2f from = mNeurons[wire.from].shape.getPosition();
        const sf::Vector2f to = mNeurons[wire.to].shape.getPosition();

        sf::Vertex line[2];
        line[0].position = from;
        line[0].color = sf::Color(215, 215, 215);
        line[1].position = to;
        line[1].color = sf::Color(215, 215, 215);
        mContext.window->draw(line, 2, sf::PrimitiveType::Lines);

    }

    if (mDragFromNeuron.has_value())
    {
        const sf::Vector2f from = mNeurons[mDragFromNeuron.value()].shape.getPosition();
        sf::Vertex previewLine[2];
        previewLine[0].position = from;
        previewLine[0].color = sf::Color(255, 255, 120);
        previewLine[1].position = mDragMousePos;
        previewLine[1].color = sf::Color(255, 255, 120);
        mContext.window->draw(previewLine, 2, sf::PrimitiveType::Lines);
    }

    for (std::size_t i = 0; i < mNeurons.size(); ++i)
    {
        auto neuron = mNeurons[i].shape;
        neuron.setFillColor(roleFillColor(mNeurons[i].role));
        if (mSelectedNeuron.has_value() && mSelectedNeuron.value() == i)
            neuron.setOutlineColor(sf::Color::Yellow);
        else
            neuron.setOutlineColor(sf::Color(120, 180, 255));

        mContext.window->draw(neuron);

        sf::Text roleText(mFont);
        roleText.setCharacterSize(14);
        roleText.setFillColor(sf::Color(245, 245, 245));
        roleText.setString(roleShortName(mNeurons[i].role));
        const auto bounds = roleText.getLocalBounds();
        roleText.setOrigin({
            bounds.position.x + bounds.size.x * 0.5f,
            bounds.position.y + bounds.size.y * 0.5f
        });
        roleText.setPosition(mNeurons[i].shape.getPosition());
        mContext.window->draw(roleText);
    }

    for (std::size_t i = 0; i < mLayerSelections.size(); ++i)
    {
        const auto& layer = mLayerSelections[i];
        sf::RectangleShape bounds;
        bounds.setPosition(layer.bounds.position);
        bounds.setSize(layer.bounds.size);
        bounds.setFillColor(sf::Color::Transparent);
        bounds.setOutlineThickness(2.f);
        bounds.setOutlineColor(sf::Color::Yellow);
        mContext.window->draw(bounds);
    }

    if (mLayerSelecting)
    {
        sf::RectangleShape dragBox;
        const sf::FloatRect dragRect = makeSelectionRect(mLayerSelectStart, mLayerSelectCurrent);
        dragBox.setPosition(dragRect.position);
        dragBox.setSize(dragRect.size);
        dragBox.setFillColor(sf::Color(255, 255, 0, 30));
        dragBox.setOutlineThickness(1.5f);
        dragBox.setOutlineColor(sf::Color::Yellow);
        mContext.window->draw(dragBox);
    }

    mContext.window->draw(mTitleText);
    mContext.window->draw(mCompileStatusText);
    mContext.window->draw(mStatusText);
    if (mCompileButton.has_value())
        mContext.window->draw(*mCompileButton);
}

sf::FloatRect NNConfigState::makeSelectionRect(sf::Vector2f a, sf::Vector2f b)
{
    const float left = std::min(a.x, b.x);
    const float top = std::min(a.y, b.y);
    const float width = std::abs(b.x - a.x);
    const float height = std::abs(b.y - a.y);
    return sf::FloatRect({left, top}, {width, height});
}

std::optional<std::size_t> NNConfigState::findNeuronAt(sf::Vector2f point) const
{
    for (std::size_t i = 0; i < mNeurons.size(); ++i)
    {
        const sf::Vector2f center = mNeurons[i].shape.getPosition();
        const float dx = center.x - point.x;
        const float dy = center.y - point.y;
        if ((dx * dx + dy * dy) <= (kNeuronRadius * kNeuronRadius))
            return i;
    }
    return std::nullopt;
}

std::vector<std::size_t> NNConfigState::findNeuronsInRect(const sf::FloatRect& rect) const
{
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < mNeurons.size(); ++i)
    {
        const sf::Vector2f center = mNeurons[i].shape.getPosition();
        if (rect.contains(center))
            indices.push_back(i);
    }
    return indices;
}

sf::FloatRect NNConfigState::computeLayerBounds(const std::vector<std::size_t>& neuronIndices) const
{
    if (neuronIndices.empty())
        return sf::FloatRect({0.f, 0.f}, {0.f, 0.f});

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();

    for (std::size_t index : neuronIndices)
    {
        const sf::Vector2f center = mNeurons[index].shape.getPosition();
        minX = std::min(minX, center.x - kNeuronRadius);
        minY = std::min(minY, center.y - kNeuronRadius);
        maxX = std::max(maxX, center.x + kNeuronRadius);
        maxY = std::max(maxY, center.y + kNeuronRadius);
    }

    return sf::FloatRect({minX, minY}, {maxX - minX, maxY - minY});
}

void NNConfigState::addLayerSelection(const sf::FloatRect& dragRect)
{
    auto indices = findNeuronsInRect(dragRect);
    if (indices.empty())
        return;

    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    // A neuron can belong to only one layer selection.
    std::vector<bool> alreadyAssigned(mNeurons.size(), false);
    for (const auto& layer : mLayerSelections)
    {
        for (std::size_t index : layer.neuronIndices)
            alreadyAssigned[index] = true;
    }

    indices.erase(
        std::remove_if(indices.begin(), indices.end(),
                       [&alreadyAssigned](std::size_t index) {
                           return alreadyAssigned[index];
                       }),
        indices.end()
    );

    if (indices.empty())
        return;

    LayerSelection layer;
    layer.neuronIndices = std::move(indices);
    layer.bounds = computeLayerBounds(layer.neuronIndices);
    mLayerSelections.push_back(std::move(layer));
}

void NNConfigState::autoWireSelectedLayers()
{
    if (mLayerSelections.size() < 2)
        return;

    for (std::size_t layer = 0; layer + 1 < mLayerSelections.size(); ++layer)
    {
        const auto& fromLayer = mLayerSelections[layer].neuronIndices;
        const auto& toLayer = mLayerSelections[layer + 1].neuronIndices;

        for (std::size_t from : fromLayer)
        {
            for (std::size_t to : toLayer)
                addWire(from, to);
        }
    }

    clearLayerSelections();
    invalidateCompiled("Compile: layout changed");
}

void NNConfigState::clearLayerSelections()
{
    mLayerSelections.clear();
    mLayerSelecting = false;
}

void NNConfigState::assignRoleToSelected(Neuron::Role role)
{
    if (!mSelectedNeuron.has_value())
    {
        mCompileMessage = "Compile: select a neuron before assigning role";
        mCompileMessageColor = sf::Color(240, 180, 120);
        mStatusText.setString("Status: select a neuron to assign role");
        return;
    }

    mNeurons[mSelectedNeuron.value()].role = role;
    invalidateCompiled("Compile: layout changed");
}

void NNConfigState::invalidateCompiled(std::string reason)
{
    mCompiledLayout.reset();
    if (mContext.neuralNetwork != nullptr)
        mContext.neuralNetwork->clear();
    mCompileMessage = std::move(reason);
    mCompileMessageColor = sf::Color(220, 220, 150);
    mStatusText.setString("Status: not compiled");
}

void NNConfigState::requiredNeuronCounts(std::size_t& inputCount, std::size_t& outputCount) const
{
    inputCount = 0;
    outputCount = 0;
    if (mContext.creature == nullptr || mContext.creature->empty())
        return;

    outputCount = mContext.creature->recommendedOutputCount();
    inputCount = mContext.creature->recommendedInputCount();
}

bool NNConfigState::compileLayout()
{
    mCompiledLayout.reset();
    if (mContext.neuralNetwork != nullptr)
        mContext.neuralNetwork->clear();
    if (mNeurons.empty())
    {
        mCompileMessage = "Compile failed: add at least one neuron";
        mCompileMessageColor = sf::Color(255, 140, 140);
        mStatusText.setString("Status: compile failed");
        return false;
    }

    const std::size_t neuronCount = mNeurons.size();
    std::vector<std::vector<std::size_t>> outgoing(neuronCount);
    std::vector<int> indegree(neuronCount, 0);
    for (const auto& wire : mWires)
    {
        if (wire.from >= neuronCount || wire.to >= neuronCount || wire.from == wire.to)
        {
            mCompileMessage = "Compile failed: invalid wire endpoints";
            mCompileMessageColor = sf::Color(255, 140, 140);
            mStatusText.setString("Status: compile failed");
            return false;
        }
        outgoing[wire.from].push_back(wire.to);
        ++indegree[wire.to];
    }

    std::vector<std::size_t> inputs;
    std::vector<std::size_t> outputs;
    std::vector<std::size_t> biases;
    inputs.reserve(neuronCount);
    outputs.reserve(neuronCount);
    biases.reserve(neuronCount);
    for (std::size_t i = 0; i < neuronCount; ++i)
    {
        switch (mNeurons[i].role)
        {
        case Neuron::Role::Input:
            inputs.push_back(i);
            break;
        case Neuron::Role::Output:
            outputs.push_back(i);
            break;
        case Neuron::Role::Bias:
            biases.push_back(i);
            break;
        case Neuron::Role::Hidden:
        default:
            break;
        }
    }

    std::size_t requiredInputCount = 0;
    std::size_t requiredOutputCount = 0;
    requiredNeuronCounts(requiredInputCount, requiredOutputCount);

    if (requiredInputCount == 0 || requiredOutputCount == 0)
    {
        mCompileMessage = "Compile failed: create a creature first (Creature editor)";
        mCompileMessageColor = sf::Color(255, 140, 140);
        mStatusText.setString("Status: compile failed");
        return false;
    }

    if (inputs.size() != requiredInputCount)
    {
        mCompileMessage = "Compile failed: Input neurons must be exactly "
            + std::to_string(requiredInputCount)
            + " (current: " + std::to_string(inputs.size()) + ")";
        mCompileMessageColor = sf::Color(255, 140, 140);
        mStatusText.setString("Status: compile failed");
        return false;
    }

    if (outputs.size() != requiredOutputCount)
    {
        mCompileMessage = "Compile failed: Output neurons must be exactly "
            + std::to_string(requiredOutputCount)
            + " (current: " + std::to_string(outputs.size()) + ")";
        mCompileMessageColor = sf::Color(255, 140, 140);
        mStatusText.setString("Status: compile failed");
        return false;
    }

    if (biases.empty())
    {
        mCompileMessage = "Compile failed: add at least one Bias neuron";
        mCompileMessageColor = sf::Color(255, 140, 140);
        mStatusText.setString("Status: compile failed");
        return false;
    }

    if (inputs.empty())
    {
        mCompileMessage = "Compile failed: mark at least one Input neuron";
        mCompileMessageColor = sf::Color(255, 140, 140);
        mStatusText.setString("Status: compile failed");
        return false;
    }

    if (outputs.empty())
    {
        mCompileMessage = "Compile failed: mark at least one Output neuron";
        mCompileMessageColor = sf::Color(255, 140, 140);
        mStatusText.setString("Status: compile failed");
        return false;
    }

    for (std::size_t biasIndex : biases)
    {
        if (indegree[biasIndex] != 0)
        {
            mCompileMessage = "Compile failed: Bias neurons cannot have incoming wires";
            mCompileMessageColor = sf::Color(255, 140, 140);
            mStatusText.setString("Status: compile failed");
            return false;
        }
    }

    for (std::size_t outputIndex : outputs)
    {
        if (indegree[outputIndex] == 0)
        {
            mCompileMessage = "Compile failed: each Output neuron needs incoming wire";
            mCompileMessageColor = sf::Color(255, 140, 140);
            mStatusText.setString("Status: compile failed");
            return false;
        }
    }

    std::vector<int> indegreeWork = indegree;
    std::queue<std::size_t> zeroInDegree;
    for (std::size_t i = 0; i < neuronCount; ++i)
    {
        if (indegreeWork[i] == 0)
            zeroInDegree.push(i);
    }

    std::vector<std::size_t> topologicalOrder;
    topologicalOrder.reserve(neuronCount);
    while (!zeroInDegree.empty())
    {
        const std::size_t index = zeroInDegree.front();
        zeroInDegree.pop();
        topologicalOrder.push_back(index);

        for (std::size_t target : outgoing[index])
        {
            --indegreeWork[target];
            if (indegreeWork[target] == 0)
                zeroInDegree.push(target);
        }
    }

    if (topologicalOrder.size() != neuronCount)
    {
        mCompileMessage = "Compile failed: cycle detected (feed-forward only)";
        mCompileMessageColor = sf::Color(255, 140, 140);
        mStatusText.setString("Status: compile failed");
        return false;
    }

    std::vector<bool> reachable(neuronCount, false);
    std::queue<std::size_t> bfs;
    for (std::size_t index : inputs)
    {
        if (reachable[index])
            continue;
        reachable[index] = true;
        bfs.push(index);
    }
    for (std::size_t index : biases)
    {
        if (reachable[index])
            continue;
        reachable[index] = true;
        bfs.push(index);
    }

    while (!bfs.empty())
    {
        const std::size_t current = bfs.front();
        bfs.pop();
        for (std::size_t target : outgoing[current])
        {
            if (reachable[target])
                continue;
            reachable[target] = true;
            bfs.push(target);
        }
    }

    for (std::size_t outputIndex : outputs)
    {
        if (!reachable[outputIndex])
        {
            mCompileMessage = "Compile failed: an Output neuron is not reachable from Input/Bias";
            mCompileMessageColor = sf::Color(255, 140, 140);
            mStatusText.setString("Status: compile failed");
            return false;
        }
    }

    CompiledLayout layout;
    layout.topologicalOrder = std::move(topologicalOrder);
    layout.inputNeurons = std::move(inputs);
    layout.outputNeurons = std::move(outputs);
    layout.biasNeurons = std::move(biases);
    mCompiledLayout = std::move(layout);

    if (mContext.neuralNetwork == nullptr)
    {
        mCompileMessage = "Compile failed: shared neural network instance is missing";
        mCompileMessageColor = sf::Color(255, 140, 140);
        mStatusText.setString("Status: compile failed");
        mCompiledLayout.reset();
        return false;
    }

    NeuralNetwork::BuildSpec nnSpec;
    nnSpec.nodeCount = neuronCount;
    nnSpec.topologicalOrder = mCompiledLayout->topologicalOrder;
    nnSpec.inputOrder = mCompiledLayout->inputNeurons;
    nnSpec.outputOrder = mCompiledLayout->outputNeurons;
    nnSpec.biasOrder = mCompiledLayout->biasNeurons;
    nnSpec.roles.resize(neuronCount, NeuralNetwork::NodeRole::Hidden);
    for (std::size_t i = 0; i < neuronCount; ++i)
    {
        switch (mNeurons[i].role)
        {
        case Neuron::Role::Input:
            nnSpec.roles[i] = NeuralNetwork::NodeRole::Input;
            break;
        case Neuron::Role::Output:
            nnSpec.roles[i] = NeuralNetwork::NodeRole::Output;
            break;
        case Neuron::Role::Bias:
            nnSpec.roles[i] = NeuralNetwork::NodeRole::Bias;
            break;
        case Neuron::Role::Hidden:
        default:
            nnSpec.roles[i] = NeuralNetwork::NodeRole::Hidden;
            break;
        }
    }
    nnSpec.connections.reserve(mWires.size());
    for (const auto& wire : mWires)
    {
        NeuralNetwork::Connection connection;
        connection.from = wire.from;
        connection.to = wire.to;
        connection.weight = wire.weight;
        nnSpec.connections.push_back(connection);
    }

    std::string nnError;
    if (!mContext.neuralNetwork->build(nnSpec, &nnError))
    {
        mCompileMessage = "Compile failed: " + nnError;
        mCompileMessageColor = sf::Color(255, 140, 140);
        mStatusText.setString("Status: compile failed");
        mCompiledLayout.reset();
        return false;
    }

    mCompileMessage = "Compile OK: "
        + std::to_string(neuronCount) + " neurons, "
        + std::to_string(mWires.size()) + " wires, "
        + std::to_string(mCompiledLayout->inputNeurons.size()) + " input (required "
        + std::to_string(requiredInputCount) + "), "
        + std::to_string(mCompiledLayout->biasNeurons.size()) + " bias, "
        + std::to_string(mCompiledLayout->outputNeurons.size()) + " output (required "
        + std::to_string(requiredOutputCount) + ")";
    mStatusText.setString("Status: compiled");
    return true;
}

void NNConfigState::addNeuron(sf::Vector2f point)
{
    Neuron neuron;
    neuron.shape = sf::CircleShape(kNeuronRadius);
    neuron.shape.setOrigin({kNeuronRadius, kNeuronRadius});
    neuron.shape.setPosition(point);
    neuron.shape.setFillColor(sf::Color(35, 35, 45));
    neuron.shape.setOutlineThickness(2.f);
    neuron.shape.setOutlineColor(sf::Color(120, 180, 255));
    mNeurons.push_back(neuron);
    invalidateCompiled("Compile: layout changed");
}

void NNConfigState::addWire(std::size_t from, std::size_t to)
{
    if (from == to || hasWire(from, to))
        return;

    static std::mt19937 rng(42u);
    static std::uniform_real_distribution<float> dist(-1.f, 1.f);

    Wire wire;
    wire.from = from;
    wire.to = to;
    wire.weight = dist(rng);
    mWires.push_back(wire);
    invalidateCompiled("Compile: layout changed");
}

bool NNConfigState::hasWire(std::size_t from, std::size_t to) const
{
    return std::any_of(mWires.begin(), mWires.end(),
                       [from, to](const Wire& wire) {
                           return wire.from == from && wire.to == to;
                       });
}

void NNConfigState::removeSelectedNeuron()
{
    if (!mSelectedNeuron.has_value())
        return;

    const std::size_t removeIndex = mSelectedNeuron.value();
    mSelectedNeuron.reset();

    mWires.erase(
        std::remove_if(mWires.begin(), mWires.end(),
                       [removeIndex](const Wire& wire) {
                           return wire.from == removeIndex || wire.to == removeIndex;
                       }),
        mWires.end()
    );

    for (auto& wire : mWires)
    {
        if (wire.from > removeIndex)
            --wire.from;
        if (wire.to > removeIndex)
            --wire.to;
    }

    mNeurons.erase(mNeurons.begin() + static_cast<std::ptrdiff_t>(removeIndex));
    clearLayerSelections();
    invalidateCompiled("Compile: layout changed");
}

void NNConfigState::updateHudText()
{
    std::size_t requiredInputCount = 0;
    std::size_t requiredOutputCount = 0;
    requiredNeuronCounts(requiredInputCount, requiredOutputCount);
    const std::string required = (requiredInputCount == 0 || requiredOutputCount == 0)
        ? "Required: create creature first"
        : ("Required: " + std::to_string(requiredInputCount)
           + " input, " + std::to_string(requiredOutputCount) + " output");
    mCompileStatusText.setFillColor(sf::Color::White);
    mCompileStatusText.setString(required);
    mCompileStatusText.setPosition({26.f, 72.f});
}

void NNConfigState::saveLayoutToShared() const
{
    if (mContext.neuralNetwork == nullptr)
        return;

    NeuralNetwork::EditorLayout layout;
    layout.neurons.reserve(mNeurons.size());
    for (const auto& neuron : mNeurons)
    {
        NeuralNetwork::EditorNeuron out;
        const sf::Vector2f pos = neuron.shape.getPosition();
        out.x = pos.x;
        out.y = pos.y;
        out.role = toNetworkRole(neuron.role);
        layout.neurons.push_back(out);
    }

    layout.wires.reserve(mWires.size());
    for (const auto& wire : mWires)
    {
        NeuralNetwork::EditorWire out;
        out.from = wire.from;
        out.to = wire.to;
        out.weight = wire.weight;
        layout.wires.push_back(out);
    }

    mContext.neuralNetwork->setEditorLayout(std::move(layout));
}

void NNConfigState::loadLayoutFromShared()
{
    if (mContext.neuralNetwork == nullptr)
        return;

    const auto& layoutOpt = mContext.neuralNetwork->editorLayout();
    if (!layoutOpt.has_value())
        return;

    const auto& layout = layoutOpt.value();

    mNeurons.clear();
    mWires.clear();
    mSelectedNeuron.reset();
    mDragFromNeuron.reset();
    clearLayerSelections();
    mCompiledLayout.reset();

    mNeurons.reserve(layout.neurons.size());
    for (const auto& source : layout.neurons)
    {
        Neuron neuron;
        neuron.shape = sf::CircleShape(kNeuronRadius);
        neuron.shape.setOrigin({kNeuronRadius, kNeuronRadius});
        neuron.shape.setPosition({source.x, source.y});
        neuron.shape.setFillColor(roleFillColor(fromNetworkRole(source.role)));
        neuron.shape.setOutlineThickness(2.f);
        neuron.shape.setOutlineColor(sf::Color(120, 180, 255));
        neuron.role = fromNetworkRole(source.role);
        mNeurons.push_back(neuron);
    }

    mWires.reserve(layout.wires.size());
    for (const auto& source : layout.wires)
    {
        if (source.from >= mNeurons.size() || source.to >= mNeurons.size() || source.from == source.to)
            continue;

        Wire wire;
        wire.from = source.from;
        wire.to = source.to;
        wire.weight = source.weight;
        mWires.push_back(wire);
    }

    mCompileMessage = "Loaded saved layout";
    mCompileMessageColor = sf::Color(180, 220, 255);
    mStatusText.setString("Status: not compiled");
}

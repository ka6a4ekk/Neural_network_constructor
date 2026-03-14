#pragma once

#include "Button.hpp"
#include "NeuralNetwork.hpp"
#include "State.hpp"
#include <SFML/Graphics.hpp>
#include <optional>
#include <string>
#include <vector>

class NNConfigState : public State
{
public:
    NNConfigState(StateStack& stack, Context context);

    void handleEvent(const sf::Event& event) override;
    void update(sf::Time dt) override;
    void render() override;

private:
    struct Neuron
    {
        enum class Role
        {
            Hidden,
            Input,
            Output,
            Bias
        };

        sf::CircleShape shape;
        Role role = Role::Hidden;
    };

    struct Wire
    {
        std::size_t from = 0;
        std::size_t to = 0;
        float weight = 0.f;
    };

    struct LayerSelection
    {
        std::vector<std::size_t> neuronIndices;
        sf::FloatRect bounds;
    };

    struct CompiledLayout
    {
        std::vector<std::size_t> topologicalOrder;
        std::vector<std::size_t> inputNeurons;
        std::vector<std::size_t> outputNeurons;
        std::vector<std::size_t> biasNeurons;
    };

private:
    static const char* roleName(Neuron::Role role);
    static const char* roleShortName(Neuron::Role role);
    static sf::Color roleFillColor(Neuron::Role role);
    static NeuralNetwork::NodeRole toNetworkRole(Neuron::Role role);
    static Neuron::Role fromNetworkRole(NeuralNetwork::NodeRole role);

    std::optional<std::size_t> findNeuronAt(sf::Vector2f point) const;
    static sf::FloatRect makeSelectionRect(sf::Vector2f a, sf::Vector2f b);
    std::vector<std::size_t> findNeuronsInRect(const sf::FloatRect& rect) const;
    sf::FloatRect computeLayerBounds(const std::vector<std::size_t>& neuronIndices) const;
    void addLayerSelection(const sf::FloatRect& dragRect);
    void autoWireSelectedLayers();
    void clearLayerSelections();
    void assignRoleToSelected(Neuron::Role role);
    void invalidateCompiled(std::string reason);
    void requiredNeuronCounts(std::size_t& inputCount, std::size_t& outputCount) const;
    bool compileLayout();
    void addNeuron(sf::Vector2f point);
    void addWire(std::size_t from, std::size_t to);
    bool hasWire(std::size_t from, std::size_t to) const;
    void removeSelectedNeuron();
    void updateHudText();
    void saveLayoutToShared() const;
    void loadLayoutFromShared();

private:
    static constexpr float kNeuronRadius = 22.f;

    std::vector<Neuron> mNeurons;
    std::vector<Wire> mWires;
    std::optional<std::size_t> mSelectedNeuron;
    std::optional<std::size_t> mDragFromNeuron;
    sf::Vector2f mDragMousePos{0.f, 0.f};
    bool mLayerSelecting = false;
    sf::Vector2f mLayerSelectStart{0.f, 0.f};
    sf::Vector2f mLayerSelectCurrent{0.f, 0.f};
    std::vector<LayerSelection> mLayerSelections;
    std::optional<CompiledLayout> mCompiledLayout;
    std::string mCompileMessage = "Compile: not built";
    sf::Color mCompileMessageColor = sf::Color(220, 220, 220);

    sf::Font mFont;
    sf::Text mTitleText;
    sf::Text mCompileStatusText;
    sf::Text mStatusText;
    std::optional<Button> mCompileButton;
};

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

class NeuralNetwork
{
public:
    enum class NodeRole
    {
        Hidden,
        Input,
        Output,
        Bias
    };

    struct Connection
    {
        std::size_t from = 0;
        std::size_t to = 0;
        float weight = 0.f;
    };

    struct BuildSpec
    {
        std::size_t nodeCount = 0;
        std::vector<NodeRole> roles;
        std::vector<Connection> connections;
        std::vector<std::size_t> topologicalOrder;
        std::vector<std::size_t> inputOrder;
        std::vector<std::size_t> outputOrder;
        std::vector<std::size_t> biasOrder;
    };

    struct EditorNeuron
    {
        float x = 0.f;
        float y = 0.f;
        NodeRole role = NodeRole::Hidden;
    };

    struct EditorWire
    {
        std::size_t from = 0;
        std::size_t to = 0;
        float weight = 0.f;
    };

    struct EditorLayout
    {
        std::vector<EditorNeuron> neurons;
        std::vector<EditorWire> wires;
    };

public:
    bool build(const BuildSpec& spec, std::string* errorMessage = nullptr);
    void clear();

    bool isCompiled() const;
    const BuildSpec& buildSpec() const;
    std::size_t inputCount() const;
    std::size_t outputCount() const;
    void setEditorLayout(EditorLayout layout);
    const std::optional<EditorLayout>& editorLayout() const;
    void clearEditorLayout();

    std::vector<float> forward(const std::vector<float>& inputs) const;

private:
    struct WeightedInput
    {
        std::size_t index = 0;
        float weight = 0.f;
    };

private:
    static float sigmoid(float x);
    static void setError(std::string* errorMessage, const char* message);

private:
    bool mCompiled = false;
    BuildSpec mSpec;
    std::vector<NodeRole> mRoles;
    std::vector<std::vector<WeightedInput>> mIncoming;
    std::vector<std::size_t> mTopologicalOrder;
    std::vector<std::size_t> mInputOrder;
    std::vector<std::size_t> mOutputOrder;
    std::vector<std::size_t> mBiasOrder;
    std::optional<EditorLayout> mEditorLayout;
};

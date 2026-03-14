#include "NeuralNetwork.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

bool NeuralNetwork::build(const BuildSpec& spec, std::string* errorMessage)
{
    clear();

    if (spec.nodeCount == 0)
    {
        setError(errorMessage, "Network has zero nodes");
        return false;
    }

    if (spec.roles.size() != spec.nodeCount)
    {
        setError(errorMessage, "Role count does not match node count");
        return false;
    }

    if (spec.topologicalOrder.size() != spec.nodeCount)
    {
        setError(errorMessage, "Topological order size mismatch");
        return false;
    }

    if (spec.inputOrder.empty())
    {
        setError(errorMessage, "At least one input neuron is required");
        return false;
    }

    if (spec.outputOrder.empty())
    {
        setError(errorMessage, "At least one output neuron is required");
        return false;
    }

    const std::size_t invalidIndex = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> orderPosition(spec.nodeCount, invalidIndex);
    for (std::size_t i = 0; i < spec.topologicalOrder.size(); ++i)
    {
        const std::size_t nodeIndex = spec.topologicalOrder[i];
        if (nodeIndex >= spec.nodeCount)
        {
            setError(errorMessage, "Topological order contains invalid node index");
            return false;
        }
        if (orderPosition[nodeIndex] != invalidIndex)
        {
            setError(errorMessage, "Topological order contains duplicate node index");
            return false;
        }
        orderPosition[nodeIndex] = i;
    }

    auto validateRoleList = [&](const std::vector<std::size_t>& list,
                                NodeRole expectedRole,
                                const char* message) -> bool {
        for (const std::size_t index : list)
        {
            if (index >= spec.nodeCount || spec.roles[index] != expectedRole)
            {
                setError(errorMessage, message);
                return false;
            }
        }
        return true;
    };

    if (!validateRoleList(spec.inputOrder, NodeRole::Input, "Input list contains invalid role/index"))
        return false;
    if (!validateRoleList(spec.outputOrder, NodeRole::Output, "Output list contains invalid role/index"))
        return false;
    if (!validateRoleList(spec.biasOrder, NodeRole::Bias, "Bias list contains invalid role/index"))
        return false;

    std::vector<std::vector<WeightedInput>> incoming(spec.nodeCount);
    for (const auto& connection : spec.connections)
    {
        if (connection.from >= spec.nodeCount || connection.to >= spec.nodeCount || connection.from == connection.to)
        {
            setError(errorMessage, "Connection endpoints are invalid");
            return false;
        }

        if (orderPosition[connection.from] >= orderPosition[connection.to])
        {
            setError(errorMessage, "Connection violates feed-forward topological order");
            return false;
        }

        incoming[connection.to].push_back(WeightedInput{connection.from, connection.weight});
    }

    mCompiled = true;
    mSpec = spec;
    mRoles = spec.roles;
    mIncoming = std::move(incoming);
    mTopologicalOrder = spec.topologicalOrder;
    mInputOrder = spec.inputOrder;
    mOutputOrder = spec.outputOrder;
    mBiasOrder = spec.biasOrder;
    return true;
}

void NeuralNetwork::clear()
{
    mCompiled = false;
    mSpec = BuildSpec{};
    mRoles.clear();
    mIncoming.clear();
    mTopologicalOrder.clear();
    mInputOrder.clear();
    mOutputOrder.clear();
    mBiasOrder.clear();
}

bool NeuralNetwork::isCompiled() const
{
    return mCompiled;
}

const NeuralNetwork::BuildSpec& NeuralNetwork::buildSpec() const
{
    return mSpec;
}

std::size_t NeuralNetwork::inputCount() const
{
    return mInputOrder.size();
}

std::size_t NeuralNetwork::outputCount() const
{
    return mOutputOrder.size();
}

void NeuralNetwork::setEditorLayout(EditorLayout layout)
{
    mEditorLayout = std::move(layout);
}

const std::optional<NeuralNetwork::EditorLayout>& NeuralNetwork::editorLayout() const
{
    return mEditorLayout;
}

void NeuralNetwork::clearEditorLayout()
{
    mEditorLayout.reset();
}

std::vector<float> NeuralNetwork::forward(const std::vector<float>& inputs) const
{
    if (!mCompiled || inputs.size() != mInputOrder.size())
        return {};

    std::vector<float> activations(mRoles.size(), 0.f);
    for (std::size_t i = 0; i < mInputOrder.size(); ++i)
        activations[mInputOrder[i]] = inputs[i];
    for (const std::size_t biasIndex : mBiasOrder)
        activations[biasIndex] = 1.f;

    for (const std::size_t nodeIndex : mTopologicalOrder)
    {
        const NodeRole role = mRoles[nodeIndex];
        if (role == NodeRole::Input || role == NodeRole::Bias)
            continue;

        float sum = 0.f;
        for (const auto& source : mIncoming[nodeIndex])
            sum += activations[source.index] * source.weight;

        activations[nodeIndex] = sigmoid(sum);
    }

    std::vector<float> outputs;
    outputs.reserve(mOutputOrder.size());
    for (const std::size_t outputIndex : mOutputOrder)
        outputs.push_back(activations[outputIndex]);
    return outputs;
}

float NeuralNetwork::sigmoid(float x)
{
    const float clamped = std::clamp(x, -40.f, 40.f);
    return 1.f / (1.f + std::exp(-clamped));
}

void NeuralNetwork::setError(std::string* errorMessage, const char* message)
{
    if (errorMessage != nullptr)
        *errorMessage = message;
}

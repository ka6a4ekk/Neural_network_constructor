#pragma once

#include "NeuralNetwork.hpp"
#include "State.hpp"
#include <SFML/Graphics.hpp>
#include <array>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <random>
#include <thread>
#include <vector>

class GamingState : public State
{
public:
    GamingState(StateStack& stack, Context context);
    ~GamingState() override;

    void handleEvent(const sf::Event& event) override;
    void update(sf::Time dt) override;
    void render() override;

private:
    struct RagdollNode
    {
        sf::Vector3f position{0.f, 0.f, 0.f};
        sf::Vector3f previousPosition{0.f, 0.f, 0.f};
        float mass = 1.f;
        float inverseMass = 1.f;
    };

    struct RagdollConstraint
    {
        std::size_t a = 0;
        std::size_t b = 0;
        float restLength = 0.f;
        float baseRestLength = 0.f;
        float stiffness = 1.f;
        std::size_t bendJoint = static_cast<std::size_t>(-1);
        int bendDriveA = -1;
        int bendDriveB = -1;
    };

    struct LimbActuator
    {
        std::size_t a = 0;
        std::size_t b = 0;
    };

    struct Agent
    {
        NeuralNetwork network;
        std::vector<RagdollNode> nodes;
        std::vector<RagdollConstraint> constraints;
        std::vector<LimbActuator> actuators;
        std::vector<float> limbPaddlePhase;
        std::vector<float> limbDriveState;
        sf::Vector3f previousCenterOfMass{0.f, 0.f, 0.f};
        bool hasPreviousCenterOfMass = false;
        float distanceToGoal = 1e9f;
        bool reachedGoal = false;
    };

    void setupProjection();
    void drawCubeWireframe();
    void drawTopCellOverlay(int cellX, int cellZ, const sf::Color& color, float lineWidth) const;
    void drawCreature();
    void drawSphere(float cx, float cy, float cz, float radius, int stacks, int slices) const;
    sf::Vector2f computeSpawnAnchor() const;
    sf::Vector2f computeDestinationAnchor() const;
    sf::Vector3f computeCenterOfMass() const;
    sf::Vector3f computeCenterOfMass(const std::vector<RagdollNode>& nodes) const;
    bool initializeAgentPhysics(std::vector<RagdollNode>& nodes,
                                std::vector<RagdollConstraint>& constraints,
                                std::vector<LimbActuator>& actuators,
                                std::vector<float>& limbPaddlePhase,
                                std::vector<float>& limbDriveState) const;
    NeuralNetwork::BuildSpec mutatedGenome(const NeuralNetwork::BuildSpec& base);
    bool initializePopulationFromGenome(const NeuralNetwork::BuildSpec& genome, bool includeUnmutatedMother);
    void startPopulationTraining();
    void stepPopulation(sf::Time dt);
    void advancePopulationStep(float dtSeconds);
    void runPopulationWorkers(float dtSeconds,
                              const sf::Vector2f& destination,
                              std::size_t requiredInputCount,
                              std::size_t requiredOutputCount,
                              float& bestDistance,
                              std::size_t& bestIndex,
                              std::optional<std::size_t>& firstWinner);
    void agentWorkerLoop(std::size_t workerIndex);
    void simulateRagdollForAgent(sf::Time dt,
                                 Agent& agent,
                                 const sf::Vector2f& destination,
                                 std::size_t requiredInputCount,
                                 std::size_t requiredOutputCount);
    void applyNeuralMotorControlForAgent(sf::Time dt,
                                         Agent& agent,
                                         std::vector<sf::Vector3f>& motorAcceleration,
                                         const sf::Vector2f& destination,
                                         std::size_t requiredInputCount,
                                         std::size_t requiredOutputCount);
    void startAgentWorkers();
    void stopAgentWorkers();
    void startRagdollDrop();
    void simulateRagdoll(sf::Time dt, const NeuralNetwork* network);
    void applyNeuralMotorControl(sf::Time dt,
                                 std::vector<sf::Vector3f>& motorAcceleration,
                                 const NeuralNetwork* network);
    void applyGroundAndBounds(RagdollNode& node) const;
    static float vectorLength(sf::Vector3f v);
    float sampleTerrainHeight(float x, float z) const;

private:
    bool mGlInitialized = false;
    bool mThirdPersonCamera = false;
    bool mHudVisible = true;
    float mRotationDegrees = 0.f;
    float mThirdPersonDistance = 2.5f;
    bool mRagdollActive = false;
    std::vector<RagdollNode> mRagdollNodes;
    std::vector<RagdollConstraint> mRagdollConstraints;
    std::vector<LimbActuator> mLimbActuators;
    std::vector<float> mLimbPaddlePhase;
    std::vector<float> mLimbDriveState;
    sf::Vector3f mPreviousCenterOfMass{0.f, 0.f, 0.f};
    bool mHasPreviousCenterOfMass = false;

    static constexpr std::size_t kPopulationSize = 50;
    static constexpr float kGenerationDuration = 20.f;
    static constexpr float kMutationRate = 0.05f;
    static constexpr float kMutationSigma = 0.35f;
    static constexpr float kPopulationStepSeconds = 1.f / 120.f;
    static constexpr float kTrainingSpeedMultiplier = 96.f;
    static constexpr std::size_t kMaxPopulationStepsPerUpdate = 4000;
    static constexpr std::size_t kAgentWorkerCount = 5;
    static constexpr std::size_t kAgentsPerWorker = 10;
    std::vector<Agent> mPopulation;
    bool mPopulationRunning = false;
    bool mSpeedhackEnabled = true;
    std::size_t mGenerationIndex = 0;
    float mGenerationTimeLeft = 0.f;
    float mPopulationAccumulator = 0.f;
    std::size_t mLastPopulationSubsteps = 0;
    std::size_t mRenderAgentIndex = 0;
    std::optional<std::size_t> mGenerationWinnerIndex;
    std::mt19937 mPopulationRng{std::random_device{}()};
    std::optional<NeuralNetwork::BuildSpec> mMotherGenome;
    sf::Vector2f mWorkerDestination{0.f, 0.f};
    std::size_t mWorkerRequiredInputCount = 0;
    std::size_t mWorkerRequiredOutputCount = 0;
    float mWorkerDtSeconds = 0.f;
    std::size_t mWorkerEpoch = 0;
    std::size_t mWorkerDoneCount = 0;
    bool mWorkerStop = false;
    std::array<float, kAgentWorkerCount> mWorkerBestDistance{};
    std::array<std::size_t, kAgentWorkerCount> mWorkerBestIndex{};
    std::array<std::optional<std::size_t>, kAgentWorkerCount> mWorkerWinner{};
    std::array<std::thread, kAgentWorkerCount> mWorkers;
    std::mutex mWorkerMutex;
    std::condition_variable mWorkerCv;
    std::condition_variable mWorkerDoneCv;

    float mThirdPersonYawDegrees = 0.f;
    float mThirdPersonPitchDegrees = 18.f;
    bool mMouseLookActive = false;

    sf::Font mFont;
    sf::Text mText;
    sf::RectangleShape mHudPanel;
};

cmake_minimum_required(VERSION 3.28)
project(NNC LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

add_executable(main
    src/main.cpp
    src/Engine.cpp
    src/Terrain.cpp
    src/Creature.cpp
    src/NeuralNetwork.cpp
    src/State.cpp
    src/StateStack.cpp
    src/Widget.cpp
    src/Label.cpp
    src/Button.cpp
    src/Slider.cpp
    src/MenuState.cpp
    src/TerrainEditorState.cpp
    src/CreatureEditorState.cpp
    src/GamingState.cpp
    src/NNConfigState.cpp
)

target_compile_features(main PRIVATE cxx_std_17)
target_link_libraries(main PRIVATE SFML::Graphics)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT main)

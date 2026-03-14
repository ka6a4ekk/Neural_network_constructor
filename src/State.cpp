#include "State.hpp"
#include "StateStack.hpp"

State::State(StateStack& stack, Context context)
    : mStack(stack)
    , mContext(context)
{
}

StateStack& State::getStateStack()
{
    return mStack;
}

void State::requestPush(std::unique_ptr<State> state)
{
    mStack.push(std::move(state));
}

void State::requestPop()
{
    mStack.pop();
}

void State::requestClear()
{
    mStack.clear();
}

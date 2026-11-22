#ifndef MODEL_HPP
#define MODEL_HPP

#include "SDK/Interfaces/ICustomMessageHandler.hpp"
#include "SDK/Interfaces/IGuiLifeCycleCallback.hpp"
#include "SDK/Kernel/Kernel.hpp"

class ModelListener;

class Model : public SDK::Interface::IGuiLifeCycleCallback,
              public SDK::Interface::ICustomMessageHandler
{
public:
    Model();

    void bind(ModelListener* listener) { mListener = listener; }

private:
    ModelListener*     mListener = nullptr;
    const SDK::Kernel& mKernel;

    void onStart() override {}
    // The port already invalidates the screen on resume; this asks the service to re-send.
    void onResume() override;
    void onSuspend() override {}
    void onStop() override {}
    bool customMessageHandler(SDK::MessageBase* msg) override;
};

#endif // MODEL_HPP

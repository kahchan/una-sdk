#include "gui/model/Model.hpp"
#include "gui/model/ModelListener.hpp"

#include "Commands.hpp"

#include "SDK/Kernel/KernelProviderGUI.hpp"
#include "SDK/Messages/MessageGuard.hpp"
#include "SDK/Port/LVGL/LvglPort.hpp"

Model::Model()
    : mKernel(SDK::KernelProviderGUI::GetInstance().getKernel())
{
    SDK::LVGL::Port::GetInstance().setAppLifeCycleCallback(this);
    SDK::LVGL::Port::GetInstance().setCustomMessageHandler(this);
}

void Model::onResume()
{
    SDK::send_msg<CustomMessage::Refresh>(mKernel);
}

bool Model::customMessageHandler(SDK::MessageBase* msg)
{
    if (!mListener) {
        return true;
    }
    switch (msg->getType()) {
        case CustomMessage::TIME: {
            auto* m = static_cast<CustomMessage::Time*>(msg);
            mListener->onTime(m->hour, m->minute, m->mday);
        } break;
        case CustomMessage::VALUES:
            mListener->onValues(static_cast<CustomMessage::Values*>(msg)->value);
            break;
        case CustomMessage::FACE:
            mListener->onFace(static_cast<CustomMessage::Face*>(msg)->index);
            break;
        default:
            break;
    }
    return true;
}

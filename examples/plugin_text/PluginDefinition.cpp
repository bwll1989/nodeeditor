#include "PluginDefinition.hpp"

#include "TextModel.hpp"

Plugin *Plugin::_this_plugin = nullptr;

Plugin::Plugin()
{
    _this_plugin = this;
}

Plugin::~Plugin()
{
    // TODO: Unregister all models here
}

void Plugin::registerDataModels(std::shared_ptr<QtNodes::NodeDelegateModelRegistry> &reg)
{
    assert(reg);
    //实例化注册对象
    reg->registerModelInstance<TextModel>(tag());
    //注册对象不实例化
    // reg->registerModel<TextModel>(name(),tag());
}

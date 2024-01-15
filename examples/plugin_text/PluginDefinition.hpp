#pragma once

#include <QObject>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/PluginInterface>

// This needs to be the same as the name of your project file ${PROJECT_NAME}
#ifdef plugin_text_EXPORTS
#define DLL_EXPORT Q_DECL_EXPORT
#else
#define DLL_EXPORT Q_DECL_IMPORT
#endif

#define PLUGIN_NAME "Text"
#define PLUGIN_VERSION "1.0"
#define PLUGIN_DESCRIBE "文本输入"
#define PLUGIN_TAG "Text"
class DLL_EXPORT Plugin
    : public QObject
    , public QtNodes::PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(QtNodes::PluginInterface)
    Q_PLUGIN_METADATA(IID PLUGIN_NAME)

public:
    Plugin();
    ~Plugin();

    QString name() const override { return PLUGIN_NAME; };
    QString version() const override {return PLUGIN_VERSION;};
    QString describe() const override {return PLUGIN_DESCRIBE;};
    void registerDataModels(std::shared_ptr<QtNodes::NodeDelegateModelRegistry> &reg) override;

private:
    static Plugin *_this_plugin;
};
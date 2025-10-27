#pragma once

#include <QTabWidget>
#include <memory>
#include <vector>

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/NodeDelegateModelRegistry>
#include "ModelDataBridge.hpp"
class DataflowViewsManger : public QTabWidget
{
public:
    explicit DataflowViewsManger(QWidget* parent = nullptr);

    // 设置默认的模型注册器，后续不显式传参时使用
    void setDefaultRegistry(std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry);

    // 使用默认注册器创建并插入一个新标签页，返回创建的 Scene 指针
    QtNodes::DataFlowGraphicsScene* addNewScene(const QString& title = QString());

    // 使用指定注册器创建并插入一个新标签页，返回创建的 Scene 指针
    QtNodes::DataFlowGraphicsScene* addNewScene(std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry,
                                                const QString& title = QString());

    // 便捷访问当前活动标签页的 Scene/View
    QtNodes::DataFlowGraphicsScene* currentScene() const;

    QtNodes::GraphicsView* currentView() const;

    QJsonObject save() const;

    void load(QJsonObject const &nodeJson);

private:
    // 持久化每个标签页的 DataFlowGraphModel，保证生命周期长于 Scene/View
    std::vector<std::unique_ptr<QtNodes::DataFlowGraphModel>> _models;
    // 默认注册器（可选）
    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> _defaultRegistry;
};
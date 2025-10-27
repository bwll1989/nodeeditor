// 文件：ViewsTabWidget.cpp（类 ViewsTabWidget 的成员实现）
// 依赖：QtNodes 的 DataFlowGraphicsScene / GraphicsView；QJsonObject / QJsonArray

#include "ViewsTabWidget.hpp"
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QJsonObject>
#include <QJsonArray>

using namespace QtNodes;

DataflowViewsManger::DataflowViewsManger(QWidget* parent)
    : QTabWidget(parent)
{

}

void DataflowViewsManger::setDefaultRegistry(std::shared_ptr<NodeDelegateModelRegistry> registry)
{
    _defaultRegistry = std::move(registry);
}

DataFlowGraphicsScene* DataflowViewsManger::addNewScene(const QString& title)
{
    // 如果未设置默认注册器，返回 nullptr
    if (!_defaultRegistry)
        return nullptr;

    return addNewScene(_defaultRegistry, title);
}

DataFlowGraphicsScene* DataflowViewsManger::addNewScene(std::shared_ptr<NodeDelegateModelRegistry> registry,
                                                   const QString& title)
{
    // 创建并持久化模型
    _models.emplace_back(std::make_unique<DataFlowGraphModel>(std::move(registry)));
    auto& model = *_models.back();

    // 以 ViewsTabWidget 为父对象，方便统一管理与销毁
    auto scene = new DataFlowGraphicsScene(model, this);
    auto view  = new GraphicsView(scene);

    // 插入标签页
    const QString tabTitle = title.isEmpty() ? QString("view%1").arg(count()) : title;
    insertTab(count(), view, tabTitle);

    // 便捷行为：加载后居中显示
    QObject::connect(scene, &DataFlowGraphicsScene::sceneLoaded, view, &GraphicsView::centerScene);

    return scene;
}

DataFlowGraphicsScene* DataflowViewsManger::currentScene() const
{
    auto* v = currentView();
    return v ? dynamic_cast<DataFlowGraphicsScene*>(v->scene()) : nullptr;
}

GraphicsView* DataflowViewsManger::currentView() const
{
    return qobject_cast<GraphicsView*>(currentWidget());
}

// 函数：ViewsTabWidget::load
// 作用：从传入的 JSON（包含所有标签页的序列化信息）恢复 ViewsTabWidget 的状态：重建每个标签页并载入对应的图模型数据。
// 参数：nodeJson - 根 JSON 对象，结构应包含 "tabs" (QJsonArray) 和可选 "currentIndex" (int)
// 注意：依赖 _defaultRegistry 创建新页；若未设置默认注册器则跳过创建。
void DataflowViewsManger::load(QJsonObject const &nodeJson) {
    // 1) 解析 tabs 数组
    const QJsonArray tabsArray = nodeJson.value(QStringLiteral("tabs")).toArray();
    if (tabsArray.isEmpty()) {
        // 无有效数据，直接返回
        return;
    }

    // 2) 清空现有标签页与模型（避免内存泄漏）
    for (int i = count() - 1; i >= 0; --i) {
        auto* view = qobject_cast<GraphicsView*>(widget(i));
        // 先移除标签页
        removeTab(i);
        // 删除当前页的 view
        if (view) {
            // 同时删除其关联的 scene（其父对象为 this）
            if (auto* sc = qobject_cast<DataFlowGraphicsScene*>(view->scene())) {
                sc->clearScene();
                delete sc;
            }
            delete view;
        }
    }
    _models.clear();

    // 3) 按序创建标签页并载入其模型 JSON
    for (const auto& tabVal : tabsArray) {
        const QJsonObject tabObj = tabVal.toObject();
        const QString title = tabObj.value(QStringLiteral("title")).toString(QString("view%1").arg(count()));
        const QJsonObject sceneJson = tabObj.value(QStringLiteral("scene")).toObject();

        // 若未设置默认注册器，则无法创建
        DataFlowGraphicsScene* scene = addNewScene(_defaultRegistry, title);
        if (!scene) {
            continue;
        }

        // 加载模型 JSON 到最新创建的模型（_models.back() 对应刚插入的标签页）
        try {
               _models.back()->load(sceneJson);
            } catch (const std::exception& e) {
                qDebug() << e.what();
                // 若加载失败（例如未注册模型类型），避免崩溃。可按需记录日志或提示。
                // 此处保持场景为空，以便用户手动处理。
            }
        
        //
        //
        // 居中视图（使加载后的内容居中显示）
        if (auto* view = qobject_cast<GraphicsView*>(widget(count() - 1))) {
            view->centerScene();
        }
    }

    // 4) 恢复当前活动标签页索引
    const int curIdx = nodeJson.value(QStringLiteral("currentIndex")).toInt(0);
    if (curIdx >= 0 && curIdx < count()) {
        setCurrentIndex(curIdx);
    }
}

QJsonObject DataflowViewsManger::save() const {
    QJsonObject root;
    QJsonArray tabsArray;

    const int tabCount = count();
    for (int i = 0; i < tabCount; ++i) {
        QJsonObject tabObj;
        tabObj.insert(QStringLiteral("title"), tabText(i));

        // _models 与标签页按插入顺序对齐（当前示例不支持移除标签页，因此索引对应）
        if (i >= 0 && i < static_cast<int>(_models.size()) && _models[i]) {
            tabObj.insert(QStringLiteral("scene"), _models[i]->save());
        } else {
            // 若模型缺失，插入空对象以占位
            tabObj.insert(QStringLiteral("scene"), QJsonObject{});
        }

        tabsArray.append(tabObj);
    }

    root.insert(QStringLiteral("tabs"), tabsArray);
    root.insert(QStringLiteral("currentIndex"), currentIndex());

    return root;
}


#include "ViewsTabWidget.hpp"

#include "ContainerDataModel.hpp"

#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>

#include <QJsonArray>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QDebug>

using namespace QtNodes;

DataflowViewsManger::DataflowViewsManger(QWidget *parent)
    : QWidget(parent)
{
    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(0);

    auto *bar = new QWidget(this);
    auto *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(6, 4, 6, 4);
    barLayout->setSpacing(8);

    _backButton = new QPushButton(QStringLiteral("← 返回"), bar);
    _backButton->setEnabled(false);
    _backButton->setFlat(true);
    connect(_backButton, &QPushButton::clicked, this, &DataflowViewsManger::goBack);

    _pathLabel = new QLabel(QStringLiteral("dataflow"), bar);
    _pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    barLayout->addWidget(_backButton);
    barLayout->addWidget(_pathLabel, 1);

    _view = new GraphicsView(this);
    connect(_view, &GraphicsView::scaleChanged, this, [this](double) { rememberCurrentViewport(); });
    connect(_view, &GraphicsView::viewportChanged, this, [this]() { rememberCurrentViewport(); });

    _layout->addWidget(bar);
    _layout->addWidget(_view, 1);
}

void DataflowViewsManger::setDefaultRegistry(std::shared_ptr<NodeDelegateModelRegistry> registry)
{
    _defaultRegistry = std::move(registry);
    ContainerDataModel::setSharedRegistry(_defaultRegistry);
}

void DataflowViewsManger::purgeScene(DataFlowGraphicsScene *scene)
{
    if (!scene)
        return;

    _sceneViewports.remove(scene);
    scene->clearScene();
    delete scene;
}

void DataflowViewsManger::runWithoutViewportRepaint(std::function<void()> fn)
{
    if (!_view || !fn)
        return;

    _view->setUpdatesEnabled(false);
    _suppressViewportSave = true;
    fn();
    _suppressViewportSave = false;
    _view->setUpdatesEnabled(true);
    _view->viewport()->update();
}

void DataflowViewsManger::rememberCurrentViewport()
{
    if (_suppressViewportSave || !_view || _view->isRestoringViewport() || !_view->scene())
        return;

    auto *currentScene = dynamic_cast<DataFlowGraphicsScene *>(_view->scene());
    if (!currentScene)
        return;

    _sceneViewports.insert(currentScene, _view->viewportState());
}

void DataflowViewsManger::restoreOrCenterViewport(DataFlowGraphicsScene *scene, bool forceCenter)
{
    if (!scene || !_view || _view->scene() != scene)
        return;

    if (!forceCenter && _sceneViewports.contains(scene)) {
        _view->setViewportState(_sceneViewports.value(scene));
        return;
    }

    _view->centerScene();
    _sceneViewports.insert(scene, _view->viewportState());
}

void DataflowViewsManger::switchToScene(DataFlowGraphicsScene *scene)
{
    if (!scene || !_view)
        return;

    rememberCurrentViewport();

    runWithoutViewportRepaint([this, scene]() {
        _view->setScene(scene);
        restoreOrCenterViewport(scene);
    });
}

void DataflowViewsManger::bindSceneLoadedViewport(DataFlowGraphicsScene *scene)
{
    connect(scene, &DataFlowGraphicsScene::sceneLoaded, this, [this, scene]() {
        if (_view->scene() != scene)
            return;

        runWithoutViewportRepaint([this, scene]() { restoreOrCenterViewport(scene, true); });
    });
}

DataFlowGraphicsScene *DataflowViewsManger::createRootScene(const QString &title)
{
    if (!_defaultRegistry)
        return nullptr;

    _view->setScene(nullptr);
    _sceneViewports.clear();

    while (!_stack.empty()) {
        Level top = _stack.back();
        _stack.pop_back();
        purgeScene(top.scene);
    }

    const auto leftover = findChildren<DataFlowGraphicsScene *>(QString(), Qt::FindDirectChildrenOnly);
    for (DataFlowGraphicsScene *s : leftover)
        purgeScene(s);

    _rootModel.reset();

    _rootModel = std::make_unique<DataFlowGraphModel>(_defaultRegistry);
    _rootModel->setModelAlias(QString());

    auto *scene = new DataFlowGraphicsScene(*_rootModel, this);
    connectSceneNavigation(scene);
    bindSceneLoadedViewport(scene);

    Level root;
    root.model = _rootModel.get();
    root.scene = scene;
    root.title = title.isEmpty() ? QStringLiteral("dataflow") : title;
    root.container = nullptr;
    root.ownsModel = true;
    pushLevel(root);

    return scene;
}

void DataflowViewsManger::connectSceneNavigation(DataFlowGraphicsScene *scene)
{
    if (!scene)
        return;

    connect(scene,
            &BasicGraphicsScene::nodeDoubleClicked,
            this,
            &DataflowViewsManger::onNodeDoubleClicked);

    auto *gm = dynamic_cast<DataFlowGraphModel *>(&scene->graphModel());
    if (!gm)
        return;

    connect(gm,
            &AbstractGraphModel::nodeCreated,
            this,
            [gm](NodeId id) {
                auto *container = gm->delegateModel<ContainerDataModel>(id);
                if (!container)
                    return;
                container->setParentAlias(gm->modelAlias());
                container->setNodeID(id);
                QTimer::singleShot(0, container, [container]() {
                    container->ensureInnerModel();
                    container->seedDefaultInterfaceNodes();
                    container->refreshInnerModelAlias();
                });
            });

    connect(gm,
            &AbstractGraphModel::nodeAboutToBeDeleted,
            this,
            [this, gm](NodeId id) {
                auto *container = gm->delegateModel<ContainerDataModel>(id);
                if (!container)
                    return;

                DataFlowGraphModel *inner = container->innerModel();

                for (size_t i = 0; i < _stack.size(); ++i) {
                    if (_stack[i].container == container) {
                        rememberCurrentViewport();
                        while (_stack.size() > i)
                            _stack.pop_back();
                        if (!_stack.empty())
                            switchToScene(_stack.back().scene);
                        updateBreadcrumb();
                        break;
                    }
                }

                if (!inner)
                    return;

                const auto scenes = findChildren<DataFlowGraphicsScene *>();
                for (DataFlowGraphicsScene *s : scenes) {
                    if (&s->graphModel() == inner)
                        purgeScene(s);
                }
            });
}

void DataflowViewsManger::pushLevel(Level level)
{
    _stack.push_back(level);
    switchToScene(level.scene);
    updateBreadcrumb();
}

void DataflowViewsManger::updateBreadcrumb()
{
    QStringList parts;
    for (auto const &lv : _stack)
        parts << lv.title;
    _pathLabel->setText(parts.join(QStringLiteral(" / ")));
    _backButton->setEnabled(canGoBack());
}

DataFlowGraphicsScene *DataflowViewsManger::currentScene() const
{
    if (_stack.empty())
        return nullptr;
    return _stack.back().scene;
}

DataFlowGraphModel *DataflowViewsManger::currentModel() const
{
    if (_stack.empty())
        return nullptr;
    return _stack.back().model;
}

void DataflowViewsManger::onNodeDoubleClicked(NodeId nodeId)
{
    auto *model = currentModel();
    auto *scene = currentScene();
    if (!model || !scene)
        return;

    auto *container = model->delegateModel<ContainerDataModel>(nodeId);
    if (!container)
        return;

    model->setNodeData(nodeId, NodeRole::WidgetEmbeddable, false);

    container->setParentAlias(model->modelAlias());
    container->setNodeID(nodeId);

    auto &inner = container->ensureInnerModel();
    container->seedDefaultInterfaceNodes();
    container->refreshInnerModelAlias();

    for (auto const &lv : _stack) {
        if (lv.container == container && lv.scene)
            return;
    }

    DataFlowGraphicsScene *innerScene = nullptr;
    const auto children = findChildren<DataFlowGraphicsScene *>();
    for (DataFlowGraphicsScene *s : children) {
        if (&s->graphModel() == &inner) {
            innerScene = s;
            break;
        }
    }

    if (!innerScene) {
        innerScene = new DataFlowGraphicsScene(inner, this);
        connectSceneNavigation(innerScene);
        bindSceneLoadedViewport(innerScene);
    }

    QString title = container->getRemarks().trimmed();
    if (title.isEmpty())
        title = QStringLiteral("Container");

    Level level;
    level.model = &inner;
    level.scene = innerScene;
    level.title = title;
    level.container = container;
    level.ownsModel = false;
    pushLevel(level);
}

void DataflowViewsManger::goBack()
{
    if (!canGoBack())
        return;

    if (_stack.back().container)
        _stack.back().container->syncInterfaceFromInner();

    rememberCurrentViewport();

    _stack.pop_back();
    switchToScene(_stack.back().scene);
    updateBreadcrumb();
}

void DataflowViewsManger::clearToNewRoot()
{
    createRootScene(QStringLiteral("dataflow"));
}

QJsonObject DataflowViewsManger::save() const
{
    QJsonObject root;
    if (_rootModel)
        root[QStringLiteral("scene")] = _rootModel->save();
    else
        root[QStringLiteral("scene")] = QJsonObject{};

    QJsonArray tabs;
    QJsonObject tab;
    tab[QStringLiteral("title")] = _stack.empty() ? QStringLiteral("dataflow") : _stack.front().title;
    tab[QStringLiteral("scene")] = root.value(QStringLiteral("scene"));
    tabs.append(tab);
    root[QStringLiteral("tabs")] = tabs;
    root[QStringLiteral("currentIndex")] = 0;
    root[QStringLiteral("format")] = QStringLiteral("container-v1");
    return root;
}

void DataflowViewsManger::load(QJsonObject const &json)
{
    if (!_defaultRegistry)
        return;

    createRootScene(QStringLiteral("dataflow"));
    if (!_rootModel)
        return;

    QJsonObject sceneJson = json.value(QStringLiteral("scene")).toObject();
    if (sceneJson.isEmpty()) {
        QJsonArray tabs = json.value(QStringLiteral("tabs")).toArray();
        if (!tabs.isEmpty())
            sceneJson = tabs.at(0).toObject().value(QStringLiteral("scene")).toObject();
    }

    if (sceneJson.isEmpty())
        return;

    {
        QJsonArray nodes = sceneJson.value(QStringLiteral("nodes")).toArray();
        bool changed = false;
        for (int i = 0; i < nodes.size(); ++i) {
            QJsonObject node = nodes.at(i).toObject();
            QString const t = node.value(QStringLiteral("type")).toString();
            if (t == QLatin1String("Entrance")) {
                node.insert(QStringLiteral("type"), QStringLiteral("In"));
                nodes.replace(i, node);
                changed = true;
            } else if (t == QLatin1String("Export")) {
                node.insert(QStringLiteral("type"), QStringLiteral("Out"));
                nodes.replace(i, node);
                changed = true;
            }
        }
        if (changed)
            sceneJson.insert(QStringLiteral("nodes"), nodes);
    }

    try {
        _rootModel->load(sceneJson);
    } catch (std::exception const &e) {
        qDebug() << "load failed:" << e.what();
    }

    if (_stack.empty())
        return;

    runWithoutViewportRepaint([this]() { restoreOrCenterViewport(_stack.back().scene, true); });
}

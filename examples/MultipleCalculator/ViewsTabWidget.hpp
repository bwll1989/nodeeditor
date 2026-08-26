#pragma once

#include <QHash>
#include <QWidget>
#include <functional>
#include <memory>
#include <vector>

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/NodeDelegateModelRegistry>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;
class ContainerDataModel;

/**
 * 单画布 + 导航栈：根 DataFlow 与 Container 内嵌 DataFlow 之间进入/返回。
 *
 * 仅有一个 GraphicsView；各层 scene 的缩放/平移保存在 _sceneViewports 中，
 * 按 scene 指针索引（Container 子 scene 在返回后仍存活，视口可复用）。
 */
class DataflowViewsManger : public QWidget
{
    Q_OBJECT
public:
    explicit DataflowViewsManger(QWidget *parent = nullptr);

    void setDefaultRegistry(std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry);

    /// 创建根画布（会清空导航栈与已存视口）
    QtNodes::DataFlowGraphicsScene *createRootScene(const QString &title = QStringLiteral("dataflow"));

    QtNodes::DataFlowGraphicsScene *currentScene() const;
    QtNodes::GraphicsView *view() const { return _view; }
    QtNodes::DataFlowGraphModel *currentModel() const;

    bool canGoBack() const { return _stack.size() > 1; }

    QJsonObject save() const;
    void load(QJsonObject const &json);

public Q_SLOTS:
    void goBack();
    void clearToNewRoot();

private:
    struct Level {
        QtNodes::DataFlowGraphModel *model = nullptr;
        QtNodes::DataFlowGraphicsScene *scene = nullptr;
        QString title;
        /// 非空表示该层由某个 Container 拥有 inner model
        ContainerDataModel *container = nullptr;
        bool ownsModel = false;
    };

    void pushLevel(Level level);
    void connectSceneNavigation(QtNodes::DataFlowGraphicsScene *scene);
    void bindSceneLoadedViewport(QtNodes::DataFlowGraphicsScene *scene);
    void onNodeDoubleClicked(QtNodes::NodeId nodeId);
    void updateBreadcrumb();

    /// 将当前 view 的视口写入 _sceneViewports（键为当前 scene）。
    void rememberCurrentViewport();

    /**
     * 恢复 scene 的已存视口；若无记录或 forceCenter 为 true，则 centerScene 并写入 map。
     * 调用方应在外层用 runWithoutViewportRepaint 包裹以避免闪烁。
     */
    void restoreOrCenterViewport(QtNodes::DataFlowGraphicsScene *scene, bool forceCenter = false);

    /// 保存当前视口 → 切换 scene → 恢复目标视口（同步、无中间帧）。
    void switchToScene(QtNodes::DataFlowGraphicsScene *scene);

    /// 切换/恢复视口期间禁止重绘与中途 save，避免闪屏和脏数据。
    void runWithoutViewportRepaint(std::function<void()> fn);

    void purgeScene(QtNodes::DataFlowGraphicsScene *scene);

    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> _defaultRegistry;
    std::unique_ptr<QtNodes::DataFlowGraphModel> _rootModel;

    QtNodes::GraphicsView *_view = nullptr;
    QPushButton *_backButton = nullptr;
    QLabel *_pathLabel = nullptr;
    QVBoxLayout *_layout = nullptr;

    std::vector<Level> _stack;

    /// 各 DataFlowGraphicsScene 独立的缩放/平移（与导航栈生命周期解耦）。
    QHash<QtNodes::DataFlowGraphicsScene *, QtNodes::GraphicsView::ViewportState> _sceneViewports;

    bool _suppressViewportSave = false;
};

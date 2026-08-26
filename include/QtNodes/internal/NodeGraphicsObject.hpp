#pragma once

#include <QtCore/QUuid>
#include <QtWidgets/QGraphicsObject>
#include "NodeState.hpp"

class QGraphicsProxyWidget;
class QLineEdit;
class QMenu;

namespace QtNodes {

class BasicGraphicsScene;
class AbstractGraphModel;

class NodeGraphicsObject : public QGraphicsObject
{
    Q_OBJECT
public:
    // Needed for qgraphicsitem_cast
    enum { Type = UserType + 1 };

    int type() const override { return Type; }

public:
    NodeGraphicsObject(BasicGraphicsScene &scene, NodeId node);

    ~NodeGraphicsObject() override;

public:
    AbstractGraphModel &graphModel() const;

    BasicGraphicsScene *nodeScene() const;

    NodeId nodeId() { return _nodeId; }

    NodeId nodeId() const { return _nodeId; }

    NodeState &nodeState() { return _nodeState; }

    NodeState const &nodeState() const { return _nodeState; }

    QRectF boundingRect() const override;

    void setGeometryChanged();

    /// Visits all attached connections and corrects
    /// their corresponding end points.
    void moveConnections() const;

    /// Repaints the node once with reacting ports.
    void reactToConnection(ConnectionGraphicsObject const *cgo);

    void updateQWidgetEmbedPos();

    void onEmbedWidgetChanged() {
        embedQWidget();
    }

    bool isEditingRemarks() const;

protected:
    void paint(QPainter *painter,
               QStyleOptionGraphicsItem const *option,
               QWidget *widget = 0) override;

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    void hoverMoveEvent(QGraphicsSceneHoverEvent *) override;

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void embedQWidget();

    /// 按当前端口高度同步嵌入控件尺寸（端口增删后需调用，否则只涨节点框不涨控件）
    void syncEmbeddedWidgetSize(QWidget *w);

    /// 开启/关闭端口编辑：开启时自动展开；关闭时恢复进入前的展开/折叠状态
    void setPortEditing(bool enabled);

    void setLockedState();

    void initRemarksEditor();
    void startEditingRemarks();
    void finishEditingRemarks();
    void syncRemarksEditorGeometry();

    void addTitleColorMenu(QMenu &menu);
    void addExternalControlMenu(QMenu &menu);
    void openNodeHelp() const;

    /**
     * 收起模式（CaptionVisible=false）下用系统 QToolTip 显示端口名。
     * 不进入场景图元，避免放大节点命中区域、干扰邻近节点连线。
     * 输入口向左偏移、输出口向右偏移（垂直布局则上下偏移）。
     */
    void showCompactPortToolTip(PortType portType, PortIndex portIndex, QPoint globalPos);
    /// 隐藏端口名 QToolTip
    void hideCompactPortToolTip();

private Q_SLOTS:
    void onLockedState(NodeId);

private:
    NodeId _nodeId;

    AbstractGraphModel &_graphModel;

    NodeState _nodeState;

    // either nullptr or owned by parent QGraphicsItem
    QGraphicsProxyWidget *_proxyWidget;

    /// 进入端口编辑前是否已记录展开状态
    bool _hasEmbeddableBeforePortEdit = false;
    /// 进入端口编辑前的 WidgetEmbeddable（展开/折叠）
    bool _embeddableBeforePortEdit = false;

    /// Scene-local remarks editor (follows zoom/pan with the node).
    QGraphicsProxyWidget *_remarksProxy = nullptr;
    QLineEdit *_remarksEditor = nullptr;
    bool _finishingRemarksEdit = false;
    bool _discardRemarksEdit = false;
};
} // namespace QtNodes

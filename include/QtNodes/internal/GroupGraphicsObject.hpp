#pragma once

#include <utility>

#include <QtCore/QUuid>
#include <QtGui/QColor>
#include <QtWidgets/QGraphicsObject>

#include "Definitions.hpp"
#include <QKeyEvent>

class QGraphicsSceneMouseEvent;
class QGraphicsProxyWidget;
class QTimer;
class QMenu;
class QPlainTextEdit;

namespace QtNodes {

class AbstractGraphModel;
class BasicGraphicsScene;

/// Graphic Object for Group. Adds itself to scene
class GroupGraphicsObject : public QGraphicsObject
{
    Q_OBJECT
public:
    // Needed for qgraphicsitem_cast
    enum { Type = UserType + 3 };

    int type() const override { return Type; }

public:
    GroupGraphicsObject(BasicGraphicsScene &scene, GroupId const groupId);

    ~GroupGraphicsObject() = default;

public:
    AbstractGraphModel &graphModel() const;

    BasicGraphicsScene *nodeScene() const;

    QString remarks() const {
        return _groupId.groupRemarks;
    }

    void setRemarks(QString remarks) {
        _groupId.groupRemarks = std::move(remarks);
    }

    /// 标题颜色（对应节点 TitleColor）
    QColor titleColor() const;

    /// 选中边框颜色（对应节点 SelectedBoundaryColor）
    QColor selectedBoundaryColor() const;

    /// 设置标题色，并同步选中边框色（与节点改色行为一致）
    void setTitleColor(QColor const &color);

    bool isCollapsed() const { return _collapsed; }

    bool isEditingRemarks() const;

    /// 标题栏高度：按备注多行文字动态计算，不小于 GroupStyle::CaptionHeight
    qreal captionHeight() const;

    /// 与节点一致：标题相对外边框的内缩（PenWidth / HoveredPenWidth）
    qreal captionInset() const;

    /// 内缩后的标题栏矩形（显示与编辑共用）
    QRectF captionBarRect() const;

    /// 标题区域总高度 = inset + captionHeight（主体从此处开始）
    qreal headerHeight() const;

    QPointF collapsedPortScenePosition(PortType portType) const;

    QRectF boundingRect() const override;

    QRectF contentRect() const { return _rect; }

    QPainterPath shape() const override;

    /// Updates the position of both ends
    void move();
    /// Returns the group id
    GroupId groupId() const {
//        qDebug() << "groupId: "<< _groupId.nodeIds.size() ;
        return _groupId; }

    void applyGroupId(GroupId const &groupId);

    /// 选中组内全部可见节点
    void selectMemberNodes();

protected:
    void paint(QPainter *painter,
               QStyleOptionGraphicsItem const *option,
               QWidget *widget = 0) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;

    bool eventFilter(QObject *obj, QEvent *event) override;

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    void updatePosition();

    void addGraphicsEffect();

    void startEditingRemarks();

    void initRemarksEditor();

    void finishEditingRemarks();

    void setCollapsed(bool collapsed, bool updateModel = true);

    void setGroupItemsVisible(bool visible);

    void updateGroupBounds();

    void onNodePositionUpdated(NodeId nodeId);

    void onNodeUpdated(NodeId nodeId);

    void onNodeDeleted(NodeId nodeId);

    void setLockedState();

    /// 与节点同名：右键菜单改标题颜色
    void addTitleColorMenu(QMenu &menu);

    qreal captionHeightForWidth(qreal width) const;

    qreal captionHeightForText(qreal width, QString const &text) const;

    /// 折叠宽度：按备注文字自然宽度 + 边距，不再沿用展开包围盒宽度
    qreal collapsedWidthForRemarks(QString const &text) const;

    qreal collapsedWidthForRemarks() const
    {
        return collapsedWidthForRemarks(_groupId.groupRemarks);
    }

    bool containsNode(NodeId nodeId) const;

    void scheduleGroupBoundsUpdate();

    qreal collapsedBodyHeight() const;

    qreal collapsedTotalHeight() const;

    void applyCollapsedGeometry(QString const &remarksText = {});

    void syncRemarksEditorGeometry();

private Q_SLOTS:
    void onLockedState(GroupId groupId);
private:
    AbstractGraphModel &_graphModel;
    GroupId _groupId;
    QRectF _rect;
    /// Scene-local remarks editor (follows zoom/pan with the group).
    QGraphicsProxyWidget *_remarksProxy = nullptr;
    QPlainTextEdit *_remarksEditor = nullptr;
    QString _remarks="Group";
    bool _finishingRemarksEdit = false;
    bool _discardRemarksEdit = false;

    bool _pressedOnCaption = false;
    bool _collapsed = false;
    /// 展开态编辑时记录上一帧标题高度，用于随草稿增减顶边
    qreal _editingCaptionHeight = 0.0;
    bool _boundsUpdatePending = false;
};

} // namespace QtNodes

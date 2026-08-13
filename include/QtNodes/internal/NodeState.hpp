#pragma once

#include <unordered_map>
#include <vector>

#include <QtCore/QPointF>
#include <QtCore/QPointer>
#include <QtCore/QUuid>

#include "Export.hpp"

#include "Definitions.hpp"
#include "NodeData.hpp"

namespace QtNodes {

class ConnectionGraphicsObject;
class NodeGraphicsObject;

/// Stores bool for hovering connections and resizing flag.
class NODE_EDITOR_PUBLIC NodeState
{
public:
    NodeState(NodeGraphicsObject &ngo);

public:
    bool hovered() const { return _hovered; }

    void setHovered(bool hovered = true) { _hovered = hovered; }

    /// 当前鼠标悬停命中的端口类型（无则 PortType::None）
    PortType hoveredPortType() const { return _hoveredPortType; }

    /// 当前鼠标悬停命中的端口索引
    PortIndex hoveredPortIndex() const { return _hoveredPortIndex; }

    /// 记录悬停端口，供绘制高亮与 QToolTip 使用
    void setHoveredPort(PortType portType, PortIndex portIndex)
    {
        _hoveredPortType = portType;
        _hoveredPortIndex = portIndex;
    }

    /// 清除悬停端口状态
    void clearHoveredPort()
    {
        _hoveredPortType = PortType::None;
        _hoveredPortIndex = InvalidPortIndex;
    }

    void setResizing(bool resizing);

    bool resizing() const;

    ConnectionGraphicsObject const *connectionForReaction() const;

    void storeConnectionForReaction(ConnectionGraphicsObject const *cgo);

    void resetConnectionForReaction();

private:
    NodeGraphicsObject &_ngo;

    bool _hovered;

    bool _resizing;

    /// 悬停端口类型；无悬停时为 PortType::None
    PortType _hoveredPortType = PortType::None;

    /// 悬停端口索引；无悬停时为 InvalidPortIndex
    PortIndex _hoveredPortIndex = InvalidPortIndex;

    // QPointer tracks the QObject inside and is automatically cleared
    // when the object is destroyed.
    QPointer<ConnectionGraphicsObject const> _connectionForReaction;
};
} // namespace QtNodes

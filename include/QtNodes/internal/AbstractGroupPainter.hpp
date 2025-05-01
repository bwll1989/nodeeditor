#pragma once

#include <QPainter>

#include "Export.hpp"

class QPainter;

namespace QtNodes {

class GroupGraphicsObject;

/// Class enables custom painting for connections.
class NODE_EDITOR_PUBLIC AbstractGroupPainter
{
public:
    virtual ~AbstractGroupPainter() = default;

    /**
     * Reimplement this function in order to have a custom connection painting.
     */
    virtual void paint(QPainter *painter, GroupGraphicsObject const &cgo) const = 0;

    
};
} // namespace QtNodes

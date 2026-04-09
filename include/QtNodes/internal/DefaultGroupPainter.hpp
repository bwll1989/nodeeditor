#pragma once

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

#include "AbstractGroupPainter.hpp"
#include "Definitions.hpp"

namespace QtNodes {

class GroupGraphicsObject;

class DefaultGroupPainter : public AbstractGroupPainter
{
public:
    void paint(QPainter *painter, GroupGraphicsObject const &cgo) const override;

    void drawGroupRect(QPainter *painter, GroupGraphicsObject const &ngo) const;

//    void drawConnectionPoints(QPainter *painter, NodeGraphicsObject &ngo) const;

//    void drawFilledConnectionPoints(QPainter *painter, NodeGraphicsObject &ngo) const;

    void drawGroupCaption(QPainter *painter, GroupGraphicsObject const &ggo) const;

    void drawGroupPorts(QPainter *painter, GroupGraphicsObject const &ggo) const;

//    void drawEntryLabels(QPainter *painter, GroupGraphicsObject const &ggo) const;

//    void drawResizeRect(QPainter *painter, NodeGraphicsObject &ngo) const;

};

} // namespace QtNodes

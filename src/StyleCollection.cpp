#include "StyleCollection.hpp"

using QtNodes::ConnectionStyle;
using QtNodes::GraphicsViewStyle;
using QtNodes::NodeStyle;
using QtNodes::StyleCollection;
using QtNodes::GroupStyle;

NodeStyle const &StyleCollection::nodeStyle()
{
    return instance()._nodeStyle;
}

ConnectionStyle const &StyleCollection::connectionStyle()
{
    return instance()._connectionStyle;
}
GroupStyle const &StyleCollection::groupStyle()
{
    return instance()._groupStyle;
}

GraphicsViewStyle const &StyleCollection::flowViewStyle()
{
    return instance()._flowViewStyle;
}

void StyleCollection::setNodeStyle(NodeStyle nodeStyle)
{
    instance()._nodeStyle = nodeStyle;
}

void StyleCollection::setConnectionStyle(ConnectionStyle connectionStyle)
{
    instance()._connectionStyle = connectionStyle;
}

void StyleCollection::setGroupStyle(GroupStyle Style)
{
    instance()._groupStyle = Style;
}

void StyleCollection::setGraphicsViewStyle(GraphicsViewStyle flowViewStyle)
{
    instance()._flowViewStyle = flowViewStyle;
}

StyleCollection &StyleCollection::instance()
{
    static StyleCollection collection;

    return collection;
}

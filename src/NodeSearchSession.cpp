#include "NodeSearchSession.hpp"

namespace QtNodes {

NodeSearchSession::NodeSearchSession(QObject *parent)
    : QObject(parent)
{
    setObjectName(QLatin1String(ObjectName));
}

NodeSearchSession *searchSessionOf(QWidget *bar)
{
    if (!bar) {
        return nullptr;
    }
    if (auto *obj = bar->findChild<QObject *>(QLatin1String(NodeSearchSession::ObjectName))) {
        return static_cast<NodeSearchSession *>(obj);
    }
    return nullptr;
}

} // namespace QtNodes

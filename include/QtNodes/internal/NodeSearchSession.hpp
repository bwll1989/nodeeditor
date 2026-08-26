#pragma once

#include "Definitions.hpp"
#include "Export.hpp"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtWidgets/QWidget>

#include <functional>

namespace QtNodes {

enum class SearchMatchKind { Node, Connection };

struct SearchEntry {
    SearchMatchKind kind = SearchMatchKind::Node;
    NodeId nodeId = InvalidNodeId;
    ConnectionId connectionId{};
    QString haystack;
};

/**
 * @brief 画布视口内嵌搜索条的会话状态。
 *
 * 无 Q_OBJECT；通过 objectName 查找后 static_cast 使用。
 */
class NODE_EDITOR_PUBLIC NodeSearchSession : public QObject
{
public:
    static constexpr char const *ObjectName = "nodeSearchSession";

    explicit NodeSearchSession(QObject *parent = nullptr);

    QVector<SearchEntry> entries;
    QVector<SearchEntry> matches;
    int matchIndex = -1;

    std::function<void()> rebuildIndex;
    std::function<void(QString const &)> refill;
    std::function<void()> reposition;
    std::function<void()> focusEdit;
};

NODE_EDITOR_PUBLIC NodeSearchSession *searchSessionOf(QWidget *bar);

} // namespace QtNodes

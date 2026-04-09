#pragma once

#include "Definitions.hpp"

#include <QJsonObject>
#include <QJsonArray>
#include <iostream>
#include <string>

namespace QtNodes {

inline QJsonObject groupToJson(GroupId const &groupId)
{
    QJsonArray nodesJsonArray;
    for (auto const &nid : groupId.nodeIds) {
        nodesJsonArray.append(static_cast<qint64>(nid));
    }
    QJsonObject groupJson;

    groupJson["nodeIds"] = nodesJsonArray;
    groupJson["remarks"] = groupId.groupRemarks;
    groupJson["collapsed"] = groupId.collapsed;
    return groupJson;
}

inline GroupId fromJsonToGroup(QJsonObject const &groupJson)
{
    GroupId groupId;
    QJsonArray nodesJsonArray = groupJson["nodeIds"].toArray();
    for (auto const &nid : nodesJsonArray) {
        groupId.nodeIds.push_back(static_cast<NodeId>(nid.toInt(InvalidNodeId)));
    }
    groupId.groupRemarks = groupJson["remarks"].toString();
    groupId.collapsed = groupJson["collapsed"].toBool(false);
    return groupId;
}

} // namespace QtNodes

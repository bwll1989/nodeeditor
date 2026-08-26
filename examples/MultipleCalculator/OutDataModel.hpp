#pragma once

#include <QtNodes/NodeDelegateModel>

#include <QtCore/QObject>
#include <unordered_map>
#include <memory>

#include "DecimalData.hpp"

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;
using QtNodes::PortIndex;
using QtNodes::PortType;

/// 子图输出接口（TD 风格 Out）：内部写入后由 Container 外壳送出。
class OutDataModel : public NodeDelegateModel
{
    Q_OBJECT
public:
    OutDataModel();
    ~OutDataModel() override = default;

    QString caption() const override { return QStringLiteral("Out"); }
    bool captionVisible() const override { return true; }
    QString type() const override { return QStringLiteral("Out"); }

    unsigned int nPorts(PortType portType) const override;
    NodeDataType dataType(PortType portType, PortIndex portIndex) const override;
    std::shared_ptr<NodeData> outData(PortIndex port) override;
    void setInData(std::shared_ptr<NodeData> data, PortIndex portIndex) override;
    QWidget *embeddedWidget() override;

private:
    std::unordered_map<PortIndex, std::shared_ptr<NodeData>> _dataMap;
};

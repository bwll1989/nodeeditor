#pragma once

#include <QtNodes/NodeDelegateModel>

#include <QtCore/QObject>

#include <iostream>
#include <QLineEdit>
#include "DecimalData.hpp"
#include "ModelDataBridge.hpp"
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;
using QtNodes::PortIndex;
using QtNodes::PortType;
/// The model dictates the number of inputs and outputs for the Node.
/// In this example it has no logic.
class ExportDataModel : public NodeDelegateModel
{
    Q_OBJECT

public:
    ExportDataModel();

    ~ExportDataModel();

public:
    QString caption() const override { return QStringLiteral("Export"); }

    bool captionVisible() const override { return true; }

    QString type() const override { return QStringLiteral("Export"); }

public:
    unsigned int nPorts(PortType portType) const override;

    NodeDataType dataType(PortType portType, PortIndex portIndex) const override;

    std::shared_ptr<NodeData> outData(PortIndex port) override;

    void setInData(std::shared_ptr<NodeData> data, PortIndex portIndex) override;

    QWidget *embeddedWidget() override;

    void setRemarks(const QString& remarks) override;
private:
    std::unordered_map<PortIndex, std::shared_ptr<NodeData>> _dataMap;
};

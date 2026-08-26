#include "OutDataModel.hpp"

#include <QtMath>

OutDataModel::OutDataModel()
{
    InPortCount = 2;
    OutPortCount = 0;
    CaptionVisible = true;
    Caption = QStringLiteral("Out");
    WidgetEmbeddable = false;
    Resizable = false;
    PortEditable = true;
    setRemarks(QStringLiteral("Out"));
}

unsigned int OutDataModel::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
        return InPortCount;
    case PortType::Out:
        return 0;
    default:
        return 0;
    }
}

NodeDataType OutDataModel::dataType(PortType, PortIndex) const
{
    return DecimalData().type();
}

std::shared_ptr<NodeData> OutDataModel::outData(PortIndex port)
{
    return _dataMap[port];
}

void OutDataModel::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    auto numberData = std::dynamic_pointer_cast<DecimalData>(data);
    auto prev = std::dynamic_pointer_cast<DecimalData>(_dataMap[portIndex]);
    if (prev && numberData && qAbs(prev->number() - numberData->number()) < 1e-12)
        return;
    if (!prev && !numberData)
        return;

    _dataMap[portIndex] = numberData;
    Q_EMIT dataUpdated(portIndex);
}

QWidget *OutDataModel::embeddedWidget()
{
    return nullptr;
}

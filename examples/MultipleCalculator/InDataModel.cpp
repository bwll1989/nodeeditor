#include "InDataModel.hpp"

#include "DecimalData.hpp"

#include <QtCore/QJsonObject>
#include <QtMath>

InDataModel::InDataModel()
{
    InPortCount = 0;
    OutPortCount = 2;
    CaptionVisible = true;
    Caption = QStringLiteral("In");
    WidgetEmbeddable = false;
    Resizable = false;
    PortEditable = true;
    setRemarks(QStringLiteral("In"));
}

QJsonObject InDataModel::save() const
{
    return NodeDelegateModel::save();
}

void InDataModel::load(QJsonObject const &p)
{
    NodeDelegateModel::load(p);
}

NodeDataType InDataModel::dataType(PortType, PortIndex) const
{
    return DecimalData().type();
}

std::shared_ptr<NodeData> InDataModel::outData(PortIndex port)
{
    return _dataMap[port];
}

QWidget *InDataModel::embeddedWidget()
{
    return nullptr;
}

void InDataModel::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
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

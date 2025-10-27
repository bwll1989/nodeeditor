#include "ExportDataModel.hpp"

#include <QtWidgets/QLabel>

ExportDataModel::ExportDataModel()
{
    setRemarks(QString("Out"));
    ModelDataBridge::instance().registerExportDelegate(this);

}

ExportDataModel::~ExportDataModel()
{
    ModelDataBridge::instance().unregisterExportDelegate(this);
}
unsigned int ExportDataModel::nPorts(PortType portType) const
{
    unsigned int result = 2;

    switch (portType) {
    case PortType::In:
        result = 2;
        break;

    case PortType::Out:
        result = 0;

    default:
        break;
    }

    return result;
}

NodeDataType ExportDataModel::dataType(PortType, PortIndex) const
{
    return DecimalData().type();
}

std::shared_ptr<NodeData> ExportDataModel::outData(PortIndex port)
{
    return  _dataMap[port];

}

void ExportDataModel::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    _dataMap[portIndex] = std::dynamic_pointer_cast<DecimalData>(data);
    emit dataUpdated(portIndex);
}

QWidget *ExportDataModel::embeddedWidget()
{
    return nullptr;
}
void ExportDataModel::setRemarks(const QString& remarks){
    NodeDelegateModel::setRemarks(remarks);
    ModelDataBridge::instance().updateRemarksForDelegate(this,false,getRemarks());
};
#include "EntranceDataModel.hpp"

#include <QVBoxLayout>

#include "DecimalData.hpp"
#include <QSpinBox>
#include <QtCore/QJsonValue>
#include <QtGui/QDoubleValidator>
#include <QtWidgets/QLineEdit>

EntranceDataModel::EntranceDataModel()
    : _inputSelector{new QComboBox()}
 {
    InPortCount =0;
    OutPortCount=2;
    CaptionVisible=false;
    Caption="Entrance";
    WidgetEmbeddable= true;
    Resizable=false;
    PortEditable= true;
    registerExternalControl("/number",_inputSelector);
    setRemarks("Out");
    ModelDataBridge::instance().registerEntranceDelegate(this);
    connect(_inputSelector,&QComboBox::currentTextChanged,this,&EntranceDataModel::setRemarks);

}

EntranceDataModel::~EntranceDataModel() {
    ModelDataBridge::instance().unregisterEntranceDelegate(this);
}
QJsonObject EntranceDataModel::save() const
{
    QJsonObject modelJson = NodeDelegateModel::save();

    modelJson["quoted address"] =_inputSelector->currentText();

    return modelJson;
}


void EntranceDataModel::setRemarks(const QString& remarks){
    NodeDelegateModel::setRemarks(remarks);
    ModelDataBridge::instance().updateRemarksForDelegate(this,true,getRemarks());
};
void EntranceDataModel::load(QJsonObject const &p)
{
    QJsonValue v = p["quoted address"];

    if (!v.isUndefined()) {
        QString strNum = v.toString();
        if (_inputSelector)
            _inputSelector->setCurrentText(strNum);
        }

}


NodeDataType EntranceDataModel::dataType(PortType, PortIndex) const
{
    return DecimalData().type();
}

std::shared_ptr<NodeData> EntranceDataModel::outData(PortIndex port)
{
    return _dataMap[port];
}

QWidget *EntranceDataModel::embeddedWidget()
{
    _inputSelector->clear();
    _inputSelector->addItems(ModelDataBridge::instance().getAllExportRemarks());
    return _inputSelector;
}

void EntranceDataModel::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    qDebug() << "setInData"<<portIndex;
   _dataMap[portIndex] = std::dynamic_pointer_cast<DecimalData>(data);
    emit dataUpdated(portIndex);
}

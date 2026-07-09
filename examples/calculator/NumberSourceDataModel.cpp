#include "NumberSourceDataModel.hpp"

#include <QVBoxLayout>

#include "DecimalData.hpp"
#include <QSpinBox>
#include <QtCore/QJsonValue>
#include <QtGui/QDoubleValidator>
#include <QtWidgets/QLineEdit>
#include <QDebug>

NumberSourceDataModel::NumberSourceDataModel()
    : _number(std::make_shared<DecimalData>(0.0)) {
    InPortCount =0;
    OutPortCount=1;
    CaptionVisible=true;
    Caption="Number";
    WidgetEmbeddable= true;
    Resizable=false;
    PortEditable= true;

    NodeDelegateModel::ExternalBinding binding;
    binding.member = "number";
    binding.feedback = true;
    registerExternalBinding("/number", this, binding);
}

QJsonObject NumberSourceDataModel::save() const
{
    QJsonObject modelJson = NodeDelegateModel::save();

    modelJson["number"] = QString::number(_number->number());

    return modelJson;
}

void NumberSourceDataModel::load(QJsonObject const &p)
{
    QJsonValue v = p["number"];

    if (!v.isUndefined()) {
        QString strNum = v.toString();

        bool ok;
        double d = strNum.toDouble(&ok);
        if (ok) {
            // _number = std::make_shared<DecimalData>(d);
            setNumber(d);
            // if (_lineEdit) {
            //     bool const old = _lineEdit->blockSignals(true);
            //     _lineEdit->setText(strNum);
            //     _lineEdit->blockSignals(old);
            // }
            this->printHello();
        }
    }
}



void NumberSourceDataModel::onTextEdited(QString const &str)
{
    bool ok = false;

    double n = str.toDouble(&ok);

    if (ok) {
        // _number = std::make_shared<DecimalData>(n);
        setNumber(n);
        // Q_EMIT numberChanged(n);
        // Q_EMIT dataUpdated(0);

    } else {
        Q_EMIT dataInvalidated(0);
    }
}

NodeDataType NumberSourceDataModel::dataType(PortType, PortIndex) const
{
    return DecimalData().type();
}

std::shared_ptr<NodeData> NumberSourceDataModel::outData(PortIndex)
{
    return _number;
}

QWidget *NumberSourceDataModel::embeddedWidget()
{

    if (!_lineEdit) {
        _lineEdit = new QLineEdit();

        _lineEdit->setValidator(new QDoubleValidator());
        _lineEdit->setMaximumSize(_lineEdit->sizeHint());

        bool const old = _lineEdit->blockSignals(true);
        _lineEdit->setText(QString::number(_number->number()));
        _lineEdit->blockSignals(old);

        NodeDelegateModel::ExternalBinding ui;
        ui.control = _lineEdit;
        registerExternalBinding("/number", nullptr, ui);
    }

    QObject::connect(_lineEdit, &QLineEdit::textChanged, this, &NumberSourceDataModel::onTextEdited, Qt::UniqueConnection);

    return _lineEdit;
}

double NumberSourceDataModel::number() const
{
    return _number ? _number->number() : 0.0;
}

void NumberSourceDataModel::setNumber(double n)
{
    if (_number && _number->number() == n)
        return;

    _number = std::make_shared<DecimalData>(n);

    Q_EMIT numberChanged(n);
    Q_EMIT dataUpdated(0);

    if (_lineEdit) {
        bool const old = _lineEdit->blockSignals(true);
        _lineEdit->setText(QString::number(_number->number()));
        _lineEdit->blockSignals(old);

    }
}

void NumberSourceDataModel::printHello()
{
    this->stateFeedBack("/number/hello", true);
            
    qDebug() << "NumberSourceDataModel::printHello";
    this->stateFeedBack("/number/hello", false);
}

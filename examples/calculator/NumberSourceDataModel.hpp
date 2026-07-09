#pragma once

#include <QtNodes/NodeDelegateModel>

#include <QtCore/QObject>

#include <iostream>

class DecimalData;

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;
using QtNodes::PortIndex;
using QtNodes::PortType;

class QLineEdit;

/// The model dictates the number of inputs and outputs for the Node.
/// In this example it has no logic.
class NumberSourceDataModel : public NodeDelegateModel
{
    Q_OBJECT
    Q_PROPERTY(double number READ number WRITE setNumber NOTIFY numberChanged)

public:
    NumberSourceDataModel();

    virtual ~NumberSourceDataModel() {}

public:
    QJsonObject save() const override;

    void load(QJsonObject const &p) override;

public:

    NodeDataType dataType(PortType portType, PortIndex portIndex) const override;

    std::shared_ptr<NodeData> outData(PortIndex port) override;

    void setInData(std::shared_ptr<NodeData>, PortIndex) override {}

    QWidget *embeddedWidget() override;

public:
    double number() const;

    void setNumber(double number);

    Q_INVOKABLE void printHello();

Q_SIGNALS:
    void numberChanged(double number);

private Q_SLOTS:

    void onTextEdited(QString const &string);

private:
    std::shared_ptr<DecimalData> _number;

    QLineEdit *_lineEdit=nullptr;
};

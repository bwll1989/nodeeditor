#include "TextModel.hpp"

#include <qtextbrowser.h>
#include <QtWidgets/QTextEdit>

TextModel::TextModel()
{
    //
    PortEditable=true;
    Resizable=true;
    registerOSCControl("/text", _textEdit);
}

unsigned int TextModel::nPorts(PortType portType) const
{
    unsigned int result = 1;

    switch (portType) {
    case PortType::In:
        result = 2;
        break;

    case PortType::Out:
        result = 2;

    default:
        break;
    }

    return result;
}

void TextModel::onTextEdited()
{
    Q_EMIT dataUpdated(0);
}

NodeDataType TextModel::dataType(PortType, PortIndex) const
{
    return TextData().type();
}

std::shared_ptr<NodeData> TextModel::outData(PortIndex const portIndex)
{
    Q_UNUSED(portIndex);
    return std::make_shared<TextData>(_textEdit->text());
}

QWidget *TextModel::embeddedWidget()
{
    if (!_textEdit) {


        connect(_textEdit, &QLineEdit::textChanged, this, &TextModel::onTextEdited);
    }

    return _textEdit;


}

void TextModel::setInData(std::shared_ptr<NodeData> data, PortIndex const)
{
    auto textData = std::dynamic_pointer_cast<TextData>(data);

    QString inputText;

    if (textData) {
        inputText = textData->text();
    } else {
        inputText = "";
    }

    _textEdit->setText(inputText);
}

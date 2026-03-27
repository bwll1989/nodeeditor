#pragma once

#include <QtGui/QColor>

#include "Export.hpp"
#include "Style.hpp"

namespace QtNodes {

class NODE_EDITOR_PUBLIC GroupStyle : public Style
{
public:
    GroupStyle();

    GroupStyle(QString jsonText);

    ~GroupStyle() = default;

public:
    static void setGroupStyle(QString jsonText);

public:
    void loadJson(QJsonObject const &json) override;

    QJsonObject toJson() const override;

public:
    QColor NormalColor;
    QColor SelectedColor;
    QColor FontColor;
    QColor GradientColor0;
    QColor GradientColor1;
    QColor GradientColor2;
    QColor GradientColor3;
    QColor CaptionColor;
    float PenWidth;
    float HoveredPenWidth;
    float BoundaryRadius;
    bool UseDataDefinedColors;
    float Opacity;
    float CaptionHeight;
};
} // namespace QtNodes

#include "GroupStyle.hpp"

#include "StyleCollection.hpp"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValueRef>

#include <QDebug>

#include <random>

using QtNodes::GroupStyle;

inline void initResources()
{
    Q_INIT_RESOURCE(resources);
}

GroupStyle::GroupStyle()
{
    // Explicit resources inialization for preventing the static initialization
    // order fiasco: https://isocpp.org/wiki/faq/ctors#static-init-order
    initResources();

    // This configuration is stored inside the compiled unit and is loaded statically
    loadJsonFile(":DefaultStyle.json");
}

GroupStyle::GroupStyle(QString jsonText)
{
    loadJsonFile(":DefaultStyle.json");
    loadJsonText(jsonText);
}

void GroupStyle::setGroupStyle(QString jsonText)
{
    GroupStyle style(jsonText);

    StyleCollection::setGroupStyle(style);
}

#ifdef STYLE_DEBUG
#define GROUP_STYLE_CHECK_UNDEFINED_VALUE(v, variable) \
    { \
        if (v.type() == QJsonValue::Undefined || v.type() == QJsonValue::Null) \
            qWarning() << "Undefined value for parameter:" << #variable; \
    }
#else
#define GROUP_STYLE_CHECK_UNDEFINED_VALUE(v, variable)
#endif

#define GROUP_VALUE_EXISTS(v) \
    (v.type() != QJsonValue::Undefined && v.type() != QJsonValue::Null)

#define GROUP_STYLE_READ_COLOR(values, variable) \
    { \
        auto valueRef = values[#variable]; \
        GROUP_STYLE_CHECK_UNDEFINED_VALUE(valueRef, variable) \
        if (GROUP_VALUE_EXISTS(valueRef)) { \
            if (valueRef.isArray()) { \
                auto colorArray = valueRef.toArray(); \
                std::vector<int> rgb; \
                rgb.reserve(3); \
                for (auto it = colorArray.begin(); it != colorArray.end(); ++it) { \
                    rgb.push_back((*it).toInt()); \
                } \
                variable = QColor(rgb[0], rgb[1], rgb[2]); \
            } else { \
                variable = QColor(valueRef.toString()); \
            } \
        } \
    }

#define GROUP_STYLE_WRITE_COLOR(values, variable) \
    { \
        values[#variable] = variable.name(); \
    }

#define GROUP_STYLE_READ_FLOAT(values, variable) \
    { \
        auto valueRef = values[#variable]; \
        GROUP_STYLE_CHECK_UNDEFINED_VALUE(valueRef, variable) \
        if (GROUP_VALUE_EXISTS(valueRef)) \
            variable = valueRef.toDouble(); \
    }

#define GROUP_STYLE_WRITE_FLOAT(values, variable) \
    { \
        values[#variable] = variable; \
    }

#define GROUP_STYLE_READ_BOOL(values, variable) \
    { \
        auto valueRef = values[#variable]; \
        GROUP_STYLE_CHECK_UNDEFINED_VALUE(valueRef, variable) \
        if (GROUP_VALUE_EXISTS(valueRef)) \
            variable = valueRef.toBool(); \
    }

#define GROUP_STYLE_WRITE_BOOL(values, variable) \
    { \
        values[#variable] = variable; \
    }

void GroupStyle::loadJson(QJsonObject const &json)
{
    QJsonValue nodeStyleValues = json["GroupStyle"];

    QJsonObject obj = nodeStyleValues.toObject();
    GROUP_STYLE_READ_COLOR(obj, NormalColor);
    GROUP_STYLE_READ_COLOR(obj, SelectedColor);
    GROUP_STYLE_READ_COLOR(obj, GradientColor0);
    GROUP_STYLE_READ_COLOR(obj, GradientColor1);
    GROUP_STYLE_READ_COLOR(obj, GradientColor2);
    GROUP_STYLE_READ_COLOR(obj, GradientColor3);
    GROUP_STYLE_READ_COLOR(obj, CaptionColor);
    GROUP_STYLE_READ_FLOAT(obj, Opacity);
    GROUP_STYLE_READ_COLOR(obj, FontColor);
    GROUP_STYLE_READ_FLOAT(obj, PenWidth);
    GROUP_STYLE_READ_FLOAT(obj, HoveredPenWidth);
    GROUP_STYLE_READ_FLOAT(obj, BoundaryRadius);
    GROUP_STYLE_READ_BOOL(obj, UseDataDefinedColors);
    GROUP_STYLE_READ_FLOAT(obj,CaptionHeight)

}

QJsonObject GroupStyle::toJson() const
{
    QJsonObject obj;
    GROUP_STYLE_WRITE_COLOR(obj, NormalColor);
    GROUP_STYLE_WRITE_COLOR(obj, SelectedColor);
    GROUP_STYLE_WRITE_COLOR(obj, GradientColor0);
    GROUP_STYLE_WRITE_COLOR(obj, GradientColor1);
    GROUP_STYLE_WRITE_COLOR(obj, GradientColor2);
    GROUP_STYLE_WRITE_COLOR(obj, GradientColor3);
    GROUP_STYLE_WRITE_COLOR(obj, CaptionColor);
    GROUP_STYLE_WRITE_COLOR(obj, FontColor);
    GROUP_STYLE_WRITE_FLOAT(obj, PenWidth);
    GROUP_STYLE_WRITE_FLOAT(obj, HoveredPenWidth);
    GROUP_STYLE_WRITE_FLOAT(obj, BoundaryRadius);
    GROUP_STYLE_WRITE_BOOL(obj,  UseDataDefinedColors);
    GROUP_STYLE_WRITE_FLOAT(obj, Opacity);
    GROUP_STYLE_WRITE_FLOAT(obj,CaptionHeight)

    QJsonObject root;
    root["GroupStyle"] = obj;

    return root;
}






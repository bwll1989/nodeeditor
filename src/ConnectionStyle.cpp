#include "ConnectionStyle.hpp"

#include "StyleCollection.hpp"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValueRef>

#include <QDebug>

#include <random>

using QtNodes::ConnectionStyle;

inline void initResources()
{
    Q_INIT_RESOURCE(resources);
}

ConnectionStyle::ConnectionStyle()
{
    // Explicit resources inialization for preventing the static initialization
    // order fiasco: https://isocpp.org/wiki/faq/ctors#static-init-order
    initResources();

    // This configuration is stored inside the compiled unit and is loaded statically
    loadJsonFile(":DefaultStyle.json");
}

ConnectionStyle::ConnectionStyle(QString jsonText)
{
    loadJsonFile(":DefaultStyle.json");
    loadJsonText(jsonText);
}

void ConnectionStyle::setConnectionStyle(QString jsonText)
{
    ConnectionStyle style(jsonText);

    StyleCollection::setConnectionStyle(style);
}

#ifdef STYLE_DEBUG
#define CONNECTION_STYLE_CHECK_UNDEFINED_VALUE(v, variable) \
    { \
        if (v.type() == QJsonValue::Undefined || v.type() == QJsonValue::Null) \
            qWarning() << "Undefined value for parameter:" << #variable; \
    }
#else
#define CONNECTION_STYLE_CHECK_UNDEFINED_VALUE(v, variable)
#endif

#define CONNECTION_VALUE_EXISTS(v) \
    (v.type() != QJsonValue::Undefined && v.type() != QJsonValue::Null)

#define CONNECTION_STYLE_READ_COLOR(values, variable) \
    { \
        auto valueRef = values[#variable]; \
        CONNECTION_STYLE_CHECK_UNDEFINED_VALUE(valueRef, variable) \
        if (CONNECTION_VALUE_EXISTS(valueRef)) { \
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

#define CONNECTION_STYLE_WRITE_COLOR(values, variable) \
    { \
        values[#variable] = variable.name(); \
    }

#define CONNECTION_STYLE_READ_FLOAT(values, variable) \
    { \
        auto valueRef = values[#variable]; \
        CONNECTION_STYLE_CHECK_UNDEFINED_VALUE(valueRef, variable) \
        if (CONNECTION_VALUE_EXISTS(valueRef)) \
            variable = valueRef.toDouble(); \
    }

#define CONNECTION_STYLE_WRITE_FLOAT(values, variable) \
    { \
        values[#variable] = variable; \
    }

#define CONNECTION_STYLE_READ_BOOL(values, variable) \
    { \
        auto valueRef = values[#variable]; \
        CONNECTION_STYLE_CHECK_UNDEFINED_VALUE(valueRef, variable) \
        if (CONNECTION_VALUE_EXISTS(valueRef)) \
            variable = valueRef.toBool(); \
    }

#define CONNECTION_STYLE_WRITE_BOOL(values, variable) \
    { \
        values[#variable] = variable; \
    }

void ConnectionStyle::loadJson(QJsonObject const &json)
{
    QJsonValue nodeStyleValues = json["ConnectionStyle"];

    QJsonObject obj = nodeStyleValues.toObject();

    CONNECTION_STYLE_READ_COLOR(obj, ConstructionColor);
    CONNECTION_STYLE_READ_COLOR(obj, NormalColor);
    CONNECTION_STYLE_READ_COLOR(obj, SelectedColor);
    CONNECTION_STYLE_READ_COLOR(obj, SelectedHaloColor);
    CONNECTION_STYLE_READ_COLOR(obj, HoveredColor);
    CONNECTION_STYLE_READ_COLOR(obj, FontColor);

    CONNECTION_STYLE_READ_FLOAT(obj, LineWidth);
    CONNECTION_STYLE_READ_FLOAT(obj, LineHoverWidth);
    CONNECTION_STYLE_READ_FLOAT(obj, LineSelectedWidth);
    CONNECTION_STYLE_READ_FLOAT(obj, ConstructionLineWidth);
    CONNECTION_STYLE_READ_FLOAT(obj, PointDiameter);

    CONNECTION_STYLE_READ_BOOL(obj, UseDataDefinedColors);
    CONNECTION_STYLE_READ_BOOL(obj, InArrow);
    CONNECTION_STYLE_READ_BOOL(obj, OutArrow);
}

QJsonObject ConnectionStyle::toJson() const
{
    QJsonObject obj;

    CONNECTION_STYLE_WRITE_COLOR(obj, ConstructionColor);
    CONNECTION_STYLE_WRITE_COLOR(obj, NormalColor);
    CONNECTION_STYLE_WRITE_COLOR(obj, SelectedColor);
    CONNECTION_STYLE_WRITE_COLOR(obj, SelectedHaloColor);
    CONNECTION_STYLE_WRITE_COLOR(obj, HoveredColor);
    CONNECTION_STYLE_WRITE_COLOR(obj, FontColor);

    CONNECTION_STYLE_WRITE_FLOAT(obj, LineWidth);
    CONNECTION_STYLE_WRITE_FLOAT(obj, LineHoverWidth);
    CONNECTION_STYLE_WRITE_FLOAT(obj, LineSelectedWidth);
    CONNECTION_STYLE_WRITE_FLOAT(obj, ConstructionLineWidth);
    CONNECTION_STYLE_WRITE_FLOAT(obj, PointDiameter);

    CONNECTION_STYLE_WRITE_BOOL(obj, UseDataDefinedColors);
    CONNECTION_STYLE_WRITE_BOOL(obj, InArrow);
    CONNECTION_STYLE_WRITE_BOOL(obj, OutArrow);
    QJsonObject root;
    root["ConnectionStyle"] = obj;

    return root;
}

QColor ConnectionStyle::constructionColor() const
{
    return ConstructionColor;
}

QColor ConnectionStyle::normalColor() const
{
    return NormalColor;
}

QColor ConnectionStyle::normalColor(QString typeId) const
{
    std::size_t hash = qHash(typeId);

    // HSL的色度范围扩展到359度（完整的色相环），最大化不同类型ID的色度差异
    std::size_t const hue_range = 359;

    // 使用黄金分割法确保色度均匀分布，避免相邻哈希值的色度过于接近
    // 黄金分割共轭数，保证生成的色相在色相环上均匀间隔
    const float golden_ratio = 0.61803398875f;
    // 基于哈希值生成唯一的初始相位
    float seed = static_cast<float>(hash % 360) / 360.0f;
    // 每次迭代添加黄金分割数，模1处理，确保分布在[0,1)区间
    seed = fmod(seed + golden_ratio, 1.0f);
    // 将归一化的种子值转换为0-359的色度值
    int hue = static_cast<int>(seed * 360);

    // 调整饱和度和亮度范围，保证颜色鲜艳且差异明显
    int sat = 160 + (hash % 96);  // 饱和度160-255，保持颜色饱和度足够高
    int lum = 130 + (hash % 70);  // 亮度130-200，避免过亮或过暗

    return QColor::fromHsl(hue, sat, lum);
}

QColor ConnectionStyle::selectedColor() const
{
    return SelectedColor;
}

QColor ConnectionStyle::selectedHaloColor() const
{
    return SelectedHaloColor;
}

QColor ConnectionStyle::hoveredColor() const
{
    return HoveredColor;
}

QColor ConnectionStyle::fontColor() const
{
    return FontColor.isValid() ? FontColor : QColor(30, 30, 30);
}

float ConnectionStyle::lineWidth() const
{
    return LineWidth;
}

float ConnectionStyle::lineHoverWidth() const
{
    return LineHoverWidth;
}

float ConnectionStyle::lineSelectedWidth() const
{
    return LineSelectedWidth;
}

float ConnectionStyle::constructionLineWidth() const
{
    return ConstructionLineWidth;
}

float ConnectionStyle::pointDiameter() const
{
    return PointDiameter;
}

bool ConnectionStyle::useDataDefinedColors() const
{
    return UseDataDefinedColors;
}


bool ConnectionStyle::inArrow() const
{
    return InArrow;
}
bool ConnectionStyle::outArrow() const
{
    return OutArrow;
}

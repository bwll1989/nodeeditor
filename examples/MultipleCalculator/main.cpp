#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QDir>
#include <QtNodes/ConnectionStyle>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/NodeData>
#include <QtNodes/NodeDelegateModelRegistry>

#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QVBoxLayout>

#include "AdditionModel.hpp"
#include "ContainerDataModel.hpp"
#include "DivisionModel.hpp"
#include "MultiplicationModel.hpp"
#include "NumberDisplayDataModel.hpp"
#include "NumberSourceDataModel.hpp"
#include "SubtractionModel.hpp"
#include "InDataModel.hpp"
#include "OutDataModel.hpp"
#include "ViewsTabWidget.hpp"

using QtNodes::ConnectionStyle;
using QtNodes::DataFlowGraphicsScene;
using QtNodes::NodeDelegateModelRegistry;

static std::shared_ptr<NodeDelegateModelRegistry> registerDataModels()
{
    auto ret = std::make_shared<NodeDelegateModelRegistry>();
    ret->registerModel<NumberSourceDataModel>("Number", "Sources");
    ret->registerModel<NumberDisplayDataModel>("Displays", "Displays");
    ret->registerModel<AdditionModel>("Addition", "Operators");
    ret->registerModel<SubtractionModel>("Subtraction", "Operators");
    ret->registerModel<MultiplicationModel>("Multiplication", "Operators");
    ret->registerModel<DivisionModel>("Division", "Operators");
    ret->registerModel<InDataModel>("In", "Interface");
    ret->registerModel<OutDataModel>("Out", "Interface");
    ret->registerModel<ContainerDataModel>("Container", "Interface");
    return ret;
}

static void setStyle()
{
    ConnectionStyle::setConnectionStyle(
        R"(
  {
    "ConnectionStyle": {
      "ConstructionColor": "gray",
      "NormalColor": "black",
      "SelectedColor": "gray",
      "SelectedHaloColor": "deepskyblue",
      "HoveredColor": "deepskyblue",
      "LineWidth": 3.0,
      "ConstructionLineWidth": 2.0,
      "PointDiameter": 10.0,
      "UseDataDefinedColors": true
    }
  }
  )");
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    setStyle();

    QWidget mainWidget;

    auto *menuBar = new QMenuBar();
    QMenu *menu = menuBar->addMenu(QStringLiteral("File"));

    auto *saveAction = menu->addAction(QStringLiteral("Save Scene"));
    saveAction->setShortcut(QKeySequence::Save);

    auto *loadAction = menu->addAction(QStringLiteral("Load Scene"));
    loadAction->setShortcut(QKeySequence::Open);

    auto *l = new QVBoxLayout(&mainWidget);
    l->addWidget(menuBar);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);

    std::shared_ptr<NodeDelegateModelRegistry> registry = registerDataModels();
    auto *host = new DataflowViewsManger();
    host->setDefaultRegistry(registry);
    l->addWidget(host);

    auto *scene = host->createRootScene(QStringLiteral("dataflow"));

    QObject::connect(saveAction, &QAction::triggered, host, [host]() {
        QString fileName = QFileDialog::getSaveFileName(nullptr,
                                                        QStringLiteral("Save Flow Scene"),
                                                        QDir::homePath(),
                                                        QStringLiteral("Flow Scene Files (*.flow)"));
        if (fileName.isEmpty())
            return;
        if (!fileName.endsWith(QStringLiteral("flow"), Qt::CaseInsensitive))
            fileName += QStringLiteral(".flow");

        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly))
            file.write(QJsonDocument(host->save()).toJson());
    });

    QObject::connect(loadAction, &QAction::triggered, host, [host, &mainWidget]() {
        QString fileName = QFileDialog::getOpenFileName(nullptr,
                                                        QStringLiteral("Open Flow Scene"),
                                                        QDir::homePath(),
                                                        QStringLiteral("Flow Scene Files (*.flow)"));
        if (!QFileInfo::exists(fileName))
            return;

        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly))
            return;

        host->load(QJsonDocument::fromJson(file.readAll()).object());
        if (auto *sc = host->currentScene()) {
            // UniqueConnection 不能配 lambda，这里 scene 已由 load 重建，普通 connect 即可
            QObject::connect(sc, &DataFlowGraphicsScene::modified, &mainWidget, [&mainWidget]() {
                mainWidget.setWindowModified(true);
            });
        }
        mainWidget.setWindowModified(false);
    });

    if (scene) {
        QObject::connect(scene, &DataFlowGraphicsScene::modified, &mainWidget, [&mainWidget]() {
            mainWidget.setWindowModified(true);
        });
    }

    mainWidget.setWindowTitle(QStringLiteral("[*]Data Flow: Container calculator"));
    mainWidget.resize(900, 650);
    mainWidget.move(QApplication::primaryScreen()->availableGeometry().center()
                    - mainWidget.rect().center());
    mainWidget.showNormal();

    return app.exec();
}

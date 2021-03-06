#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGridLayout>
#include <QToolBar>
#include <QPushButton>
#include <QFileDialog>
#include <QTabWidget>
#include <QBuffer>
#include <QMessageBox>
#include <QTimer>
#include <QMenuBar>
#include <QPointer>
#include <QApplication>
#include <set>
#include <QDesktopServices>
#include <QDockWidget>
#include "skeletondocumentwindow.h"
#include "skeletongraphicswidget.h"
#include "theme.h"
#include "ds3file.h"
#include "skeletonsnapshot.h"
#include "skeletonxml.h"
#include "logbrowser.h"
#include "dust3dutil.h"
#include "aboutwidget.h"
#include "version.h"
#include "gltffile.h"
#include "graphicscontainerwidget.h"
#include "skeletonparttreewidget.h"
#include "rigwidget.h"
#include "markiconcreator.h"

int SkeletonDocumentWindow::m_modelRenderWidgetInitialX = 16;
int SkeletonDocumentWindow::m_modelRenderWidgetInitialY = 16;
int SkeletonDocumentWindow::m_modelRenderWidgetInitialSize = 128;
int SkeletonDocumentWindow::m_skeletonRenderWidgetInitialX = SkeletonDocumentWindow::m_modelRenderWidgetInitialX + SkeletonDocumentWindow::m_modelRenderWidgetInitialSize + 16;
int SkeletonDocumentWindow::m_skeletonRenderWidgetInitialY = SkeletonDocumentWindow::m_modelRenderWidgetInitialY;
int SkeletonDocumentWindow::m_skeletonRenderWidgetInitialSize = SkeletonDocumentWindow::m_modelRenderWidgetInitialSize;

LogBrowser *g_logBrowser = nullptr;
std::set<SkeletonDocumentWindow *> g_documentWindows;
QTextBrowser *g_acknowlegementsWidget = nullptr;
AboutWidget *g_aboutWidget = nullptr;
QTextBrowser *g_contributorsWidget = nullptr;

void outputMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (g_logBrowser)
        g_logBrowser->outputMessage(type, msg, context.file, context.line);
}

void SkeletonDocumentWindow::showAcknowlegements()
{
    if (!g_acknowlegementsWidget) {
        g_acknowlegementsWidget = new QTextBrowser;
        g_acknowlegementsWidget->setWindowTitle(unifiedWindowTitle(tr("Acknowlegements")));
        g_acknowlegementsWidget->setMinimumSize(QSize(400, 300));
        QFile file(":/ACKNOWLEDGEMENTS.html");
        file.open(QFile::ReadOnly | QFile::Text);
        QTextStream stream(&file);
        g_acknowlegementsWidget->setHtml(stream.readAll());
    }
    g_acknowlegementsWidget->show();
    g_acknowlegementsWidget->activateWindow();
    g_acknowlegementsWidget->raise();
}

void SkeletonDocumentWindow::showContributors()
{
    if (!g_contributorsWidget) {
        g_contributorsWidget = new QTextBrowser;
        g_contributorsWidget->setWindowTitle(unifiedWindowTitle(tr("Contributors")));
        g_contributorsWidget->setMinimumSize(QSize(400, 300));
        QFile authors(":/AUTHORS");
        authors.open(QFile::ReadOnly | QFile::Text);
        QFile contributors(":/CONTRIBUTORS");
        contributors.open(QFile::ReadOnly | QFile::Text);
        g_contributorsWidget->setHtml("<h1>AUTHORS</h1><pre>" + authors.readAll() + "</pre><h1>CONTRIBUTORS</h1><pre>" + contributors.readAll() + "</pre>");
    }
    g_contributorsWidget->show();
    g_contributorsWidget->activateWindow();
    g_contributorsWidget->raise();
}

void SkeletonDocumentWindow::showAbout()
{
    if (!g_aboutWidget) {
        g_aboutWidget = new AboutWidget;
    }
    g_aboutWidget->show();
    g_aboutWidget->activateWindow();
    g_aboutWidget->raise();
}

SkeletonDocumentWindow::SkeletonDocumentWindow() :
    m_document(nullptr),
    m_firstShow(true),
    m_documentSaved(true),
    m_exportPreviewWidget(nullptr),
    m_advanceSettingWidget(nullptr)
{
    if (!g_logBrowser) {
        g_logBrowser = new LogBrowser;
        qInstallMessageHandler(&outputMessage);
    }

    g_documentWindows.insert(this);

    m_document = new SkeletonDocument;

    QVBoxLayout *toolButtonLayout = new QVBoxLayout;
    toolButtonLayout->setSpacing(0);
    toolButtonLayout->setContentsMargins(5, 10, 4, 0);

    QPushButton *addButton = new QPushButton(QChar(fa::plus));
    Theme::initAwesomeButton(addButton);

    QPushButton *selectButton = new QPushButton(QChar(fa::mousepointer));
    Theme::initAwesomeButton(selectButton);

    QPushButton *dragButton = new QPushButton(QChar(fa::handrocko));
    Theme::initAwesomeButton(dragButton);

    QPushButton *zoomInButton = new QPushButton(QChar(fa::searchplus));
    Theme::initAwesomeButton(zoomInButton);

    QPushButton *zoomOutButton = new QPushButton(QChar(fa::searchminus));
    Theme::initAwesomeButton(zoomOutButton);

    m_xlockButton = new QPushButton(QChar('X'));
    initLockButton(m_xlockButton);
    updateXlockButtonState();

    m_ylockButton = new QPushButton(QChar('Y'));
    initLockButton(m_ylockButton);
    updateYlockButtonState();

    m_zlockButton = new QPushButton(QChar('Z'));
    initLockButton(m_zlockButton);
    updateZlockButtonState();
    
    m_radiusLockButton = new QPushButton(QChar(fa::bullseye));
    Theme::initAwesomeButton(m_radiusLockButton);
    updateRadiusLockButtonState();
    
    QPushButton *rotateCounterclockwiseButton = new QPushButton(QChar(fa::rotateleft));
    Theme::initAwesomeButton(rotateCounterclockwiseButton);
    
    QPushButton *rotateClockwiseButton = new QPushButton(QChar(fa::rotateright));
    Theme::initAwesomeButton(rotateClockwiseButton);

    toolButtonLayout->addWidget(addButton);
    toolButtonLayout->addWidget(selectButton);
    toolButtonLayout->addWidget(dragButton);
    toolButtonLayout->addWidget(zoomInButton);
    toolButtonLayout->addWidget(zoomOutButton);
    toolButtonLayout->addSpacing(10);
    toolButtonLayout->addWidget(m_xlockButton);
    toolButtonLayout->addWidget(m_ylockButton);
    toolButtonLayout->addWidget(m_zlockButton);
    toolButtonLayout->addWidget(m_radiusLockButton);
    toolButtonLayout->addSpacing(10);
    toolButtonLayout->addWidget(rotateCounterclockwiseButton);
    toolButtonLayout->addWidget(rotateClockwiseButton);

    QLabel *verticalLogoLabel = new QLabel;
    QImage verticalLogoImage;
    verticalLogoImage.load(":/resources/dust3d-vertical.png");
    verticalLogoLabel->setPixmap(QPixmap::fromImage(verticalLogoImage));

    QHBoxLayout *logoLayout = new QHBoxLayout;
    logoLayout->addWidget(verticalLogoLabel);
    logoLayout->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout *mainLeftLayout = new QVBoxLayout;
    mainLeftLayout->setSpacing(0);
    mainLeftLayout->setContentsMargins(0, 0, 0, 0);
    mainLeftLayout->addLayout(toolButtonLayout);
    mainLeftLayout->addStretch();
    mainLeftLayout->addLayout(logoLayout);
    mainLeftLayout->addSpacing(10);

    SkeletonGraphicsWidget *graphicsWidget = new SkeletonGraphicsWidget(m_document);
    m_graphicsWidget = graphicsWidget;

    GraphicsContainerWidget *containerWidget = new GraphicsContainerWidget;
    containerWidget->setGraphicsWidget(graphicsWidget);
    QGridLayout *containerLayout = new QGridLayout;
    containerLayout->setSpacing(0);
    containerLayout->setContentsMargins(1, 0, 0, 0);
    containerLayout->addWidget(graphicsWidget);
    containerWidget->setLayout(containerLayout);
    containerWidget->setMinimumSize(400, 400);

    m_modelRenderWidget = new ModelWidget(containerWidget);
    m_modelRenderWidget->setMinimumSize(SkeletonDocumentWindow::m_modelRenderWidgetInitialSize, SkeletonDocumentWindow::m_modelRenderWidgetInitialSize);
    m_modelRenderWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_modelRenderWidget->move(SkeletonDocumentWindow::m_modelRenderWidgetInitialX, SkeletonDocumentWindow::m_modelRenderWidgetInitialY);
    m_modelRenderWidget->setGraphicsFunctions(graphicsWidget);
    
    m_document->setSharedContextWidget(m_modelRenderWidget);
    
    setTabPosition(Qt::RightDockWidgetArea, QTabWidget::East);

    QDockWidget *partTreeDocker = new QDockWidget(tr("Parts"), this);
    partTreeDocker->setAllowedAreas(Qt::RightDockWidgetArea);
    SkeletonPartTreeWidget *partTreeWidget = new SkeletonPartTreeWidget(m_document, partTreeDocker);
    partTreeWidget->setGraphicsFunctions(graphicsWidget);
    partTreeDocker->setWidget(partTreeWidget);
    addDockWidget(Qt::RightDockWidgetArea, partTreeDocker);
    connect(partTreeDocker, &QDockWidget::topLevelChanged, [=](bool topLevel) {
        Q_UNUSED(topLevel);
        for (const auto &part: m_document->partMap)
            partTreeWidget->partPreviewChanged(part.first);
    });
    
    QDockWidget *rigDocker = new QDockWidget(tr("Rig"), this);
    rigDocker->setAllowedAreas(Qt::RightDockWidgetArea);
    m_rigWidget = new RigWidget(m_document, rigDocker);
    rigDocker->setWidget(m_rigWidget);
    addDockWidget(Qt::RightDockWidgetArea, rigDocker);
    connect(rigDocker, &QDockWidget::topLevelChanged, [=](bool topLevel) {
        Q_UNUSED(topLevel);
        updateRigWeightRenderWidget();
    });
    
    QDockWidget *poseDocker = new QDockWidget(tr("Poses"), this);
    poseDocker->setAllowedAreas(Qt::RightDockWidgetArea);
    PoseManageWidget *poseManageWidget = new PoseManageWidget(m_document, poseDocker);
    poseDocker->setWidget(poseManageWidget);
    connect(poseManageWidget, &PoseManageWidget::registerDialog, this, &SkeletonDocumentWindow::registerDialog);
    connect(poseManageWidget, &PoseManageWidget::unregisterDialog, this, &SkeletonDocumentWindow::unregisterDialog);
    addDockWidget(Qt::RightDockWidgetArea, poseDocker);
    connect(poseDocker, &QDockWidget::topLevelChanged, [=](bool topLevel) {
        Q_UNUSED(topLevel);
        for (const auto &pose: m_document->poseMap)
            emit m_document->posePreviewChanged(pose.first);
    });
    
    tabifyDockWidget(partTreeDocker, rigDocker);
    tabifyDockWidget(rigDocker, poseDocker);
    
    partTreeDocker->raise();

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(mainLeftLayout);
    mainLayout->addWidget(containerWidget);
    mainLayout->addSpacing(3);

    QWidget *centralWidget = new QWidget;
    centralWidget->setLayout(mainLayout);

    setCentralWidget(centralWidget);
    setWindowTitle(APP_NAME);

    m_fileMenu = menuBar()->addMenu(tr("File"));

    m_newWindowAction = new QAction(tr("New Window"), this);
    connect(m_newWindowAction, &QAction::triggered, this, &SkeletonDocumentWindow::newWindow, Qt::QueuedConnection);
    m_fileMenu->addAction(m_newWindowAction);

    m_newDocumentAction = new QAction(tr("New"), this);
    connect(m_newDocumentAction, &QAction::triggered, this, &SkeletonDocumentWindow::newDocument);
    m_fileMenu->addAction(m_newDocumentAction);

    m_openAction = new QAction(tr("Open..."), this);
    connect(m_openAction, &QAction::triggered, this, &SkeletonDocumentWindow::open, Qt::QueuedConnection);
    m_fileMenu->addAction(m_openAction);

    m_saveAction = new QAction(tr("Save"), this);
    connect(m_saveAction, &QAction::triggered, this, &SkeletonDocumentWindow::save, Qt::QueuedConnection);
    m_fileMenu->addAction(m_saveAction);

    m_saveAsAction = new QAction(tr("Save As..."), this);
    connect(m_saveAsAction, &QAction::triggered, this, &SkeletonDocumentWindow::saveAs, Qt::QueuedConnection);
    m_fileMenu->addAction(m_saveAsAction);

    m_saveAllAction = new QAction(tr("Save All"), this);
    connect(m_saveAllAction, &QAction::triggered, this, &SkeletonDocumentWindow::saveAll, Qt::QueuedConnection);
    m_fileMenu->addAction(m_saveAllAction);

    m_fileMenu->addSeparator();

    //m_exportMenu = m_fileMenu->addMenu(tr("Export"));

    m_exportAction = new QAction(tr("Export..."), this);
    connect(m_exportAction, &QAction::triggered, this, &SkeletonDocumentWindow::showExportPreview, Qt::QueuedConnection);
    m_fileMenu->addAction(m_exportAction);
    
    m_exportAsObjAction = new QAction(tr("Export as OBJ..."), this);
    connect(m_exportAsObjAction, &QAction::triggered, this, &SkeletonDocumentWindow::exportObjResult, Qt::QueuedConnection);
    m_fileMenu->addAction(m_exportAsObjAction);
    
    //m_exportRenderedAsImageAction = new QAction(tr("Export as PNG..."), this);
    //connect(m_exportRenderedAsImageAction, &QAction::triggered, this, &SkeletonDocumentWindow::exportRenderedResult, Qt::QueuedConnection);
    //m_fileMenu->addAction(m_exportRenderedAsImageAction);
    
    //m_exportAsObjPlusMaterialsAction = new QAction(tr("Wavefront (.obj + .mtl)..."), this);
    //connect(m_exportAsObjPlusMaterialsAction, &QAction::triggered, this, &SkeletonDocumentWindow::exportObjPlusMaterialsResult, Qt::QueuedConnection);
    //m_exportMenu->addAction(m_exportAsObjPlusMaterialsAction);
    
    m_fileMenu->addSeparator();

    m_changeTurnaroundAction = new QAction(tr("Change Reference Sheet..."), this);
    connect(m_changeTurnaroundAction, &QAction::triggered, this, &SkeletonDocumentWindow::changeTurnaround, Qt::QueuedConnection);
    m_fileMenu->addAction(m_changeTurnaroundAction);
    
    m_fileMenu->addSeparator();

    connect(m_fileMenu, &QMenu::aboutToShow, [=]() {
        m_exportAsObjAction->setEnabled(m_graphicsWidget->hasItems());
        //m_exportAsObjPlusMaterialsAction->setEnabled(m_graphicsWidget->hasItems());
        m_exportAction->setEnabled(m_graphicsWidget->hasItems());
        //m_exportRenderedAsImageAction->setEnabled(m_graphicsWidget->hasItems());
    });

    m_editMenu = menuBar()->addMenu(tr("Edit"));

    m_addAction = new QAction(tr("Add..."), this);
    connect(m_addAction, &QAction::triggered, [=]() {
        m_document->setEditMode(SkeletonDocumentEditMode::Add);
    });
    m_editMenu->addAction(m_addAction);

    m_undoAction = new QAction(tr("Undo"), this);
    connect(m_undoAction, &QAction::triggered, m_document, &SkeletonDocument::undo);
    m_editMenu->addAction(m_undoAction);

    m_redoAction = new QAction(tr("Redo"), this);
    connect(m_redoAction, &QAction::triggered, m_document, &SkeletonDocument::redo);
    m_editMenu->addAction(m_redoAction);

    m_deleteAction = new QAction(tr("Delete"), this);
    connect(m_deleteAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::deleteSelected);
    m_editMenu->addAction(m_deleteAction);

    m_breakAction = new QAction(tr("Break"), this);
    connect(m_breakAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::breakSelected);
    m_editMenu->addAction(m_breakAction);

    m_connectAction = new QAction(tr("Connect"), this);
    connect(m_connectAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::connectSelected);
    m_editMenu->addAction(m_connectAction);

    m_cutAction = new QAction(tr("Cut"), this);
    connect(m_cutAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::cut);
    m_editMenu->addAction(m_cutAction);

    m_copyAction = new QAction(tr("Copy"), this);
    connect(m_copyAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::copy);
    m_editMenu->addAction(m_copyAction);

    m_pasteAction = new QAction(tr("Paste"), this);
    connect(m_pasteAction, &QAction::triggered, m_document, &SkeletonDocument::paste);
    m_editMenu->addAction(m_pasteAction);

    m_flipHorizontallyAction = new QAction(tr("H Flip"), this);
    connect(m_flipHorizontallyAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::flipHorizontally);
    m_editMenu->addAction(m_flipHorizontallyAction);

    m_flipVerticallyAction = new QAction(tr("V Flip"), this);
    connect(m_flipVerticallyAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::flipVertically);
    m_editMenu->addAction(m_flipVerticallyAction);

    m_rotateClockwiseAction = new QAction(tr("Rotate 90D CW"), this);
    connect(m_rotateClockwiseAction, &QAction::triggered, [=] {
        m_graphicsWidget->rotateClockwise90Degree();
    });
    m_editMenu->addAction(m_rotateClockwiseAction);

    m_rotateCounterclockwiseAction = new QAction(tr("Rotate 90D CCW"), this);
    connect(m_rotateCounterclockwiseAction, &QAction::triggered, [=] {
        m_graphicsWidget->rotateCounterclockwise90Degree();
    });
    m_editMenu->addAction(m_rotateCounterclockwiseAction);

    m_switchXzAction = new QAction(tr("Switch XZ"), this);
    connect(m_switchXzAction, &QAction::triggered, [=] {
        m_graphicsWidget->switchSelectedXZ();
    });
    m_editMenu->addAction(m_switchXzAction);

    m_alignToMenu = new QMenu(tr("Align To"));

    m_alignToLocalCenterAction = new QAction(tr("Local Center"), this);
    connect(m_alignToLocalCenterAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::alignSelectedToLocalCenter);
    m_alignToMenu->addAction(m_alignToLocalCenterAction);

    m_alignToLocalVerticalCenterAction = new QAction(tr("Local Vertical Center"), this);
    connect(m_alignToLocalVerticalCenterAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::alignSelectedToLocalVerticalCenter);
    m_alignToMenu->addAction(m_alignToLocalVerticalCenterAction);

    m_alignToLocalHorizontalCenterAction = new QAction(tr("Local Horizontal Center"), this);
    connect(m_alignToLocalHorizontalCenterAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::alignSelectedToLocalHorizontalCenter);
    m_alignToMenu->addAction(m_alignToLocalHorizontalCenterAction);
    
    m_alignToGlobalCenterAction = new QAction(tr("Global Center"), this);
    connect(m_alignToGlobalCenterAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::alignSelectedToGlobalCenter);
    m_alignToMenu->addAction(m_alignToGlobalCenterAction);

    m_alignToGlobalVerticalCenterAction = new QAction(tr("Global Vertical Center"), this);
    connect(m_alignToGlobalVerticalCenterAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::alignSelectedToGlobalVerticalCenter);
    m_alignToMenu->addAction(m_alignToGlobalVerticalCenterAction);

    m_alignToGlobalHorizontalCenterAction = new QAction(tr("Global Horizontal Center"), this);
    connect(m_alignToGlobalHorizontalCenterAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::alignSelectedToGlobalHorizontalCenter);
    m_alignToMenu->addAction(m_alignToGlobalHorizontalCenterAction);

    m_editMenu->addMenu(m_alignToMenu);
    
    m_markAsMenu = new QMenu(tr("Mark As"));

    m_markAsNoneAction = new QAction(tr("None"), this);
    connect(m_markAsNoneAction, &QAction::triggered, [=]() {
        m_graphicsWidget->setSelectedNodesBoneMark(SkeletonBoneMark::None);
    });
    m_markAsMenu->addAction(m_markAsNoneAction);

    m_markAsMenu->addSeparator();

    for (int i = 0; i < (int)SkeletonBoneMark::Count - 1; i++) {
        SkeletonBoneMark boneMark = (SkeletonBoneMark)(i + 1);
        m_markAsActions[i] = new QAction(MarkIconCreator::createIcon(boneMark), SkeletonBoneMarkToDispName(boneMark), this);
        connect(m_markAsActions[i], &QAction::triggered, [=]() {
            m_graphicsWidget->setSelectedNodesBoneMark(boneMark);
        });
        m_markAsMenu->addAction(m_markAsActions[i]);
    }

    m_editMenu->addMenu(m_markAsMenu);

    m_selectAllAction = new QAction(tr("Select All"), this);
    connect(m_selectAllAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::selectAll);
    m_editMenu->addAction(m_selectAllAction);

    m_selectPartAllAction = new QAction(tr("Select Part"), this);
    connect(m_selectPartAllAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::selectPartAll);
    m_editMenu->addAction(m_selectPartAllAction);

    m_unselectAllAction = new QAction(tr("Unselect All"), this);
    connect(m_unselectAllAction, &QAction::triggered, m_graphicsWidget, &SkeletonGraphicsWidget::unselectAll);
    m_editMenu->addAction(m_unselectAllAction);

    connect(m_editMenu, &QMenu::aboutToShow, [=]() {
        m_undoAction->setEnabled(m_document->undoable());
        m_redoAction->setEnabled(m_document->redoable());
        m_deleteAction->setEnabled(m_graphicsWidget->hasSelection());
        m_breakAction->setEnabled(m_graphicsWidget->hasEdgeSelection());
        m_connectAction->setEnabled(m_graphicsWidget->hasTwoDisconnectedNodesSelection());
        m_cutAction->setEnabled(m_graphicsWidget->hasSelection());
        m_copyAction->setEnabled(m_graphicsWidget->hasSelection());
        m_pasteAction->setEnabled(m_document->hasPastableNodesInClipboard());
        m_flipHorizontallyAction->setEnabled(m_graphicsWidget->hasMultipleSelection());
        m_flipVerticallyAction->setEnabled(m_graphicsWidget->hasMultipleSelection());
        m_rotateClockwiseAction->setEnabled(m_graphicsWidget->hasMultipleSelection());
        m_rotateCounterclockwiseAction->setEnabled(m_graphicsWidget->hasMultipleSelection());
        m_switchXzAction->setEnabled(m_graphicsWidget->hasSelection());
        m_alignToGlobalCenterAction->setEnabled(m_graphicsWidget->hasSelection() && m_document->originSettled());
        m_alignToGlobalVerticalCenterAction->setEnabled(m_graphicsWidget->hasSelection() && m_document->originSettled());
        m_alignToGlobalHorizontalCenterAction->setEnabled(m_graphicsWidget->hasSelection() && m_document->originSettled());
        m_alignToLocalCenterAction->setEnabled(m_graphicsWidget->hasMultipleSelection());
        m_alignToLocalVerticalCenterAction->setEnabled(m_graphicsWidget->hasMultipleSelection());
        m_alignToLocalHorizontalCenterAction->setEnabled(m_graphicsWidget->hasMultipleSelection());
        m_alignToMenu->setEnabled((m_graphicsWidget->hasSelection() && m_document->originSettled()) || m_graphicsWidget->hasMultipleSelection());
        m_selectAllAction->setEnabled(m_graphicsWidget->hasItems());
        m_selectPartAllAction->setEnabled(m_graphicsWidget->hasItems());
        m_unselectAllAction->setEnabled(m_graphicsWidget->hasSelection());
    });

    m_viewMenu = menuBar()->addMenu(tr("View"));

    auto isModelSitInVisibleArea = [](ModelWidget *modelWidget) {
        QRect parentRect = QRect(QPoint(0, 0), modelWidget->parentWidget()->size());
        return parentRect.contains(modelWidget->geometry().center());
    };

    m_resetModelWidgetPosAction = new QAction(tr("Show Model"), this);
    connect(m_resetModelWidgetPosAction, &QAction::triggered, [=]() {
        if (!isModelSitInVisibleArea(m_modelRenderWidget)) {
            m_modelRenderWidget->move(SkeletonDocumentWindow::m_modelRenderWidgetInitialX, SkeletonDocumentWindow::m_modelRenderWidgetInitialY);
        }
    });
    m_viewMenu->addAction(m_resetModelWidgetPosAction);

    m_toggleWireframeAction = new QAction(tr("Toggle Wireframe"), this);
    connect(m_toggleWireframeAction, &QAction::triggered, [=]() {
        m_modelRenderWidget->toggleWireframe();
    });
    m_viewMenu->addAction(m_toggleWireframeAction);
    
    m_toggleSmoothNormalAction = new QAction(tr("Toggle Smooth Normal"), this);
    connect(m_toggleSmoothNormalAction, &QAction::triggered, [=]() {
        m_document->toggleSmoothNormal();
    });
    m_viewMenu->addAction(m_toggleSmoothNormalAction);

    connect(m_viewMenu, &QMenu::aboutToShow, [=]() {
        m_resetModelWidgetPosAction->setEnabled(!isModelSitInVisibleArea(m_modelRenderWidget));
    });
    
    m_windowMenu = menuBar()->addMenu(tr("Window"));
    
    m_showPartsListAction = new QAction(tr("Parts"), this);
    connect(m_showPartsListAction, &QAction::triggered, [=]() {
        partTreeDocker->show();
        partTreeDocker->raise();
    });
    m_windowMenu->addAction(m_showPartsListAction);
    
    m_showRigAction = new QAction(tr("Rig"), this);
    connect(m_showRigAction, &QAction::triggered, [=]() {
        rigDocker->show();
        rigDocker->raise();
    });
    m_windowMenu->addAction(m_showRigAction);
    
    m_showPosesAction = new QAction(tr("Poses"), this);
    connect(m_showPosesAction, &QAction::triggered, [=]() {
        poseDocker->show();
        poseDocker->raise();
    });
    m_windowMenu->addAction(m_showPosesAction);
    
    QMenu *dialogsMenu = m_windowMenu->addMenu(tr("Dialogs"));
    connect(dialogsMenu, &QMenu::aboutToShow, [=]() {
        dialogsMenu->clear();
        if (this->m_dialogs.empty()) {
            QAction *action = dialogsMenu->addAction(tr("None"));
            action->setEnabled(false);
            return;
        }
        for (const auto &dialog: this->m_dialogs) {
            QAction *action = dialogsMenu->addAction(dialog->windowTitle());
            connect(action, &QAction::triggered, [=]() {
                dialog->show();
                dialog->raise();
            });
        }
    });
    
    m_showDebugDialogAction = new QAction(tr("Debug"), this);
    connect(m_showDebugDialogAction, &QAction::triggered, g_logBrowser, &LogBrowser::showDialog);
    m_windowMenu->addAction(m_showDebugDialogAction);
    
    m_showAdvanceSettingAction = new QAction(tr("Advance"), this);
    connect(m_showAdvanceSettingAction, &QAction::triggered, this, &SkeletonDocumentWindow::showAdvanceSetting);
#ifndef NDEBUG
    m_windowMenu->addAction(m_showAdvanceSettingAction);
#endif

    m_helpMenu = menuBar()->addMenu(tr("Help"));

    m_viewSourceAction = new QAction(tr("Fork me on GitHub"), this);
    connect(m_viewSourceAction, &QAction::triggered, this, &SkeletonDocumentWindow::viewSource);
    m_helpMenu->addAction(m_viewSourceAction);

    m_helpMenu->addSeparator();

    m_seeReferenceGuideAction = new QAction(tr("Reference Guide"), this);
    connect(m_seeReferenceGuideAction, &QAction::triggered, this, &SkeletonDocumentWindow::seeReferenceGuide);
    m_helpMenu->addAction(m_seeReferenceGuideAction);

    m_helpMenu->addSeparator();

    m_aboutAction = new QAction(tr("About"), this);
    connect(m_aboutAction, &QAction::triggered, this, &SkeletonDocumentWindow::about);
    m_helpMenu->addAction(m_aboutAction);

    m_reportIssuesAction = new QAction(tr("Report Issues"), this);
    connect(m_reportIssuesAction, &QAction::triggered, this, &SkeletonDocumentWindow::reportIssues);
    m_helpMenu->addAction(m_reportIssuesAction);
    
    m_helpMenu->addSeparator();

    m_seeContributorsAction = new QAction(tr("Contributors"), this);
    connect(m_seeContributorsAction, &QAction::triggered, this, &SkeletonDocumentWindow::seeContributors);
    m_helpMenu->addAction(m_seeContributorsAction);

    m_seeAcknowlegementsAction = new QAction(tr("Acknowlegements"), this);
    connect(m_seeAcknowlegementsAction, &QAction::triggered, this, &SkeletonDocumentWindow::seeAcknowlegements);
    m_helpMenu->addAction(m_seeAcknowlegementsAction);

    connect(containerWidget, &GraphicsContainerWidget::containerSizeChanged,
        graphicsWidget, &SkeletonGraphicsWidget::canvasResized);

    connect(m_document, &SkeletonDocument::turnaroundChanged,
        graphicsWidget, &SkeletonGraphicsWidget::turnaroundChanged);

    connect(rotateCounterclockwiseButton, &QPushButton::clicked, graphicsWidget, &SkeletonGraphicsWidget::rotateAllMainProfileCounterclockwise90DegreeAlongOrigin);
    connect(rotateClockwiseButton, &QPushButton::clicked, graphicsWidget, &SkeletonGraphicsWidget::rotateAllMainProfileClockwise90DegreeAlongOrigin);

    connect(addButton, &QPushButton::clicked, [=]() {
        m_document->setEditMode(SkeletonDocumentEditMode::Add);
    });

    connect(selectButton, &QPushButton::clicked, [=]() {
        m_document->setEditMode(SkeletonDocumentEditMode::Select);
    });

    connect(dragButton, &QPushButton::clicked, [=]() {
        m_document->setEditMode(SkeletonDocumentEditMode::Drag);
    });

    connect(zoomInButton, &QPushButton::clicked, [=]() {
        m_document->setEditMode(SkeletonDocumentEditMode::ZoomIn);
    });

    connect(zoomOutButton, &QPushButton::clicked, [=]() {
        m_document->setEditMode(SkeletonDocumentEditMode::ZoomOut);
    });

    connect(m_xlockButton, &QPushButton::clicked, [=]() {
        m_document->setXlockState(!m_document->xlocked);
    });
    connect(m_ylockButton, &QPushButton::clicked, [=]() {
        m_document->setYlockState(!m_document->ylocked);
    });
    connect(m_zlockButton, &QPushButton::clicked, [=]() {
        m_document->setZlockState(!m_document->zlocked);
    });
    connect(m_radiusLockButton, &QPushButton::clicked, [=]() {
        m_document->setRadiusLockState(!m_document->radiusLocked);
    });

    m_partListDockerVisibleSwitchConnection = connect(m_document, &SkeletonDocument::skeletonChanged, [=]() {
        if (m_graphicsWidget->hasItems()) {
            if (partTreeDocker->isHidden())
                partTreeDocker->show();
            disconnect(m_partListDockerVisibleSwitchConnection);
        }
    });

    connect(m_document, &SkeletonDocument::editModeChanged, graphicsWidget, &SkeletonGraphicsWidget::editModeChanged);

    connect(graphicsWidget, &SkeletonGraphicsWidget::zoomRenderedModelBy, m_modelRenderWidget, &ModelWidget::zoom);

    connect(graphicsWidget, &SkeletonGraphicsWidget::addNode, m_document, &SkeletonDocument::addNode);
    connect(graphicsWidget, &SkeletonGraphicsWidget::scaleNodeByAddRadius, m_document, &SkeletonDocument::scaleNodeByAddRadius);
    connect(graphicsWidget, &SkeletonGraphicsWidget::moveNodeBy, m_document, &SkeletonDocument::moveNodeBy);
    connect(graphicsWidget, &SkeletonGraphicsWidget::setNodeOrigin, m_document, &SkeletonDocument::setNodeOrigin);
    connect(graphicsWidget, &SkeletonGraphicsWidget::setNodeBoneMark, m_document, &SkeletonDocument::setNodeBoneMark);
    connect(graphicsWidget, &SkeletonGraphicsWidget::removeNode, m_document, &SkeletonDocument::removeNode);
    connect(graphicsWidget, &SkeletonGraphicsWidget::setEditMode, m_document, &SkeletonDocument::setEditMode);
    connect(graphicsWidget, &SkeletonGraphicsWidget::removeEdge, m_document, &SkeletonDocument::removeEdge);
    connect(graphicsWidget, &SkeletonGraphicsWidget::addEdge, m_document, &SkeletonDocument::addEdge);
    connect(graphicsWidget, &SkeletonGraphicsWidget::groupOperationAdded, m_document, &SkeletonDocument::saveSnapshot);
    connect(graphicsWidget, &SkeletonGraphicsWidget::undo, m_document, &SkeletonDocument::undo);
    connect(graphicsWidget, &SkeletonGraphicsWidget::redo, m_document, &SkeletonDocument::redo);
    connect(graphicsWidget, &SkeletonGraphicsWidget::paste, m_document, &SkeletonDocument::paste);
    connect(graphicsWidget, &SkeletonGraphicsWidget::batchChangeBegin, m_document, &SkeletonDocument::batchChangeBegin);
    connect(graphicsWidget, &SkeletonGraphicsWidget::batchChangeEnd, m_document, &SkeletonDocument::batchChangeEnd);
    connect(graphicsWidget, &SkeletonGraphicsWidget::breakEdge, m_document, &SkeletonDocument::breakEdge);
    connect(graphicsWidget, &SkeletonGraphicsWidget::moveOriginBy, m_document, &SkeletonDocument::moveOriginBy);
    connect(graphicsWidget, &SkeletonGraphicsWidget::partChecked, m_document, &SkeletonDocument::partChecked);
    connect(graphicsWidget, &SkeletonGraphicsWidget::partUnchecked, m_document, &SkeletonDocument::partUnchecked);
    connect(graphicsWidget, &SkeletonGraphicsWidget::switchNodeXZ, m_document, &SkeletonDocument::switchNodeXZ);

    connect(graphicsWidget, &SkeletonGraphicsWidget::setPartLockState, m_document, &SkeletonDocument::setPartLockState);
    connect(graphicsWidget, &SkeletonGraphicsWidget::setPartVisibleState, m_document, &SkeletonDocument::setPartVisibleState);
    connect(graphicsWidget, &SkeletonGraphicsWidget::setPartSubdivState, m_document, &SkeletonDocument::setPartSubdivState);
    connect(graphicsWidget, &SkeletonGraphicsWidget::setPartDisableState, m_document, &SkeletonDocument::setPartDisableState);
    connect(graphicsWidget, &SkeletonGraphicsWidget::setPartXmirrorState, m_document, &SkeletonDocument::setPartXmirrorState);
    connect(graphicsWidget, &SkeletonGraphicsWidget::setPartRoundState, m_document, &SkeletonDocument::setPartRoundState);
    connect(graphicsWidget, &SkeletonGraphicsWidget::setPartWrapState, m_document, &SkeletonDocument::setPartWrapState);

    connect(graphicsWidget, &SkeletonGraphicsWidget::setXlockState, m_document, &SkeletonDocument::setXlockState);
    connect(graphicsWidget, &SkeletonGraphicsWidget::setYlockState, m_document, &SkeletonDocument::setYlockState);
    connect(graphicsWidget, &SkeletonGraphicsWidget::setZlockState, m_document, &SkeletonDocument::setZlockState);
    
    connect(graphicsWidget, &SkeletonGraphicsWidget::enableAllPositionRelatedLocks, m_document, &SkeletonDocument::enableAllPositionRelatedLocks);
    connect(graphicsWidget, &SkeletonGraphicsWidget::disableAllPositionRelatedLocks, m_document, &SkeletonDocument::disableAllPositionRelatedLocks);

    connect(graphicsWidget, &SkeletonGraphicsWidget::changeTurnaround, this, &SkeletonDocumentWindow::changeTurnaround);
    connect(graphicsWidget, &SkeletonGraphicsWidget::save, this, &SkeletonDocumentWindow::save);
    connect(graphicsWidget, &SkeletonGraphicsWidget::open, this, &SkeletonDocumentWindow::open);

    connect(m_document, &SkeletonDocument::nodeAdded, graphicsWidget, &SkeletonGraphicsWidget::nodeAdded);
    connect(m_document, &SkeletonDocument::nodeRemoved, graphicsWidget, &SkeletonGraphicsWidget::nodeRemoved);
    connect(m_document, &SkeletonDocument::edgeAdded, graphicsWidget, &SkeletonGraphicsWidget::edgeAdded);
    connect(m_document, &SkeletonDocument::edgeRemoved, graphicsWidget, &SkeletonGraphicsWidget::edgeRemoved);
    connect(m_document, &SkeletonDocument::nodeRadiusChanged, graphicsWidget, &SkeletonGraphicsWidget::nodeRadiusChanged);
    connect(m_document, &SkeletonDocument::nodeBoneMarkChanged, graphicsWidget, &SkeletonGraphicsWidget::nodeBoneMarkChanged);
    connect(m_document, &SkeletonDocument::nodeOriginChanged, graphicsWidget, &SkeletonGraphicsWidget::nodeOriginChanged);
    connect(m_document, &SkeletonDocument::partVisibleStateChanged, graphicsWidget, &SkeletonGraphicsWidget::partVisibleStateChanged);
    connect(m_document, &SkeletonDocument::partDisableStateChanged, graphicsWidget, &SkeletonGraphicsWidget::partVisibleStateChanged);
    connect(m_document, &SkeletonDocument::cleanup, graphicsWidget, &SkeletonGraphicsWidget::removeAllContent);
    connect(m_document, &SkeletonDocument::originChanged, graphicsWidget, &SkeletonGraphicsWidget::originChanged);
    connect(m_document, &SkeletonDocument::checkPart, graphicsWidget, &SkeletonGraphicsWidget::selectPartAllById);
    connect(m_document, &SkeletonDocument::enableBackgroundBlur, graphicsWidget, &SkeletonGraphicsWidget::enableBackgroundBlur);
    connect(m_document, &SkeletonDocument::disableBackgroundBlur, graphicsWidget, &SkeletonGraphicsWidget::disableBackgroundBlur);
    connect(m_document, &SkeletonDocument::uncheckAll, graphicsWidget, &SkeletonGraphicsWidget::unselectAll);
    connect(m_document, &SkeletonDocument::checkNode, graphicsWidget, &SkeletonGraphicsWidget::addSelectNode);
    connect(m_document, &SkeletonDocument::checkEdge, graphicsWidget, &SkeletonGraphicsWidget::addSelectEdge);
    
    connect(partTreeWidget, &SkeletonPartTreeWidget::currentComponentChanged, m_document, &SkeletonDocument::setCurrentCanvasComponentId);
    connect(partTreeWidget, &SkeletonPartTreeWidget::moveComponentUp, m_document, &SkeletonDocument::moveComponentUp);
    connect(partTreeWidget, &SkeletonPartTreeWidget::moveComponentDown, m_document, &SkeletonDocument::moveComponentDown);
    connect(partTreeWidget, &SkeletonPartTreeWidget::moveComponentToTop, m_document, &SkeletonDocument::moveComponentToTop);
    connect(partTreeWidget, &SkeletonPartTreeWidget::moveComponentToBottom, m_document, &SkeletonDocument::moveComponentToBottom);
    connect(partTreeWidget, &SkeletonPartTreeWidget::checkPart, m_document, &SkeletonDocument::checkPart);
    connect(partTreeWidget, &SkeletonPartTreeWidget::createNewComponentAndMoveThisIn, m_document, &SkeletonDocument::createNewComponentAndMoveThisIn);
    connect(partTreeWidget, &SkeletonPartTreeWidget::createNewChildComponent, m_document, &SkeletonDocument::createNewChildComponent);
    connect(partTreeWidget, &SkeletonPartTreeWidget::renameComponent, m_document, &SkeletonDocument::renameComponent);
    connect(partTreeWidget, &SkeletonPartTreeWidget::setComponentExpandState, m_document, &SkeletonDocument::setComponentExpandState);
    connect(partTreeWidget, &SkeletonPartTreeWidget::setComponentSmoothAll, m_document, &SkeletonDocument::setComponentSmoothAll);
    connect(partTreeWidget, &SkeletonPartTreeWidget::setComponentSmoothSeam, m_document, &SkeletonDocument::setComponentSmoothSeam);
    connect(partTreeWidget, &SkeletonPartTreeWidget::moveComponent, m_document, &SkeletonDocument::moveComponent);
    connect(partTreeWidget, &SkeletonPartTreeWidget::removeComponent, m_document, &SkeletonDocument::removeComponent);
    connect(partTreeWidget, &SkeletonPartTreeWidget::hideOtherComponents, m_document, &SkeletonDocument::hideOtherComponents);
    connect(partTreeWidget, &SkeletonPartTreeWidget::lockOtherComponents, m_document, &SkeletonDocument::lockOtherComponents);
    connect(partTreeWidget, &SkeletonPartTreeWidget::hideAllComponents, m_document, &SkeletonDocument::hideAllComponents);
    connect(partTreeWidget, &SkeletonPartTreeWidget::showAllComponents, m_document, &SkeletonDocument::showAllComponents);
    connect(partTreeWidget, &SkeletonPartTreeWidget::collapseAllComponents, m_document, &SkeletonDocument::collapseAllComponents);
    connect(partTreeWidget, &SkeletonPartTreeWidget::expandAllComponents, m_document, &SkeletonDocument::expandAllComponents);
    connect(partTreeWidget, &SkeletonPartTreeWidget::lockAllComponents, m_document, &SkeletonDocument::lockAllComponents);
    connect(partTreeWidget, &SkeletonPartTreeWidget::unlockAllComponents, m_document, &SkeletonDocument::unlockAllComponents);
    connect(partTreeWidget, &SkeletonPartTreeWidget::setPartLockState, m_document, &SkeletonDocument::setPartLockState);
    connect(partTreeWidget, &SkeletonPartTreeWidget::setPartVisibleState, m_document, &SkeletonDocument::setPartVisibleState);
    connect(partTreeWidget, &SkeletonPartTreeWidget::setComponentInverseState, m_document, &SkeletonDocument::setComponentInverseState);
    connect(partTreeWidget, &SkeletonPartTreeWidget::hideDescendantComponents, m_document, &SkeletonDocument::hideDescendantComponents);
    connect(partTreeWidget, &SkeletonPartTreeWidget::showDescendantComponents, m_document, &SkeletonDocument::showDescendantComponents);
    connect(partTreeWidget, &SkeletonPartTreeWidget::lockDescendantComponents, m_document, &SkeletonDocument::lockDescendantComponents);
    connect(partTreeWidget, &SkeletonPartTreeWidget::unlockDescendantComponents, m_document, &SkeletonDocument::unlockDescendantComponents);
    
    connect(partTreeWidget, &SkeletonPartTreeWidget::addPartToSelection, graphicsWidget, &SkeletonGraphicsWidget::addPartToSelection);
    
    connect(m_document, &SkeletonDocument::componentNameChanged, partTreeWidget, &SkeletonPartTreeWidget::componentNameChanged);
    connect(m_document, &SkeletonDocument::componentChildrenChanged, partTreeWidget, &SkeletonPartTreeWidget::componentChildrenChanged);
    connect(m_document, &SkeletonDocument::componentRemoved, partTreeWidget, &SkeletonPartTreeWidget::componentRemoved);
    connect(m_document, &SkeletonDocument::componentAdded, partTreeWidget, &SkeletonPartTreeWidget::componentAdded);
    connect(m_document, &SkeletonDocument::componentExpandStateChanged, partTreeWidget, &SkeletonPartTreeWidget::componentExpandStateChanged);
    connect(m_document, &SkeletonDocument::partPreviewChanged, partTreeWidget, &SkeletonPartTreeWidget::partPreviewChanged);
    connect(m_document, &SkeletonDocument::partLockStateChanged, partTreeWidget, &SkeletonPartTreeWidget::partLockStateChanged);
    connect(m_document, &SkeletonDocument::partVisibleStateChanged, partTreeWidget, &SkeletonPartTreeWidget::partVisibleStateChanged);
    connect(m_document, &SkeletonDocument::partSubdivStateChanged, partTreeWidget, &SkeletonPartTreeWidget::partSubdivStateChanged);
    connect(m_document, &SkeletonDocument::partDisableStateChanged, partTreeWidget, &SkeletonPartTreeWidget::partDisableStateChanged);
    connect(m_document, &SkeletonDocument::partXmirrorStateChanged, partTreeWidget, &SkeletonPartTreeWidget::partXmirrorStateChanged);
    connect(m_document, &SkeletonDocument::partDeformThicknessChanged, partTreeWidget, &SkeletonPartTreeWidget::partDeformChanged);
    connect(m_document, &SkeletonDocument::partDeformWidthChanged, partTreeWidget, &SkeletonPartTreeWidget::partDeformChanged);
    connect(m_document, &SkeletonDocument::partRoundStateChanged, partTreeWidget, &SkeletonPartTreeWidget::partRoundStateChanged);
    connect(m_document, &SkeletonDocument::partWrapStateChanged, partTreeWidget, &SkeletonPartTreeWidget::partWrapStateChanged);
    connect(m_document, &SkeletonDocument::partColorStateChanged, partTreeWidget, &SkeletonPartTreeWidget::partColorStateChanged);
    connect(m_document, &SkeletonDocument::partRemoved, partTreeWidget, &SkeletonPartTreeWidget::partRemoved);
    connect(m_document, &SkeletonDocument::cleanup, partTreeWidget, &SkeletonPartTreeWidget::removeAllContent);
    connect(m_document, &SkeletonDocument::partChecked, partTreeWidget, &SkeletonPartTreeWidget::partChecked);
    connect(m_document, &SkeletonDocument::partUnchecked, partTreeWidget, &SkeletonPartTreeWidget::partUnchecked);

    connect(m_document, &SkeletonDocument::skeletonChanged, m_document, &SkeletonDocument::generateMesh);
    connect(m_document, &SkeletonDocument::resultMeshChanged, [=]() {
        if ((m_exportPreviewWidget && m_exportPreviewWidget->isVisible())) {
            m_document->postProcess();
        }
    });
    connect(m_document, &SkeletonDocument::resultMeshChanged, m_document, &SkeletonDocument::generateRig);
    connect(m_document, &SkeletonDocument::rigChanged, m_document, &SkeletonDocument::generateRig);
    connect(m_document, &SkeletonDocument::postProcessedResultChanged, m_document, &SkeletonDocument::generateTexture);
    connect(m_document, &SkeletonDocument::resultTextureChanged, m_document, &SkeletonDocument::bakeAmbientOcclusionTexture);

    connect(m_document, &SkeletonDocument::resultMeshChanged, [=]() {
        m_modelRenderWidget->updateMesh(m_document->takeResultMesh());
    });
    //connect(m_document, &SkeletonDocument::resultSkeletonChanged, [=]() {
    //    m_skeletonRenderWidget->updateMesh(m_document->takeResultSkeletonMesh());
    //});

    connect(graphicsWidget, &SkeletonGraphicsWidget::cursorChanged, [=]() {
        m_modelRenderWidget->setCursor(graphicsWidget->cursor());
        //m_skeletonRenderWidget->setCursor(graphicsWidget->cursor());
    });

    connect(m_document, &SkeletonDocument::skeletonChanged, this, &SkeletonDocumentWindow::documentChanged);
    connect(m_document, &SkeletonDocument::turnaroundChanged, this, &SkeletonDocumentWindow::documentChanged);
    connect(m_document, &SkeletonDocument::optionsChanged, this, &SkeletonDocumentWindow::documentChanged);
    connect(m_document, &SkeletonDocument::rigChanged, this, &SkeletonDocumentWindow::documentChanged);

    connect(m_modelRenderWidget, &ModelWidget::customContextMenuRequested, [=](const QPoint &pos) {
        graphicsWidget->showContextMenu(graphicsWidget->mapFromGlobal(m_modelRenderWidget->mapToGlobal(pos)));
    });

    connect(m_document, &SkeletonDocument::xlockStateChanged, this, &SkeletonDocumentWindow::updateXlockButtonState);
    connect(m_document, &SkeletonDocument::ylockStateChanged, this, &SkeletonDocumentWindow::updateYlockButtonState);
    connect(m_document, &SkeletonDocument::zlockStateChanged, this, &SkeletonDocumentWindow::updateZlockButtonState);
    connect(m_document, &SkeletonDocument::radiusLockStateChanged, this, &SkeletonDocumentWindow::updateRadiusLockButtonState);
    
    connect(m_rigWidget, &RigWidget::setRigType, m_document, &SkeletonDocument::setRigType);
    
    connect(m_document, &SkeletonDocument::rigTypeChanged, m_rigWidget, &RigWidget::rigTypeChanged);
    connect(m_document, &SkeletonDocument::resultRigChanged, m_rigWidget, &RigWidget::updateResultInfo);
    connect(m_document, &SkeletonDocument::resultRigChanged, this, &SkeletonDocumentWindow::updateRigWeightRenderWidget);
    
    //connect(m_document, &SkeletonDocument::resultRigChanged, tetrapodPoseEditWidget, &TetrapodPoseEditWidget::updatePreview);
    
    connect(m_document, &SkeletonDocument::poseAdded, this, [=](QUuid poseId) {
        Q_UNUSED(poseId);
        m_document->generatePosePreviews();
    });
    connect(m_document, &SkeletonDocument::poseParametersChanged, this, [=](QUuid poseId) {
        Q_UNUSED(poseId);
        m_document->generatePosePreviews();
    });
    connect(m_document, &SkeletonDocument::resultRigChanged, m_document, &SkeletonDocument::generatePosePreviews);

    connect(this, &SkeletonDocumentWindow::initialized, m_document, &SkeletonDocument::uiReady);
    
    QTimer *timer = new QTimer(this);
    timer->setInterval(250);
    connect(timer, &QTimer::timeout, [=] {
        QWidget *focusedWidget = QApplication::focusWidget();
        //qDebug() << (focusedWidget ? ("Focused on:" + QString(focusedWidget->metaObject()->className()) + " title:" + focusedWidget->windowTitle()) : "No Focus") << " isActive:" << isActiveWindow();
        if (nullptr == focusedWidget && isActiveWindow())
            graphicsWidget->setFocus();
    });
    timer->start();
}

SkeletonDocumentWindow *SkeletonDocumentWindow::createDocumentWindow()
{
    SkeletonDocumentWindow *documentWindow = new SkeletonDocumentWindow();
    documentWindow->setAttribute(Qt::WA_DeleteOnClose);
    documentWindow->showMaximized();
    return documentWindow;
}

void SkeletonDocumentWindow::closeEvent(QCloseEvent *event)
{
    if (m_documentSaved) {
        event->accept();
        return;
    }

    QMessageBox::StandardButton answer = QMessageBox::question(this,
        APP_NAME,
        tr("Do you really want to close while there are unsaved changes?"),
        QMessageBox::Yes | QMessageBox::No);
    if (answer == QMessageBox::Yes)
        event->accept();
    else
        event->ignore();
}

void SkeletonDocumentWindow::setCurrentFilename(const QString &filename)
{
    m_currentFilename = filename;
    m_documentSaved = true;
    updateTitle();
}

void SkeletonDocumentWindow::updateTitle()
{
    QString appName = APP_NAME;
    QString appVer = APP_HUMAN_VER;
    setWindowTitle(QString("%1 %2 %3%4").arg(appName).arg(appVer).arg(m_currentFilename).arg(m_documentSaved ? "" : "*"));
}

void SkeletonDocumentWindow::documentChanged()
{
    if (m_documentSaved) {
        m_documentSaved = false;
        updateTitle();
    }
}

void SkeletonDocumentWindow::newWindow()
{
    SkeletonDocumentWindow::createDocumentWindow();
}

void SkeletonDocumentWindow::newDocument()
{
    if (!m_documentSaved) {
        QMessageBox::StandardButton answer = QMessageBox::question(this,
            APP_NAME,
            tr("Do you really want to create new document and lose the unsaved changes?"),
            QMessageBox::Yes | QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    m_document->reset();
}

void SkeletonDocumentWindow::saveAs()
{
    QString filename = QFileDialog::getSaveFileName(this, QString(), QString(),
       tr("Dust3D Document (*.ds3)"));
    if (filename.isEmpty()) {
        return;
    }
    saveTo(filename);
}

void SkeletonDocumentWindow::saveAll()
{
    for (auto &window: g_documentWindows) {
        window->save();
    }
}

void SkeletonDocumentWindow::viewSource()
{
    QString url = APP_REPOSITORY_URL;
    qDebug() << "viewSource:" << url;
    QDesktopServices::openUrl(QUrl(url));
}

void SkeletonDocumentWindow::about()
{
    SkeletonDocumentWindow::showAbout();
}

void SkeletonDocumentWindow::reportIssues()
{
    QString url = APP_ISSUES_URL;
    qDebug() << "reportIssues:" << url;
    QDesktopServices::openUrl(QUrl(url));
}

void SkeletonDocumentWindow::seeReferenceGuide()
{
    QString url = APP_REFERENCE_GUIDE_URL;
    qDebug() << "referenceGuide:" << url;
    QDesktopServices::openUrl(QUrl(url));
}

void SkeletonDocumentWindow::seeAcknowlegements()
{
    SkeletonDocumentWindow::showAcknowlegements();
}

void SkeletonDocumentWindow::seeContributors()
{
    SkeletonDocumentWindow::showContributors();
}

void SkeletonDocumentWindow::initLockButton(QPushButton *button)
{
    QFont font;
    font.setWeight(QFont::Light);
    font.setPixelSize(Theme::toolIconFontSize);
    font.setBold(false);

    button->setFont(font);
    button->setFixedSize(Theme::toolIconSize, Theme::toolIconSize);
    button->setStyleSheet("QPushButton {color: #f7d9c8}");
    button->setFocusPolicy(Qt::NoFocus);
}

SkeletonDocumentWindow::~SkeletonDocumentWindow()
{
    g_documentWindows.erase(this);
}

void SkeletonDocumentWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_firstShow) {
        m_firstShow = false;
        updateTitle();
        m_document->saveSnapshot();
        m_graphicsWidget->setFocus();
        emit initialized();
    }
}

void SkeletonDocumentWindow::mousePressEvent(QMouseEvent *event)
{
    QMainWindow::mousePressEvent(event);
}

void SkeletonDocumentWindow::changeTurnaround()
{
    QString fileName = QFileDialog::getOpenFileName(this, QString(), QString(),
        tr("Image Files (*.png *.jpg *.bmp)")).trimmed();
    if (fileName.isEmpty())
        return;
    QImage image;
    if (!image.load(fileName))
        return;
    m_document->updateTurnaround(image);
}

void SkeletonDocumentWindow::save()
{
    saveTo(m_currentFilename);
}

void SkeletonDocumentWindow::saveTo(const QString &saveAsFilename)
{
    QString filename = saveAsFilename;

    if (filename.isEmpty()) {
        filename = QFileDialog::getSaveFileName(this, QString(), QString(),
           tr("Dust3D Document (*.ds3)"));
        if (filename.isEmpty()) {
            return;
        }
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);

    Ds3FileWriter ds3Writer;

    QByteArray modelXml;
    QXmlStreamWriter stream(&modelXml);
    SkeletonSnapshot snapshot;
    m_document->toSnapshot(&snapshot);
    saveSkeletonToXmlStream(&snapshot, &stream);
    if (modelXml.size() > 0)
        ds3Writer.add("model.xml", "model", &modelXml);

    QByteArray imageByteArray;
    QBuffer pngBuffer(&imageByteArray);
    if (!m_document->turnaround.isNull()) {
        pngBuffer.open(QIODevice::WriteOnly);
        m_document->turnaround.save(&pngBuffer, "PNG");
        if (imageByteArray.size() > 0)
            ds3Writer.add("canvas.png", "asset", &imageByteArray);
    }

    if (ds3Writer.save(filename)) {
        setCurrentFilename(filename);
    }

    QApplication::restoreOverrideCursor();
}

void SkeletonDocumentWindow::open()
{
    if (!m_documentSaved) {
        QMessageBox::StandardButton answer = QMessageBox::question(this,
            APP_NAME,
            tr("Do you really want to open another file and lose the unsaved changes?"),
            QMessageBox::Yes | QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    QString filename = QFileDialog::getOpenFileName(this, QString(), QString(),
        tr("Dust3D Document (*.ds3)"));
    if (filename.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    Ds3FileReader ds3Reader(filename);
    for (int i = 0; i < ds3Reader.items().size(); ++i) {
        Ds3ReaderItem item = ds3Reader.items().at(i);
        if (item.type == "model") {
            QByteArray data;
            ds3Reader.loadItem(item.name, &data);
            QXmlStreamReader stream(data);
            SkeletonSnapshot snapshot;
            loadSkeletonFromXmlStream(&snapshot, stream);
            m_document->fromSnapshot(snapshot);
            m_document->saveSnapshot();
        } else if (item.type == "asset") {
            if (item.name == "canvas.png") {
                QByteArray data;
                ds3Reader.loadItem(item.name, &data);
                QImage image = QImage::fromData(data, "PNG");
                m_document->updateTurnaround(image);
            }
        }
    }
    QApplication::restoreOverrideCursor();

    setCurrentFilename(filename);
}

void SkeletonDocumentWindow::showAdvanceSetting()
{
    if (nullptr == m_advanceSettingWidget) {
        m_advanceSettingWidget = new AdvanceSettingWidget(m_document, this);
    }
    m_advanceSettingWidget->show();
    m_advanceSettingWidget->raise();
}

void SkeletonDocumentWindow::exportObjResult()
{
    QString filename = QFileDialog::getSaveFileName(this, QString(), QString(),
       tr("Wavefront (*.obj)"));
    if (filename.isEmpty()) {
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_modelRenderWidget->exportMeshAsObj(filename);
    QApplication::restoreOverrideCursor();
}

void SkeletonDocumentWindow::exportObjPlusMaterialsResult()
{
    QString filename = QFileDialog::getSaveFileName(this, QString(), QString(),
       tr("Wavefront (*.obj)"));
    if (filename.isEmpty()) {
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_modelRenderWidget->exportMeshAsObjPlusMaterials(filename);
    QApplication::restoreOverrideCursor();
}

void SkeletonDocumentWindow::showExportPreview()
{
    if (nullptr == m_exportPreviewWidget) {
        m_exportPreviewWidget = new ExportPreviewWidget(m_document, this);
        connect(m_exportPreviewWidget, &ExportPreviewWidget::regenerate, m_document, &SkeletonDocument::regenerateMesh);
        connect(m_exportPreviewWidget, &ExportPreviewWidget::save, this, &SkeletonDocumentWindow::exportGltfResult);
        connect(m_document, &SkeletonDocument::resultMeshChanged, m_exportPreviewWidget, &ExportPreviewWidget::checkSpinner);
        connect(m_document, &SkeletonDocument::exportReady, m_exportPreviewWidget, &ExportPreviewWidget::checkSpinner);
        connect(m_document, &SkeletonDocument::resultTextureChanged, m_exportPreviewWidget, &ExportPreviewWidget::updateTexturePreview);
        connect(m_document, &SkeletonDocument::resultBakedTextureChanged, m_exportPreviewWidget, &ExportPreviewWidget::updateTexturePreview);
        registerDialog(m_exportPreviewWidget);
    }
    m_exportPreviewWidget->show();
    m_exportPreviewWidget->raise();
}

void SkeletonDocumentWindow::exportGltfResult()
{
    QString filename = QFileDialog::getSaveFileName(this, QString(), QString(),
       tr("GL Transmission Format (.gltf)"));
    if (filename.isEmpty()) {
        return;
    }
    if (!m_document->isExportReady()) {
        qDebug() << "Export but document is not export ready";
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    MeshResultContext skeletonResult = m_document->currentPostProcessedResultContext();
    GltfFileWriter gltfFileWriter(skeletonResult, m_document->resultRigBones(), m_document->resultRigWeights(), filename);
    gltfFileWriter.save();
    if (m_document->textureImage)
        m_document->textureImage->save(gltfFileWriter.textureFilenameInGltf());
    QFileInfo nameInfo(filename);
    QString borderFilename = nameInfo.path() + QDir::separator() + nameInfo.completeBaseName() + "-BORDER.png";
    if (m_document->textureBorderImage)
        m_document->textureBorderImage->save(borderFilename);
    QString ambientOcclusionFilename = nameInfo.path() + QDir::separator() + nameInfo.completeBaseName() + "-AMBIENT-OCCLUSION.png";
    if (m_document->textureAmbientOcclusionImage)
        m_document->textureAmbientOcclusionImage->save(ambientOcclusionFilename);
    QString colorFilename = nameInfo.path() + QDir::separator() + nameInfo.completeBaseName() + "-COLOR.png";
    if (m_document->textureColorImage)
        m_document->textureColorImage->save(colorFilename);
    QApplication::restoreOverrideCursor();
}

void SkeletonDocumentWindow::updateXlockButtonState()
{
    if (m_document->xlocked)
        m_xlockButton->setStyleSheet("QPushButton {color: #252525}");
    else
        m_xlockButton->setStyleSheet("QPushButton {color: #fc6621}");
}

void SkeletonDocumentWindow::updateYlockButtonState()
{
    if (m_document->ylocked)
        m_ylockButton->setStyleSheet("QPushButton {color: #252525}");
    else
        m_ylockButton->setStyleSheet("QPushButton {color: #2a5aac}");
}

void SkeletonDocumentWindow::updateZlockButtonState()
{
    if (m_document->zlocked)
        m_zlockButton->setStyleSheet("QPushButton {color: #252525}");
    else
        m_zlockButton->setStyleSheet("QPushButton {color: #aaebc4}");
}

void SkeletonDocumentWindow::updateRadiusLockButtonState()
{
    if (m_document->radiusLocked)
        m_radiusLockButton->setStyleSheet("QPushButton {color: #252525}");
    else
        m_radiusLockButton->setStyleSheet("QPushButton {color: " + Theme::white.name() + "}");
}

void SkeletonDocumentWindow::updateRigWeightRenderWidget()
{
    MeshLoader *resultRigWeightMesh = m_document->takeResultRigWeightMesh();
    if (nullptr == resultRigWeightMesh) {
        m_rigWidget->rigWeightRenderWidget()->hide();
    } else {
        m_rigWidget->rigWeightRenderWidget()->updateMesh(resultRigWeightMesh);
        m_rigWidget->rigWeightRenderWidget()->show();
        m_rigWidget->rigWeightRenderWidget()->update();
    }
}

void SkeletonDocumentWindow::registerDialog(QWidget *widget)
{
    m_dialogs.push_back(widget);
}

void SkeletonDocumentWindow::unregisterDialog(QWidget *widget)
{
    m_dialogs.erase(std::remove(m_dialogs.begin(), m_dialogs.end(), widget), m_dialogs.end());
}

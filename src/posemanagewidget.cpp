#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include "posemanagewidget.h"
#include "theme.h"
#include "poseeditwidget.h"

PoseManageWidget::PoseManageWidget(const SkeletonDocument *document, QWidget *parent) :
    QWidget(parent),
    m_document(document)
{
    QPushButton *addPoseButton = new QPushButton(Theme::awesome()->icon(fa::plus), tr("Add Pose..."));
    
    connect(addPoseButton, &QPushButton::clicked, this, &PoseManageWidget::showAddPoseDialog);
    
    QHBoxLayout *toolsLayout = new QHBoxLayout;
    toolsLayout->addWidget(addPoseButton);
    
    m_poseListWidget = new PoseListWidget(document);
    connect(m_poseListWidget, &PoseListWidget::modifyPose, this, &PoseManageWidget::showPoseDialog);
    
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(toolsLayout);
    mainLayout->addWidget(m_poseListWidget);
    
    setLayout(mainLayout);
}

PoseListWidget *PoseManageWidget::poseListWidget()
{
    return m_poseListWidget;
}

QSize PoseManageWidget::sizeHint() const
{
    return QSize(Theme::sidebarPreferredWidth, 0);
}

void PoseManageWidget::showAddPoseDialog()
{
    showPoseDialog(QUuid());
}

void PoseManageWidget::showPoseDialog(QUuid poseId)
{
    PoseEditWidget *poseEditWidget = new PoseEditWidget(m_document);
    poseEditWidget->setAttribute(Qt::WA_DeleteOnClose);
    if (!poseId.isNull()) {
        const SkeletonPose *pose = m_document->findPose(poseId);
        if (nullptr != pose) {
            poseEditWidget->setEditPoseId(poseId);
            poseEditWidget->setEditPoseName(pose->name);
            poseEditWidget->setEditParameters(pose->parameters);
            poseEditWidget->clearUnsaveState();
        }
    }
    poseEditWidget->show();
    connect(poseEditWidget, &QDialog::destroyed, [=]() {
        emit unregisterDialog((QWidget *)poseEditWidget);
    });
    emit registerDialog((QWidget *)poseEditWidget);
}

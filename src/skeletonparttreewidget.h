#ifndef SKELETON_PART_TREE_WIDGET_H
#define SKELETON_PART_TREE_WIDGET_H
#include <QTreeWidget>
#include <QUuid>
#include <QMouseEvent>
#include "skeletondocument.h"
#include "skeletongraphicswidget.h"

class SkeletonPartTreeWidget : public QTreeWidget, public SkeletonGraphicsFunctions
{
    Q_OBJECT
signals:
    void currentComponentChanged(QUuid componentId);
    void moveComponentUp(QUuid componentId);
    void moveComponentDown(QUuid componentId);
    void moveComponentToTop(QUuid componentId);
    void moveComponentToBottom(QUuid componentId);
    void checkPart(QUuid partId);
    void createNewComponentAndMoveThisIn(QUuid componentId);
    void createNewChildComponent(QUuid parentComponentId);
    void renameComponent(QUuid componentId, QString name);
    void setComponentExpandState(QUuid componentId, bool expanded);
    void setComponentSmoothAll(QUuid componentId, float toSmoothAll);
    void setComponentSmoothSeam(QUuid componentId, float toSmoothSeam);
    void moveComponent(QUuid componentId, QUuid toParentId);
    void removeComponent(QUuid componentId);
    void hideOtherComponents(QUuid componentId);
    void lockOtherComponents(QUuid componentId);
    void hideAllComponents();
    void showAllComponents();
    void collapseAllComponents();
    void expandAllComponents();
    void lockAllComponents();
    void unlockAllComponents();
    void setPartLockState(QUuid partId, bool locked);
    void setPartVisibleState(QUuid partId, bool visible);
    void setComponentInverseState(QUuid componentId, bool inverse);
    void hideDescendantComponents(QUuid componentId);
    void showDescendantComponents(QUuid componentId);
    void lockDescendantComponents(QUuid componentId);
    void unlockDescendantComponents(QUuid componentId);
    void addPartToSelection(QUuid partId);
    void groupOperationAdded();
public:
    SkeletonPartTreeWidget(const SkeletonDocument *document, QWidget *parent);
    QTreeWidgetItem *findComponentItem(QUuid componentId);
    void setGraphicsFunctions(SkeletonGraphicsFunctions *graphicsFunctions);
public slots:
    void componentNameChanged(QUuid componentId);
    void componentChildrenChanged(QUuid componentId);
    void componentRemoved(QUuid componentId);
    void componentAdded(QUuid componentId);
    void componentExpandStateChanged(QUuid componentId);
    void partRemoved(QUuid partId);
    void partPreviewChanged(QUuid partid);
    void partLockStateChanged(QUuid partId);
    void partVisibleStateChanged(QUuid partId);
    void partSubdivStateChanged(QUuid partId);
    void partDisableStateChanged(QUuid partId);
    void partXmirrorStateChanged(QUuid partId);
    void partDeformChanged(QUuid partId);
    void partRoundStateChanged(QUuid partId);
    void partWrapStateChanged(QUuid partId);
    void partColorStateChanged(QUuid partId);
    void partChecked(QUuid partId);
    void partUnchecked(QUuid partId);
    void groupChanged(QTreeWidgetItem *item, int column);
    void groupExpanded(QTreeWidgetItem *item);
    void groupCollapsed(QTreeWidgetItem *item);
    void removeAllContent();
    void showContextMenu(const QPoint &pos);
protected:
    virtual QSize sizeHint() const;
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    bool mouseMove(QMouseEvent *event);
    bool wheel(QWheelEvent *event);
    bool mouseRelease(QMouseEvent *event);
    bool mousePress(QMouseEvent *event);
    bool mouseDoubleClick(QMouseEvent *event);
    bool keyPress(QKeyEvent *event);
private:
    void addComponentChildrenToItem(QUuid componentId, QTreeWidgetItem *parentItem);
    void deleteItemChildren(QTreeWidgetItem *item);
    void selectComponent(QUuid componentId, bool multiple=false);
    QWidget *createSmoothMenuWidget(QUuid componentId);
    void updateComponentSelectState(QUuid componentId, bool selected);
    bool isComponentSelected(QUuid componentId);
private:
    const SkeletonDocument *m_document = nullptr;
    std::map<QUuid, QTreeWidgetItem *> m_partItemMap;
    std::map<QUuid, QTreeWidgetItem *> m_componentItemMap;
    SkeletonGraphicsFunctions *m_graphicsFunctions = nullptr;
    QFont m_normalFont;
    QFont m_selectedFont;
    QUuid m_currentSelectedComponentId;
    QBrush m_hightlightedPartBackground;
    QUuid m_shiftStartComponentId;
    std::set<QUuid> m_selectedComponentIds;
};

#endif

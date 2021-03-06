#ifndef MESH_GENERATOR_H
#define MESH_GENERATOR_H
#include <QObject>
#include <QString>
#include <QImage>
#include <map>
#include <set>
#include <QThread>
#include <QOpenGLWidget>
#include "skeletonsnapshot.h"
#include "meshloader.h"
#include "meshresultcontext.h"
#include "positionmap.h"

class GeneratedCacheContext
{
public:
    ~GeneratedCacheContext();
    std::map<QString, std::vector<BmeshVertex>> partBmeshVertices;
    std::map<QString, std::vector<BmeshNode>> partBmeshNodes;
    std::map<QString, std::vector<std::tuple<PositionMapKey, PositionMapKey, PositionMapKey, PositionMapKey>>> partBmeshQuads;
    std::map<QString, void *> componentCombinableMeshs;
    std::map<QString, std::vector<QVector3D>> componentPositions;
    std::map<QString, PositionMap<BmeshVertex>> componentVerticesSources;
    std::map<QString, QString> partMirrorIdMap;
    void updateComponentCombinableMesh(QString componentId, void *mesh);
};

class MeshGenerator : public QObject
{
    Q_OBJECT
public:
    MeshGenerator(SkeletonSnapshot *snapshot, QThread *thread);
    ~MeshGenerator();
    void setSharedContextWidget(QOpenGLWidget *widget);
    void addPartPreviewRequirement(const QUuid &partId);
    void setGeneratedCacheContext(GeneratedCacheContext *cacheContext);
    void setSmoothNormal(bool smoothNormal);
    void setWeldEnabled(bool weldEnabled);
    MeshLoader *takeResultMesh();
    MeshLoader *takePartPreviewMesh(const QUuid &partId);
    const std::set<QUuid> &requirePreviewPartIds();
    const std::set<QUuid> &generatedPreviewPartIds();
    MeshResultContext *takeMeshResultContext();
signals:
    void finished();
public slots:
    void process();
private:
    SkeletonSnapshot *m_snapshot;
    MeshLoader *m_mesh;
    std::map<QUuid, MeshLoader *> m_partPreviewMeshMap;
    std::set<QUuid> m_requirePreviewPartIds;
    std::set<QUuid> m_generatedPreviewPartIds;
    QThread *m_thread;
    MeshResultContext *m_meshResultContext;
    QOpenGLWidget *m_sharedContextWidget;
    void *m_meshliteContext;
    GeneratedCacheContext *m_cacheContext;
    bool m_smoothNormal;
    bool m_weldEnabled;
    float m_mainProfileMiddleX;
    float m_sideProfileMiddleX;
    float m_mainProfileMiddleY;
    std::map<QString, std::set<QString>> m_partNodeIds;
    std::map<QString, std::set<QString>> m_partEdgeIds;
    std::set<QString> m_dirtyComponentIds;
    std::set<QString> m_dirtyPartIds;
private:
    static bool m_enableDebug;
    static PositionMap<int> *m_forMakePositionKey;
private:
    void loadVertexSources(void *meshliteContext, int meshId, QUuid partId, const std::map<int, QUuid> &bmeshToNodeIdMap, std::vector<BmeshVertex> &bmeshVertices,
        std::vector<std::tuple<PositionMapKey, PositionMapKey, PositionMapKey, PositionMapKey>> &bmeshQuads);
    void loadGeneratedPositionsToMeshResultContext(void *meshliteContext, int triangulatedMeshId);
    void collectParts();
    void checkDirtyFlags();
    bool checkIsComponentDirty(QString componentId);
    bool checkIsPartDirty(QString partId);
    void *combineComponentMesh(QString componentId, bool *inverse);
    void *combinePartMesh(QString partId);
};

#endif

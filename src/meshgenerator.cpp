#include <vector>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <unordered_set>
#include "meshgenerator.h"
#include "dust3dutil.h"
#include "skeletondocument.h"
#include "meshlite.h"
#include "meshutil.h"
#include "theme.h"
#include "positionmap.h"
#include "meshquadify.h"
#include "meshweldseam.h"

bool MeshGenerator::m_enableDebug = false;
PositionMap<int> *MeshGenerator::m_forMakePositionKey = new PositionMap<int>;

GeneratedCacheContext::~GeneratedCacheContext()
{
    for (auto &cache: componentCombinableMeshs) {
        deleteCombinableMesh(cache.second);
    }
}

void GeneratedCacheContext::updateComponentCombinableMesh(QString componentId, void *mesh)
{
    auto &cache = componentCombinableMeshs[componentId];
    if (nullptr != cache)
        deleteCombinableMesh(cache);
    cache = cloneCombinableMesh(mesh);
}

MeshGenerator::MeshGenerator(SkeletonSnapshot *snapshot, QThread *thread) :
    m_snapshot(snapshot),
    m_mesh(nullptr),
    m_thread(thread),
    m_meshResultContext(nullptr),
    m_sharedContextWidget(nullptr),
    m_cacheContext(nullptr),
    m_smoothNormal(true),
    m_weldEnabled(true)
{
}

MeshGenerator::~MeshGenerator()
{
    delete m_snapshot;
    delete m_mesh;
    for (const auto &partPreviewMeshIt: m_partPreviewMeshMap) {
        delete partPreviewMeshIt.second;
    }
    delete m_meshResultContext;
}

void MeshGenerator::setSmoothNormal(bool smoothNormal)
{
    m_smoothNormal = smoothNormal;
}

void MeshGenerator::setWeldEnabled(bool weldEnabled)
{
    m_weldEnabled = weldEnabled;
}

void MeshGenerator::setGeneratedCacheContext(GeneratedCacheContext *cacheContext)
{
    m_cacheContext = cacheContext;
}

void MeshGenerator::addPartPreviewRequirement(const QUuid &partId)
{
    //qDebug() << "addPartPreviewRequirement:" << partId;
    m_requirePreviewPartIds.insert(partId);
}

void MeshGenerator::setSharedContextWidget(QOpenGLWidget *widget)
{
    m_sharedContextWidget = widget;
}

MeshLoader *MeshGenerator::takeResultMesh()
{
    MeshLoader *resultMesh = m_mesh;
    m_mesh = nullptr;
    return resultMesh;
}

MeshLoader *MeshGenerator::takePartPreviewMesh(const QUuid &partId)
{
    MeshLoader *resultMesh = m_partPreviewMeshMap[partId];
    m_partPreviewMeshMap[partId] = nullptr;
    return resultMesh;
}

const std::set<QUuid> &MeshGenerator::requirePreviewPartIds()
{
    return m_requirePreviewPartIds;
}

const std::set<QUuid> &MeshGenerator::generatedPreviewPartIds()
{
    return m_generatedPreviewPartIds;
}

MeshResultContext *MeshGenerator::takeMeshResultContext()
{
    MeshResultContext *meshResultContext = m_meshResultContext;
    m_meshResultContext = nullptr;
    return meshResultContext;
}

void MeshGenerator::loadVertexSources(void *meshliteContext, int meshId, QUuid partId, const std::map<int, QUuid> &bmeshToNodeIdMap, std::vector<BmeshVertex> &bmeshVertices,
    std::vector<std::tuple<PositionMapKey, PositionMapKey, PositionMapKey, PositionMapKey>> &bmeshQuads)
{
    int vertexCount = meshlite_get_vertex_count(meshliteContext, meshId);
    int positionBufferLen = vertexCount * 3;
    float *positionBuffer = new float[positionBufferLen];
    int positionCount = meshlite_get_vertex_position_array(meshliteContext, meshId, positionBuffer, positionBufferLen) / 3;
    int *sourceBuffer = new int[positionBufferLen];
    int sourceCount = meshlite_get_vertex_source_array(meshliteContext, meshId, sourceBuffer, positionBufferLen);
    Q_ASSERT(positionCount == sourceCount);
    std::vector<QVector3D> verticesPositions;
    for (int i = 0, positionIndex = 0; i < positionCount; i++, positionIndex+=3) {
        BmeshVertex vertex;
        vertex.partId = partId;
        auto findNodeId = bmeshToNodeIdMap.find(sourceBuffer[i]);
        if (findNodeId != bmeshToNodeIdMap.end())
            vertex.nodeId = findNodeId->second;
        vertex.position = QVector3D(positionBuffer[positionIndex + 0], positionBuffer[positionIndex + 1], positionBuffer[positionIndex + 2]);
        verticesPositions.push_back(vertex.position);
        bmeshVertices.push_back(vertex);
    }
    int faceCount = meshlite_get_face_count(meshliteContext, meshId);
    int *faceVertexNumAndIndices = new int[faceCount * MAX_VERTICES_PER_FACE];
    int filledLength = meshlite_get_face_index_array(meshliteContext, meshId, faceVertexNumAndIndices, faceCount * MAX_VERTICES_PER_FACE);
    int i = 0;
    while (i < filledLength) {
        int num = faceVertexNumAndIndices[i++];
        Q_ASSERT(num > 0 && num <= MAX_VERTICES_PER_FACE);
        if (4 != num) {
            i += num;
            continue;
        }
        int i0 = faceVertexNumAndIndices[i++];
        int i1 = faceVertexNumAndIndices[i++];
        int i2 = faceVertexNumAndIndices[i++];
        int i3 = faceVertexNumAndIndices[i++];
        const auto &v0 = verticesPositions[i0];
        const auto &v1 = verticesPositions[i1];
        const auto &v2 = verticesPositions[i2];
        const auto &v3 = verticesPositions[i3];
        bmeshQuads.push_back(std::make_tuple(m_forMakePositionKey->makeKey(v0.x(), v0.y(), v0.z()),
            m_forMakePositionKey->makeKey(v1.x(), v1.y(), v1.z()),
            m_forMakePositionKey->makeKey(v2.x(), v2.y(), v2.z()),
            m_forMakePositionKey->makeKey(v3.x(), v3.y(), v3.z())));
    }
    delete[] faceVertexNumAndIndices;
    delete[] positionBuffer;
    delete[] sourceBuffer;
}

void MeshGenerator::loadGeneratedPositionsToMeshResultContext(void *meshliteContext, int triangulatedMeshId)
{
    int vertexCount = meshlite_get_vertex_count(meshliteContext, triangulatedMeshId);
    int positionBufferLen = vertexCount * 3;
    float *positionBuffer = new float[positionBufferLen];
    int positionCount = meshlite_get_vertex_position_array(meshliteContext, triangulatedMeshId, positionBuffer, positionBufferLen) / 3;
    std::map<int, int> verticesMap;
    for (int i = 0, positionIndex = 0; i < positionCount; i++, positionIndex+=3) {
        ResultVertex vertex;
        vertex.position = QVector3D(positionBuffer[positionIndex + 0], positionBuffer[positionIndex + 1], positionBuffer[positionIndex + 2]);
        verticesMap[i] = m_meshResultContext->vertices.size();
        m_meshResultContext->vertices.push_back(vertex);
    }
    int faceCount = meshlite_get_face_count(meshliteContext, triangulatedMeshId);
    int triangleIndexBufferLen = faceCount * 3;
    int *triangleIndexBuffer = new int[triangleIndexBufferLen];
    int triangleCount = meshlite_get_triangle_index_array(meshliteContext, triangulatedMeshId, triangleIndexBuffer, triangleIndexBufferLen) / 3;
    int triangleNormalBufferLen = faceCount * 3;
    float *normalBuffer = new float[triangleNormalBufferLen];
    int normalCount = meshlite_get_triangle_normal_array(meshliteContext, triangulatedMeshId, normalBuffer, triangleNormalBufferLen) / 3;
    Q_ASSERT(triangleCount == normalCount);
    for (int i = 0, triangleVertIndex = 0, normalIndex=0; i < triangleCount; i++, triangleVertIndex+=3, normalIndex += 3) {
        ResultTriangle triangle;
        triangle.indicies[0] = verticesMap[triangleIndexBuffer[triangleVertIndex + 0]];
        triangle.indicies[1] = verticesMap[triangleIndexBuffer[triangleVertIndex + 1]];
        triangle.indicies[2] = verticesMap[triangleIndexBuffer[triangleVertIndex + 2]];
        triangle.normal = QVector3D(normalBuffer[normalIndex + 0], normalBuffer[normalIndex + 1], normalBuffer[normalIndex + 2]);
        m_meshResultContext->triangles.push_back(triangle);
    }
    delete[] positionBuffer;
    delete[] triangleIndexBuffer;
    delete[] normalBuffer;
}

void MeshGenerator::collectParts()
{
    for (const auto &node: m_snapshot->nodes) {
        QString partId = valueOfKeyInMapOrEmpty(node.second, "partId");
        if (partId.isEmpty())
            continue;
        m_partNodeIds[partId].insert(node.first);
    }
    for (const auto &edge: m_snapshot->edges) {
        QString partId = valueOfKeyInMapOrEmpty(edge.second, "partId");
        if (partId.isEmpty())
            continue;
        m_partEdgeIds[partId].insert(edge.first);
    }
}

bool MeshGenerator::checkIsPartDirty(QString partId)
{
    auto findPart = m_snapshot->parts.find(partId);
    if (findPart == m_snapshot->parts.end()) {
        qDebug() << "Find part failed:" << partId;
        return false;
    }
    return isTrueValueString(valueOfKeyInMapOrEmpty(findPart->second, "dirty"));
}

void *MeshGenerator::combinePartMesh(QString partId)
{
    auto findPart = m_snapshot->parts.find(partId);
    if (findPart == m_snapshot->parts.end()) {
        qDebug() << "Find part failed:" << partId;
        return nullptr;
    }
    QUuid partIdNotAsString = QUuid(partId);
    
    auto &part = findPart->second;
    bool isDisabled = isTrueValueString(valueOfKeyInMapOrEmpty(part, "disabled"));
    bool xMirrored = isTrueValueString(valueOfKeyInMapOrEmpty(part, "xMirrored"));
    bool subdived = isTrueValueString(valueOfKeyInMapOrEmpty(part, "subdived"));
    bool wrapped = isTrueValueString(valueOfKeyInMapOrEmpty(part, "wrapped"));
    int bmeshId = meshlite_bmesh_create(m_meshliteContext);
    if (subdived)
        meshlite_bmesh_set_cut_subdiv_count(m_meshliteContext, bmeshId, 1);
    if (isTrueValueString(valueOfKeyInMapOrEmpty(part, "rounded")))
        meshlite_bmesh_set_round_way(m_meshliteContext, bmeshId, 1);

    QString colorString = valueOfKeyInMapOrEmpty(part, "color");
    QColor partColor = colorString.isEmpty() ? Theme::white : QColor(colorString);

    QString thicknessString = valueOfKeyInMapOrEmpty(part, "deformThickness");
    if (!thicknessString.isEmpty())
        meshlite_bmesh_set_deform_thickness(m_meshliteContext, bmeshId, thicknessString.toFloat());
    QString widthString = valueOfKeyInMapOrEmpty(part, "deformWidth");
    if (!widthString.isEmpty())
        meshlite_bmesh_set_deform_width(m_meshliteContext, bmeshId, widthString.toFloat());
    if (MeshGenerator::m_enableDebug)
        meshlite_bmesh_enable_debug(m_meshliteContext, bmeshId, 1);
    
    QString mirroredPartId;
    QUuid mirroredPartIdNotAsString;
    if (xMirrored) {
        mirroredPartIdNotAsString = QUuid().createUuid();
        mirroredPartId = mirroredPartIdNotAsString.toString();
        m_cacheContext->partMirrorIdMap[mirroredPartId] = partId;
    }
    
    std::map<QString, int> nodeToBmeshIdMap;
    std::map<int, QUuid> bmeshToNodeIdMap;
    auto &cacheBmeshNodes = m_cacheContext->partBmeshNodes[partId];
    auto &cacheBmeshVertices = m_cacheContext->partBmeshVertices[partId];
    auto &cacheBmeshQuads = m_cacheContext->partBmeshQuads[partId];
    cacheBmeshNodes.clear();
    cacheBmeshVertices.clear();
    cacheBmeshQuads.clear();
    for (const auto &nodeId: m_partNodeIds[partId]) {
        auto findNode = m_snapshot->nodes.find(nodeId);
        if (findNode == m_snapshot->nodes.end()) {
            qDebug() << "Find node failed:" << nodeId;
            continue;
        }
        auto &node = findNode->second;
        
        float radius = valueOfKeyInMapOrEmpty(node, "radius").toFloat();
        float x = (valueOfKeyInMapOrEmpty(node, "x").toFloat() - m_mainProfileMiddleX);
        float y = (m_mainProfileMiddleY - valueOfKeyInMapOrEmpty(node, "y").toFloat());
        float z = (m_sideProfileMiddleX - valueOfKeyInMapOrEmpty(node, "z").toFloat());
        int bmeshNodeId = meshlite_bmesh_add_node(m_meshliteContext, bmeshId, x, y, z, radius);
        
        SkeletonBoneMark boneMark = SkeletonBoneMarkFromString(valueOfKeyInMapOrEmpty(node, "boneMark").toUtf8().constData());
        
        nodeToBmeshIdMap[nodeId] = bmeshNodeId;
        bmeshToNodeIdMap[bmeshNodeId] = nodeId;
        
        BmeshNode bmeshNode;
        bmeshNode.partId = QUuid(partId);
        bmeshNode.origin = QVector3D(x, y, z);
        bmeshNode.radius = radius;
        bmeshNode.nodeId = QUuid(nodeId);
        bmeshNode.color = partColor;
        bmeshNode.boneMark = boneMark;
        //if (SkeletonBoneMark::None != boneMark)
        //    bmeshNode.color = SkeletonBoneMarkToColor(boneMark);
        cacheBmeshNodes.push_back(bmeshNode);
        if (xMirrored) {
            bmeshNode.partId = mirroredPartId;
            bmeshNode.origin.setX(-x);
            cacheBmeshNodes.push_back(bmeshNode);
        }
    }
    
    for (const auto &edgeId: m_partEdgeIds[partId]) {
        auto findEdge = m_snapshot->edges.find(edgeId);
        if (findEdge == m_snapshot->edges.end()) {
            qDebug() << "Find edge failed:" << edgeId;
            continue;
        }
        auto &edge = findEdge->second;
        
        QString fromNodeId = valueOfKeyInMapOrEmpty(edge, "from");
        QString toNodeId = valueOfKeyInMapOrEmpty(edge, "to");
        
        auto findFromBmeshId = nodeToBmeshIdMap.find(fromNodeId);
        if (findFromBmeshId == nodeToBmeshIdMap.end()) {
            qDebug() << "Find from-node bmesh failed:" << fromNodeId;
            continue;
        }
        
        auto findToBmeshId = nodeToBmeshIdMap.find(toNodeId);
        if (findToBmeshId == nodeToBmeshIdMap.end()) {
            qDebug() << "Find to-node bmesh failed:" << toNodeId;
            continue;
        }
        
        meshlite_bmesh_add_edge(m_meshliteContext, bmeshId, findFromBmeshId->second, findToBmeshId->second);
    }
    
    int meshId = 0;
    void *resultMesh = nullptr;
    if (!bmeshToNodeIdMap.empty()) {
        meshId = meshlite_bmesh_generate_mesh(m_meshliteContext, bmeshId);
        loadVertexSources(m_meshliteContext, meshId, partIdNotAsString, bmeshToNodeIdMap, cacheBmeshVertices, cacheBmeshQuads);
        if (wrapped)
            resultMesh = convertToCombinableConvexHullMesh(m_meshliteContext, meshId);
        else
            resultMesh = convertToCombinableMesh(m_meshliteContext, meshlite_triangulate(m_meshliteContext, meshId));
    }
    
    if (nullptr != resultMesh) {
        if (xMirrored) {
            int xMirroredMeshId = meshlite_mirror_in_x(m_meshliteContext, meshId, 0);
            loadVertexSources(m_meshliteContext, xMirroredMeshId, mirroredPartIdNotAsString, bmeshToNodeIdMap, cacheBmeshVertices, cacheBmeshQuads);
            void *mirroredMesh = nullptr;
            if (wrapped)
                mirroredMesh = convertToCombinableConvexHullMesh(m_meshliteContext, xMirroredMeshId);
            else
                mirroredMesh = convertToCombinableMesh(m_meshliteContext, meshlite_triangulate(m_meshliteContext, xMirroredMeshId));
            if (nullptr != mirroredMesh) {
                void *newResultMesh = unionCombinableMeshs(resultMesh, mirroredMesh);
                deleteCombinableMesh(mirroredMesh);
                if (nullptr != newResultMesh) {
                    deleteCombinableMesh(resultMesh);
                    resultMesh = newResultMesh;
                }
            }
        }
    }
    
    if (m_requirePreviewPartIds.find(partIdNotAsString) != m_requirePreviewPartIds.end()) {
        int trimedMeshId = meshlite_trim(m_meshliteContext, meshId, 1);
        m_partPreviewMeshMap[partIdNotAsString] = new MeshLoader(m_meshliteContext, trimedMeshId, -1, partColor, nullptr, m_smoothNormal);
        m_generatedPreviewPartIds.insert(partIdNotAsString);
    }
    
    if (isDisabled) {
        if (nullptr != resultMesh) {
            deleteCombinableMesh(resultMesh);
            resultMesh = nullptr;
        }
    }
    
    return resultMesh;
}

bool MeshGenerator::checkIsComponentDirty(QString componentId)
{
    bool isDirty = false;
    
    const std::map<QString, QString> *component = &m_snapshot->rootComponent;
    if (componentId != QUuid().toString()) {
        auto findComponent = m_snapshot->components.find(componentId);
        if (findComponent == m_snapshot->components.end()) {
            qDebug() << "Component not found:" << componentId;
            return isDirty;
        }
        component = &findComponent->second;
    }
    
    if (isTrueValueString(valueOfKeyInMapOrEmpty(*component, "dirty"))) {
        isDirty = true;
    }
    
    QString linkDataType = valueOfKeyInMapOrEmpty(*component, "linkDataType");
    if ("partId" == linkDataType) {
        QString partId = valueOfKeyInMapOrEmpty(*component, "linkData");
        if (checkIsPartDirty(partId)) {
            m_dirtyPartIds.insert(partId);
            isDirty = true;
        }
    }
    
    for (const auto &childId: valueOfKeyInMapOrEmpty(*component, "children").split(",")) {
        if (childId.isEmpty())
            continue;
        if (checkIsComponentDirty(childId)) {
            isDirty = true;
        }
    }
    
    if (isDirty)
        m_dirtyComponentIds.insert(componentId);
    
    return isDirty;
}

void *MeshGenerator::combineComponentMesh(QString componentId, bool *inverse)
{
    QUuid componentIdNotAsString;
    
    *inverse = false;
    
    const std::map<QString, QString> *component = &m_snapshot->rootComponent;
    if (componentId != QUuid().toString()) {
        componentIdNotAsString = QUuid(componentId);
        auto findComponent = m_snapshot->components.find(componentId);
        if (findComponent == m_snapshot->components.end()) {
            qDebug() << "Component not found:" << componentId;
            return nullptr;
        }
        component = &findComponent->second;
    }
    
    if (isTrueValueString(valueOfKeyInMapOrEmpty(*component, "inverse")))
        *inverse = true;
    
    if (m_dirtyComponentIds.find(componentId) == m_dirtyComponentIds.end()) {
        auto findCachedMesh = m_cacheContext->componentCombinableMeshs.find(componentId);
        if (findCachedMesh != m_cacheContext->componentCombinableMeshs.end() &&
                nullptr != findCachedMesh->second) {
            //qDebug() << "Component mesh cache used:" << componentId;
            return cloneCombinableMesh(findCachedMesh->second);
        }
    }
    
    bool smoothSeam = false;
    float smoothSeamFactor = 0.0;
    QString smoothSeamString = valueOfKeyInMapOrEmpty(*component, "smoothSeam");
    if (!smoothSeamString.isEmpty()) {
        smoothSeam = true;
        smoothSeamFactor = smoothSeamString.toFloat();
    }
    
    bool smoothAll = false;
    float smoothAllFactor = 0.0;
    QString smoothAllString = valueOfKeyInMapOrEmpty(*component, "smoothAll");
    if (!smoothAllString.isEmpty()) {
        smoothAll = true;
        smoothAllFactor = smoothAllString.toFloat();
    }
    
    void *resultMesh = nullptr;
    
    PositionMap<bool> positionsBeforeCombination;
    auto &verticesSources = m_cacheContext->componentVerticesSources[componentId];
    verticesSources.map().clear();
    QString linkDataType = valueOfKeyInMapOrEmpty(*component, "linkDataType");
    if ("partId" == linkDataType) {
        QString partId = valueOfKeyInMapOrEmpty(*component, "linkData");
        resultMesh = combinePartMesh(partId);
        for (const auto &bmeshVertex: m_cacheContext->partBmeshVertices[partId]) {
            verticesSources.addPosition(bmeshVertex.position.x(), bmeshVertex.position.y(), bmeshVertex.position.z(),
                bmeshVertex);
        }
    } else {
        for (const auto &childId: valueOfKeyInMapOrEmpty(*component, "children").split(",")) {
            if (childId.isEmpty())
                continue;
            bool childInverse = false;
            void *childCombinedMesh = combineComponentMesh(childId, &childInverse);
            for (const auto &positionIt: m_cacheContext->componentPositions[childId]) {
                positionsBeforeCombination.addPosition(positionIt.x(), positionIt.y(), positionIt.z(), true);
            }
            for (const auto &verticesSourceIt: m_cacheContext->componentVerticesSources[childId].map()) {
                verticesSources.map()[verticesSourceIt.first] = verticesSourceIt.second;
            }
            if (nullptr == childCombinedMesh)
                continue;
            if (nullptr == resultMesh) {
                if (childInverse) {
                    deleteCombinableMesh(childCombinedMesh);
                } else {
                    resultMesh = childCombinedMesh;
                }
            } else {
                void *newResultMesh = childInverse ? diffCombinableMeshs(resultMesh, childCombinedMesh) : unionCombinableMeshs(resultMesh, childCombinedMesh);
                deleteCombinableMesh(childCombinedMesh);
                if (nullptr != newResultMesh) {
                    deleteCombinableMesh(resultMesh);
                    resultMesh = newResultMesh;
                }
            }
        }
    }
    
    if (nullptr != resultMesh) {
        int meshIdForSmooth = convertFromCombinableMesh(m_meshliteContext, resultMesh);
        std::vector<QVector3D> positionsBeforeSmooth;
        loadMeshVerticesPositions(m_meshliteContext, meshIdForSmooth, positionsBeforeSmooth);
        
        if (!positionsBeforeSmooth.empty()) {
            std::vector<int> seamVerticesIds;
            std::unordered_set<int> seamVerticesIndicies;
            
            if (!positionsBeforeCombination.map().empty()) {
                for (size_t vertexIndex = 0; vertexIndex < positionsBeforeSmooth.size(); vertexIndex++) {
                    const auto &oldPosition = positionsBeforeSmooth[vertexIndex];
                    if (!positionsBeforeCombination.findPosition(oldPosition.x(), oldPosition.y(), oldPosition.z())) {
                        seamVerticesIds.push_back(vertexIndex + 1);
                        seamVerticesIndicies.insert(vertexIndex);
                    }
                }
            }
            
            bool meshChanged = false;
            if (m_weldEnabled) {
                if (!seamVerticesIndicies.empty()) {
                    int weldedMeshId = meshWeldSeam(m_meshliteContext, meshIdForSmooth, 0.025, seamVerticesIndicies);
                    {
                        void *testCombinableMesh = convertToCombinableMesh(m_meshliteContext, weldedMeshId);
                        if (nullptr != testCombinableMesh) {
                            deleteCombinableMesh(testCombinableMesh);
                            meshIdForSmooth = weldedMeshId;
                            meshChanged = true;
                        } else {
                            qDebug() << "Weld seam failed, fall back";
                        }
                    }
                }
            }
        
            if (smoothSeam) {
                if (!seamVerticesIds.empty()) {
                    //qDebug() << "smoothSeamFactor:" << smoothSeamFactor << "seamVerticesIndicies.size():" << seamVerticesNum;
                    meshlite_smooth_vertices(m_meshliteContext, meshIdForSmooth, smoothSeamFactor, seamVerticesIds.data(), seamVerticesIds.size());
                    meshChanged = true;
                }
            }
            
            if (smoothAll) {
                meshlite_smooth(m_meshliteContext, meshIdForSmooth, smoothAllFactor);
                meshChanged = true;
            }
            
            if (meshChanged) {
                std::vector<QVector3D> positionsAfterSmooth;
                loadMeshVerticesPositions(m_meshliteContext, meshIdForSmooth, positionsAfterSmooth);
                Q_ASSERT(positionsBeforeSmooth.size() == positionsAfterSmooth.size());
                
                for (size_t vertexIndex = 0; vertexIndex < positionsBeforeSmooth.size(); vertexIndex++) {
                    const auto &oldPosition = positionsBeforeSmooth[vertexIndex];
                    const auto &smoothedPosition = positionsAfterSmooth[vertexIndex];
                    BmeshVertex source;
                    if (verticesSources.findPosition(oldPosition.x(), oldPosition.y(), oldPosition.z(), &source)) {
                        verticesSources.removePosition(oldPosition.x(), oldPosition.y(), oldPosition.z());
                        source.position = smoothedPosition;
                        verticesSources.addPosition(smoothedPosition.x(), smoothedPosition.y(), smoothedPosition.z(), source);
                    }
                }
                deleteCombinableMesh(resultMesh);
                resultMesh = convertToCombinableMesh(m_meshliteContext, meshIdForSmooth);
            }
        }
    }
    
    m_cacheContext->updateComponentCombinableMesh(componentId, resultMesh);
    auto &cachedComponentPositions = m_cacheContext->componentPositions[componentId];
    cachedComponentPositions.clear();
    loadCombinableMeshVerticesPositions(resultMesh, cachedComponentPositions);
    
    return resultMesh;
}

void MeshGenerator::process()
{
    if (nullptr == m_snapshot)
        return;
    QElapsedTimer countTimeConsumed;
    countTimeConsumed.start();

    m_meshliteContext = meshlite_create_context();
    
    initMeshUtils();
    m_meshResultContext = new MeshResultContext;
    
    bool needDeleteCacheContext = false;
    if (nullptr == m_cacheContext) {
        m_cacheContext = new GeneratedCacheContext;
        needDeleteCacheContext = true;
    } else {
        for (auto it = m_cacheContext->partBmeshNodes.begin(); it != m_cacheContext->partBmeshNodes.end(); ) {
            if (m_snapshot->parts.find(it->first) == m_snapshot->parts.end()) {
                auto mirrorFrom = m_cacheContext->partMirrorIdMap.find(it->first);
                if (mirrorFrom != m_cacheContext->partMirrorIdMap.end()) {
                    if (m_snapshot->parts.find(mirrorFrom->second) != m_snapshot->parts.end()) {
                        it++;
                        continue;
                    }
                    m_cacheContext->partMirrorIdMap.erase(mirrorFrom);
                }
                it = m_cacheContext->partBmeshNodes.erase(it);
                continue;
            }
            it++;
        }
        for (auto it = m_cacheContext->partBmeshVertices.begin(); it != m_cacheContext->partBmeshVertices.end(); ) {
            if (m_snapshot->parts.find(it->first) == m_snapshot->parts.end()) {
                it = m_cacheContext->partBmeshVertices.erase(it);
                continue;
            }
            it++;
        }
        for (auto it = m_cacheContext->partBmeshQuads.begin(); it != m_cacheContext->partBmeshQuads.end(); ) {
            if (m_snapshot->parts.find(it->first) == m_snapshot->parts.end()) {
                it = m_cacheContext->partBmeshQuads.erase(it);
                continue;
            }
            it++;
        }
        for (auto it = m_cacheContext->componentCombinableMeshs.begin(); it != m_cacheContext->componentCombinableMeshs.end(); ) {
            if (m_snapshot->components.find(it->first) == m_snapshot->components.end()) {
                deleteCombinableMesh(it->second);
                it = m_cacheContext->componentCombinableMeshs.erase(it);
                continue;
            }
            it++;
        }
        for (auto it = m_cacheContext->componentPositions.begin(); it != m_cacheContext->componentPositions.end(); ) {
            if (m_snapshot->components.find(it->first) == m_snapshot->components.end()) {
                it = m_cacheContext->componentPositions.erase(it);
                continue;
            }
            it++;
        }
        for (auto it = m_cacheContext->componentVerticesSources.begin(); it != m_cacheContext->componentVerticesSources.end(); ) {
            if (m_snapshot->components.find(it->first) == m_snapshot->components.end()) {
                it = m_cacheContext->componentVerticesSources.erase(it);
                continue;
            }
            it++;
        }
    }
    
    collectParts();
    checkDirtyFlags();
    
    m_dirtyComponentIds.insert(QUuid().toString());
    
    m_mainProfileMiddleX = valueOfKeyInMapOrEmpty(m_snapshot->canvas, "originX").toFloat();
    m_mainProfileMiddleY = valueOfKeyInMapOrEmpty(m_snapshot->canvas, "originY").toFloat();
    m_sideProfileMiddleX = valueOfKeyInMapOrEmpty(m_snapshot->canvas, "originZ").toFloat();
    
    int resultMeshId = 0;
    
    bool inverse = false;
    void *combinedMesh = combineComponentMesh(QUuid().toString(), &inverse);
    if (nullptr != combinedMesh) {
        resultMeshId = convertFromCombinableMesh(m_meshliteContext, combinedMesh);
        deleteCombinableMesh(combinedMesh);
    }
    
    for (const auto &verticesSourcesIt: m_cacheContext->componentVerticesSources[QUuid().toString()].map()) {
        m_meshResultContext->bmeshVertices.push_back(verticesSourcesIt.second);
    }
    
    for (const auto &bmeshNodes: m_cacheContext->partBmeshNodes) {
        m_meshResultContext->bmeshNodes.insert(m_meshResultContext->bmeshNodes.end(),
            bmeshNodes.second.begin(), bmeshNodes.second.end());
    }
    
    int triangulatedFinalMeshId = resultMeshId;
    if (triangulatedFinalMeshId > 0) {
        std::set<std::pair<PositionMapKey, PositionMapKey>> sharedQuadEdges;
        for (const auto &bmeshQuads: m_cacheContext->partBmeshQuads) {
            for (const auto &quad: bmeshQuads.second) {
                sharedQuadEdges.insert(std::make_pair(std::get<0>(quad), std::get<2>(quad)));
                sharedQuadEdges.insert(std::make_pair(std::get<1>(quad), std::get<3>(quad)));
            }
        }
        if (!sharedQuadEdges.empty()) {
            resultMeshId = meshQuadify(m_meshliteContext, triangulatedFinalMeshId, sharedQuadEdges, m_forMakePositionKey);
        }
    }
    
    if (resultMeshId > 0) {
        loadGeneratedPositionsToMeshResultContext(m_meshliteContext, triangulatedFinalMeshId);
        m_mesh = new MeshLoader(m_meshliteContext, resultMeshId, triangulatedFinalMeshId, Theme::white, &m_meshResultContext->triangleColors(), m_smoothNormal);
    }
    
    if (needDeleteCacheContext) {
        delete m_cacheContext;
        m_cacheContext = nullptr;
    }
    
    meshlite_destroy_context(m_meshliteContext);
    
    qDebug() << "The mesh generation took" << countTimeConsumed.elapsed() << "milliseconds";
    
    this->moveToThread(QGuiApplication::instance()->thread());
    emit finished();
}

void MeshGenerator::checkDirtyFlags()
{
    checkIsComponentDirty(QUuid().toString());
}

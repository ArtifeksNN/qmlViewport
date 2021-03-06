#include "viewport_item.h"

#include "render_thread.h"
#include "texture_node.h"

ViewportItem::ViewportItem() : _renderThread(nullptr) {
    setFlag(ItemHasContents, true);
    _renderThread = new RenderThread(QSize(512, 512));
}

void ViewportItem::ready() {
    _renderThread->surface = new QOffscreenSurface();
    _renderThread->surface->setFormat(_renderThread->context->format());
    _renderThread->surface->create();

    _renderThread->moveToThread(_renderThread);

    connect(window(), &QQuickWindow::sceneGraphInvalidated, _renderThread,
            &RenderThread::shutDown, Qt::QueuedConnection);

    _renderThread->start();
    update();
}

QSGNode *ViewportItem::updatePaintNode(QSGNode *oldNode,
                                       QQuickItem::UpdatePaintNodeData *) {
    if (width() <= 0 || height() <= 0) {
        delete oldNode;
        return nullptr;
    }
    _renderThread->_size = QSize(width(), height());

    TextureNode *node = static_cast<TextureNode *>(oldNode);

    if (!_renderThread->context) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QOpenGLContext *current = window()->openglContext();
#else
        QOpenGLContext *current = QOpenGLContext::currentContext();
#endif
        current->doneCurrent();

        _renderThread->context = std::make_shared<QOpenGLContext>();
        _renderThread->context->setFormat(current->format());
        _renderThread->context->setShareContext(current);
        _renderThread->context->create();
        _renderThread->context->moveToThread(_renderThread);
        current->makeCurrent(window());
        QMetaObject::invokeMethod(this, "ready");
        return nullptr;
    }

    if (!node) {
        node = new TextureNode(window());

        connect(_renderThread, &RenderThread::textureReady, node,
                &TextureNode::newTexture, Qt::DirectConnection);
        connect(node, &TextureNode::pendingNewTexture, window(),
                &QQuickWindow::update, Qt::QueuedConnection);
        connect(window(), &QQuickWindow::beforeRendering, node,
                &TextureNode::prepareNode, Qt::DirectConnection);
        connect(node, &TextureNode::textureInUse, _renderThread,
                &RenderThread::renderNext, Qt::QueuedConnection);

        QMetaObject::invokeMethod(_renderThread, "renderNext",
                                  Qt::QueuedConnection);
    }

    node->setRect(boundingRect());

    return node;
}

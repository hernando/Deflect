/*********************************************************************/
/* Copyright (c) 2015, EPFL/Blue Brain Project                       */
/*                     Daniel.Nachbaur <daniel.nachbaur@epfl.ch>     */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/*   1. Redistributions of source code must retain the above         */
/*      copyright notice, this list of conditions and the following  */
/*      disclaimer.                                                  */
/*                                                                   */
/*   2. Redistributions in binary form must reproduce the above      */
/*      copyright notice, this list of conditions and the following  */
/*      disclaimer in the documentation and/or other materials       */
/*      provided with the distribution.                              */
/*                                                                   */
/*    THIS  SOFTWARE IS PROVIDED  BY THE  UNIVERSITY OF  TEXAS AT    */
/*    AUSTIN  ``AS IS''  AND ANY  EXPRESS OR  IMPLIED WARRANTIES,    */
/*    INCLUDING, BUT  NOT LIMITED  TO, THE IMPLIED  WARRANTIES OF    */
/*    MERCHANTABILITY  AND FITNESS FOR  A PARTICULAR  PURPOSE ARE    */
/*    DISCLAIMED.  IN  NO EVENT SHALL THE UNIVERSITY  OF TEXAS AT    */
/*    AUSTIN OR CONTRIBUTORS BE  LIABLE FOR ANY DIRECT, INDIRECT,    */
/*    INCIDENTAL,  SPECIAL, EXEMPLARY,  OR  CONSEQUENTIAL DAMAGES    */
/*    (INCLUDING, BUT  NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE    */
/*    GOODS  OR  SERVICES; LOSS  OF  USE,  DATA,  OR PROFITS;  OR    */
/*    BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON ANY THEORY OF    */
/*    LIABILITY, WHETHER  IN CONTRACT, STRICT  LIABILITY, OR TORT    */
/*    (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY WAY OUT    */
/*    OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF ADVISED  OF  THE    */
/*    POSSIBILITY OF SUCH DAMAGE.                                    */
/*                                                                   */
/* The views and conclusions contained in the software and           */
/* documentation are those of the authors and should not be          */
/* interpreted as representing official policies, either expressed   */
/* or implied, of The University of Texas at Austin.                 */
/*********************************************************************/

#include "QmlStreamerImpl.h"
#include "EventReceiver.h"

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickWindow>

namespace
{
const std::string DEFAULT_STREAM_NAME( "QmlStreamer" );
}

class RenderControl : public QQuickRenderControl
{
public:
    RenderControl( QWindow* w )
        : _window( w )
    {}

    QWindow* renderWindow( QPoint* offset ) final
    {
        if( offset )
            *offset = QPoint( 0, 0 );
        return _window;
    }

private:
    QWindow* _window;
};

namespace deflect
{
namespace qt
{

QmlStreamer::Impl::Impl( const QString& qmlFile, const std::string& streamHost,
                         const std::string& streamName )
    : QWindow()
    , _context( new QOpenGLContext )
    , _offscreenSurface( new QOffscreenSurface )
    , _renderControl( new RenderControl( this ))
    // Create a QQuickWindow that is associated with out render control. Note
    // that this window never gets created or shown, meaning that it will never
    // get an underlying native (platform) window.
    , _quickWindow( new QQuickWindow( _renderControl ))
    , _qmlEngine( new QQmlEngine )
    , _qmlComponent( new QQmlComponent( _qmlEngine, QUrl( qmlFile )))
    , _rootItem( nullptr )
    , _fbo( nullptr )
    , _stream( nullptr )
    , _eventHandler( nullptr )
    , _streaming( false )
    , _streamHost( streamHost )
    , _streamName( streamName )
{
    setSurfaceType( QSurface::OpenGLSurface );

    // Qt Quick may need a depth and stencil buffer
    QSurfaceFormat format_;
    format_.setDepthBufferSize( 16 );
    format_.setStencilBufferSize( 8 );
    setFormat( format_ );

    _context->setFormat( format_ );
    _context->create();

    // Pass _context->format(), not format_. Format does not specify and color
    // buffer sizes, while the context, that has just been created, reports a
    // format that has these values filled in. Pass this to the offscreen
    // surface to make sure it will be compatible with the context's
    // configuration.
    _offscreenSurface->setFormat( _context->format( ));
    _offscreenSurface->create();

    if( !_qmlEngine->incubationController( ))
        _qmlEngine->setIncubationController( _quickWindow->incubationController( ));

    // When Quick says there is a need to render, we will not render
    // immediately. Instead, a timer with a small interval is used to get better
    // performance.
    _updateTimer.setSingleShot( true );
    _updateTimer.setInterval( 5 );
    connect( &_updateTimer, &QTimer::timeout,
             this, &QmlStreamer::Impl::_render );

    // Now hook up the signals. For simplicy we don't differentiate between
    // renderRequested (only render is needed, no sync) and sceneChanged (polish
    // and sync is needed too).
    connect( _quickWindow, &QQuickWindow::sceneGraphInitialized,
             this, &QmlStreamer::Impl::_createFbo );
    connect( _quickWindow, &QQuickWindow::sceneGraphInvalidated,
             this, &QmlStreamer::Impl::_destroyFbo );
    connect( _renderControl, &QQuickRenderControl::renderRequested,
             this, &QmlStreamer::Impl::_requestUpdate );
    connect( _renderControl, &QQuickRenderControl::sceneChanged,
             this, &QmlStreamer::Impl::_requestUpdate );

    // remote URL to QML components are loaded asynchronously
    if( _qmlComponent->isLoading( ))
        connect( _qmlComponent, &QQmlComponent::statusChanged,
                 this, &QmlStreamer::Impl::_setupRootItem );
    else if( !_setupRootItem( ))
        throw std::runtime_error( "Failed to setup/load QML" );
}

QmlStreamer::Impl::~Impl()
{
    delete _eventHandler;
    delete _stream;

    _context->makeCurrent( _offscreenSurface );

    // delete first to free scenegraph resources for following destructions
    delete _renderControl;

    delete _rootItem;
    delete _qmlComponent;
    delete _quickWindow;
    delete _qmlEngine;
    delete _fbo;

    _context->doneCurrent();

    delete _offscreenSurface;
    delete _context;
}

void QmlStreamer::Impl::_createFbo()
{
    _fbo =
        new QOpenGLFramebufferObject( _quickWindow->size() * devicePixelRatio(),
                               QOpenGLFramebufferObject::CombinedDepthStencil );
    _quickWindow->setRenderTarget( _fbo );
}

void QmlStreamer::Impl::_destroyFbo()
{
    delete _fbo;
    _fbo = 0;
}

void QmlStreamer::Impl::_render()
{
    if( !_context->makeCurrent( _offscreenSurface ))
        return;

    // fixed sized rootItems won't signal sizes changes, finish their setup now
    if( !_fbo )
        _updateSizes( QSize( _rootItem->width(), _rootItem->height( )));

    // Polish, synchronize and render the next frame (into our fbo). In this
    // example everything happens on the same thread and therefore all three
    // steps are performed in succession from here. In a threaded setup the
    // render() call would happen on a separate thread.
    _renderControl->polishItems();
    _renderControl->sync();
    _renderControl->render();

    _quickWindow->resetOpenGLState();
    QOpenGLFramebufferObject::bindDefault();

    _context->functions()->glFlush();

    if( !_streaming )
    {
        QCoreApplication::quit();
        return;
    }

    const QImage image = _fbo->toImage();
    if( image.isNull( ))
    {
        qDebug() << "Empty image not streamed";
        return;
    }

    ImageWrapper imageWrapper( image.constBits(), image.width(), image.height(),
                               BGRA, 0, 0 );
    imageWrapper.compressionPolicy = COMPRESSION_ON;
    imageWrapper.compressionQuality = 100;
    _streaming = _stream->send( imageWrapper ) && _stream->finishFrame();
}

void QmlStreamer::Impl::_requestUpdate()
{
    _updateTimer.start();
}

void QmlStreamer::Impl::_onPressed( double x_, double y_ )
{
    const QPoint point( x_ * width(), y_ * height( ));
    QMouseEvent* e = new QMouseEvent( QEvent::MouseButtonPress, point,
                                      Qt::LeftButton, Qt::LeftButton,
                                      Qt::NoModifier );
    QCoreApplication::postEvent( _quickWindow, e );
}

void QmlStreamer::Impl::_onMoved( double x_, double y_ )
{
    const QPoint point( x_ * width(), y_ * height( ));
    QMouseEvent* e = new QMouseEvent( QEvent::MouseMove, point, Qt::LeftButton,
                                      Qt::LeftButton, Qt::NoModifier );
    QCoreApplication::postEvent( _quickWindow, e );
}

void QmlStreamer::Impl::_onReleased( double x_, double y_ )
{
    const QPoint point( x_ * width(), y_ * height( ));
    QMouseEvent* e = new QMouseEvent( QEvent::MouseButtonRelease, point,
                                      Qt::LeftButton, Qt::NoButton,
                                      Qt::NoModifier );
    QCoreApplication::postEvent( _quickWindow, e );
}

void QmlStreamer::Impl::_onWheeled( double x_, double y_, double deltaY )
{
    const QPoint point( x_ * width(), y_ * height( ));
    QWheelEvent* e = new QWheelEvent( point, deltaY, Qt::NoButton,
                                      Qt::NoModifier, Qt::Vertical );
    QCoreApplication::postEvent( _quickWindow, e );
}

void QmlStreamer::Impl::_onResized( double x_, double y_ )
{
    QResizeEvent* resizeEvent_ = new QResizeEvent( QSize( x_, y_ ), size( ));
    QCoreApplication::postEvent( this, resizeEvent_ );
}

bool QmlStreamer::Impl::_setupRootItem()
{
    disconnect( _qmlComponent, &QQmlComponent::statusChanged,
                this, &QmlStreamer::Impl::_setupRootItem );

    if( _qmlComponent->isError( ))
    {
        QList< QQmlError > errorList = _qmlComponent->errors();
        foreach( const QQmlError &error, errorList )
            qWarning() << error.url() << error.line() << error;
        return false;
    }

    QObject* rootObject = _qmlComponent->create();
    if( _qmlComponent->isError( ))
    {
        QList< QQmlError > errorList = _qmlComponent->errors();
        foreach( const QQmlError &error, errorList )
            qWarning() << error.url() << error.line() << error;
        return false;
    }

    _rootItem = qobject_cast< QQuickItem* >( rootObject );
    if( !_rootItem )
    {
        delete rootObject;
        return false;
    }

    connect( _rootItem, &QQuickItem::widthChanged, this,
             [this]() { _updateSizes( QSize( _rootItem->width(),
                                             _rootItem->height( ))); } );
    connect( _rootItem, &QQuickItem::heightChanged, this,
             [this]() { _updateSizes( QSize( _rootItem->width(),
                                             _rootItem->height( ))); } );

    connect( _quickWindow, &QQuickWindow::minimumWidthChanged,
             [this]( int size ) { _sizeHints.minWidth = size; } );
    connect( _quickWindow, &QQuickWindow::minimumHeightChanged,
             [this]( int size ) { _sizeHints.minHeight = size; } );
    connect( _quickWindow, &QQuickWindow::maximumWidthChanged,
             [this]( int size ) { _sizeHints.maxWidth = size; } );
    connect( _quickWindow, &QQuickWindow::maximumHeightChanged,
             [this]( int size ) { _sizeHints.maxHeight = size; } );
    connect( _quickWindow, &QQuickWindow::widthChanged,
             [this]( int size ) { _sizeHints.preferredWidth = size; } );
    connect( _quickWindow, &QQuickWindow::heightChanged,
             [this]( int size ) { _sizeHints.preferredHeight = size; } );

    // The root item is ready. Associate it with the window.
    _rootItem->setParentItem( _quickWindow->contentItem( ));

    return true;
}

std::string QmlStreamer::Impl::_getDeflectStreamName() const
{
    if( !_streamName.empty( ))
        return _streamName;

    const std::string streamName = _rootItem->objectName().toStdString();
    return streamName.empty() ? DEFAULT_STREAM_NAME : streamName;
}

bool QmlStreamer::Impl::_setupDeflectStream()
{
    if( !_stream )
        _stream = new Stream( _getDeflectStreamName(), _streamHost );

    if( !_stream->isConnected( ))
        return false;

    if( !_stream->registerForEvents( ))
        return false;

    _stream->sendSizeHints( _sizeHints );

    _streaming = true;
    _eventHandler = new EventReceiver( *_stream );
    connect( _eventHandler, &EventReceiver::pressed,
             this, &QmlStreamer::Impl::_onPressed );
    connect( _eventHandler, &EventReceiver::released,
             this, &QmlStreamer::Impl::_onReleased );
    connect( _eventHandler, &EventReceiver::moved,
             this, &QmlStreamer::Impl::_onMoved );
    connect( _eventHandler, &EventReceiver::resized,
             this, &QmlStreamer::Impl::_onResized );
    connect( _eventHandler, &EventReceiver::wheeled,
             this, &QmlStreamer::Impl::_onWheeled );

    return true;
}

void QmlStreamer::Impl::_updateSizes( const QSize& size_ )
{
    setWidth( size_.width( ));
    setHeight( size_.height( ));
    if( _rootItem )
    {
        _rootItem->setWidth( size_.width( ));
        _rootItem->setHeight( size_.height( ));
    }
    _quickWindow->blockSignals( true );
    _quickWindow->setWidth( size_.width( ));
    _quickWindow->setHeight( size_.height( ));
    _quickWindow->blockSignals( false );

    // Initialize the render control and our OpenGL resources. Do this as late
    // as possible to use the proper size reported by the rootItem.
    if( !_streaming )
    {
        _context->makeCurrent( _offscreenSurface );
        _renderControl->initialize( _context );

        if( !_setupDeflectStream( ))
            qWarning() << "Could not setup Deflect stream";
    }
}

void QmlStreamer::Impl::resizeEvent( QResizeEvent* e )
{
    _updateSizes( e->size( ));

    if( _fbo && _fbo->size() != e->size() * devicePixelRatio() &&
        _context->makeCurrent( _offscreenSurface ))
    {
        _destroyFbo();
        _createFbo();
        _context->doneCurrent();
    }
}

void QmlStreamer::Impl::mousePressEvent( QMouseEvent* e )
{
    // Use the constructor taking localPos and screenPos. That puts localPos
    // into the event's localPos and windowPos, and screenPos into the event's
    // screenPos. This way the windowPos in e is ignored and is replaced by
    // localPos. This is necessary because QQuickWindow thinks of itself as a
    // top-level window always.
    QMouseEvent mappedEvent( e->type(), e->localPos(), e->screenPos(),
                             e->button(), e->buttons(), e->modifiers( ));
    QCoreApplication::sendEvent( _quickWindow, &mappedEvent );
}

void QmlStreamer::Impl::mouseReleaseEvent( QMouseEvent* e )
{
    QMouseEvent mappedEvent( e->type(), e->localPos(), e->screenPos(),
                             e->button(), e->buttons(), e->modifiers( ));
    QCoreApplication::sendEvent( _quickWindow, &mappedEvent );
}

}
}

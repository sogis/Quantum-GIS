/***************************************************************************
    qgswmsdataitems.cpp
    ---------------------
    begin                : October 2011
    copyright            : (C) 2011 by Martin Dobias
    email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgswmsdataitems.h"

#include "qgslogger.h"

#include "qgsdatasourceuri.h"
#include "qgswmscapabilities.h"
#include "qgswmsconnection.h"
#include "qgswmssourceselect.h"
#include "qgsnewhttpconnection.h"
#include "qgstilescalewidget.h"

// ---------------------------------------------------------------------------
QgsWMSConnectionItem::QgsWMSConnectionItem( QgsDataItem* parent, QString name, QString path, QString uri )
    : QgsDataCollectionItem( parent, name, path )
    , mUri( uri )
    , mCapabilitiesDownload( 0 )
{
  mIconName = "mIconConnect.png";
  mCapabilitiesDownload = new QgsWmsCapabilitiesDownload();
}

QgsWMSConnectionItem::~QgsWMSConnectionItem()
{
  QgsDebugMsg( "Entered" );
  delete mCapabilitiesDownload;
}

void QgsWMSConnectionItem::deleteLater()
{
  QgsDebugMsg( "Entered" );
  if ( mCapabilitiesDownload )
  {
    mCapabilitiesDownload->abort();
  }
  QgsDataCollectionItem::deleteLater();
}

QVector<QgsDataItem*> QgsWMSConnectionItem::createChildren()
{
  QgsDebugMsg( "Entered" );
  QVector<QgsDataItem*> children;

  QgsDataSourceURI uri;
  uri.setEncodedUri( mUri );

  QgsDebugMsg( "mUri = " + mUri );

  QgsWmsSettings wmsSettings;
  if ( !wmsSettings.parseUri( mUri ) )
  {
    children.append( new QgsErrorItem( this, tr( "Failed to parse WMS URI" ), mPath + "/error" ) );
    return children;
  }

  bool res = mCapabilitiesDownload->downloadCapabilities( wmsSettings.baseUrl(), wmsSettings.authorization() );

  if ( !res )
  {
    children.append( new QgsErrorItem( this, tr( "Failed to download capabilities" ), mPath + "/error" ) );
    return children;
  }

  QgsWmsCapabilities caps;
  if ( !caps.parseResponse( mCapabilitiesDownload->response(), wmsSettings.parserSettings() ) )
  {
    children.append( new QgsErrorItem( this, tr( "Failed to parse capabilities" ), mPath + "/error" ) );
    return children;
  }

  // Attention: supportedLayers() gives tree leafs, not top level
  QVector<QgsWmsLayerProperty> layerProperties = caps.supportedLayers();
  if ( layerProperties.count() )
  {
    QgsWmsCapabilitiesProperty capabilitiesProperty = caps.capabilitiesProperty();
    const QgsWmsCapabilityProperty& capabilityProperty = capabilitiesProperty.capability;

    // Top level layer is present max once
    // <element name="Capability">
    //    <element ref="wms:Layer" minOccurs="0"/>  - default maxOccurs=1
    const QgsWmsLayerProperty &topLayerProperty = capabilityProperty.layer;
    Q_FOREACH ( const QgsWmsLayerProperty &layerProperty, topLayerProperty.layer )
    {
      // Attention, the name may be empty
      QgsDebugMsg( QString::number( layerProperty.orderId ) + " " + layerProperty.name + " " + layerProperty.title );
      QString pathName = layerProperty.name.isEmpty() ? QString::number( layerProperty.orderId ) : layerProperty.name;

      QgsWMSLayerItem *layer = new QgsWMSLayerItem( this, layerProperty.title, mPath + "/" + pathName, capabilitiesProperty, uri, layerProperty );

      children << layer;
    }
  }

  QList<QgsWmtsTileLayer> tileLayers = caps.supportedTileLayers();
  if ( tileLayers.count() )
  {
    QHash<QString, QgsWmtsTileMatrixSet> tileMatrixSets = caps.supportedTileMatrixSets();

    Q_FOREACH ( const QgsWmtsTileLayer &l, tileLayers )
    {
      QString title = l.title.isEmpty() ? l.identifier : l.title;
      QgsDataItem *layerItem = l.styles.size() == 1 ? this : new QgsDataCollectionItem( this, title, mPath + "/" + l.identifier );
      if ( layerItem != this )
      {
        layerItem->setToolTip( title );
        addChildItem( layerItem );
      }

      Q_FOREACH ( const QgsWmtsStyle &style, l.styles )
      {
        QString styleName = style.title.isEmpty() ? style.identifier : style.title;
        if ( layerItem == this )
          styleName.prepend( title + " - " );

        QgsDataItem *styleItem = l.setLinks.size() == 1 ? layerItem : new QgsDataCollectionItem( layerItem, styleName, layerItem->path() + "/" + style.identifier );
        if ( styleItem != layerItem )
        {
          styleItem->setToolTip( styleName );
          layerItem->addChildItem( styleItem );
        }

        Q_FOREACH ( const QgsWmtsTileMatrixSetLink &setLink, l.setLinks )
        {
          QString linkName = setLink.tileMatrixSet;
          if ( styleItem == layerItem )
            linkName.prepend( styleName + " - " );

          QgsDataItem *linkItem = l.formats.size() == 1 ? styleItem : new QgsDataCollectionItem( styleItem, linkName, styleItem->path() + "/" + setLink.tileMatrixSet );
          if ( linkItem != styleItem )
          {
            linkItem->setToolTip( linkName );
            styleItem->addChildItem( linkItem );
          }

          Q_FOREACH ( const QString& format, l.formats )
          {
            QString name = format;
            if ( linkItem == styleItem )
              name.prepend( linkName + " - " );

            QgsDataItem *layerItem = new QgsWMTSLayerItem( linkItem, name, linkItem->path() + "/" + name, uri,
                l.identifier, format, style.identifier, setLink.tileMatrixSet, tileMatrixSets[ setLink.tileMatrixSet ].crs, title );
            layerItem->setToolTip( name );
            linkItem->addChildItem( layerItem );
          }
        }
      }
    }
  }

  return children;
}

bool QgsWMSConnectionItem::equal( const QgsDataItem *other )
{
  if ( type() != other->type() )
  {
    return false;
  }
  const QgsWMSConnectionItem *o = dynamic_cast<const QgsWMSConnectionItem *>( other );
  if ( !o )
  {
    return false;
  }

  return ( mPath == o->mPath && mName == o->mName );
}

QList<QAction*> QgsWMSConnectionItem::actions()
{
  QList<QAction*> lst;

  QAction* actionEdit = new QAction( tr( "Edit..." ), this );
  connect( actionEdit, SIGNAL( triggered() ), this, SLOT( editConnection() ) );
  lst.append( actionEdit );

  QAction* actionDelete = new QAction( tr( "Delete" ), this );
  connect( actionDelete, SIGNAL( triggered() ), this, SLOT( deleteConnection() ) );
  lst.append( actionDelete );

  return lst;
}

void QgsWMSConnectionItem::editConnection()
{
  QgsNewHttpConnection nc( 0, "/Qgis/connections-wms/", mName );

  if ( nc.exec() )
  {
    // the parent should be updated
    mParent->refresh();
  }
}

void QgsWMSConnectionItem::deleteConnection()
{
  QgsWMSConnection::deleteConnection( mName );
  // the parent should be updated
  mParent->refresh();
}


// ---------------------------------------------------------------------------

QgsWMSLayerItem::QgsWMSLayerItem( QgsDataItem* parent, QString name, QString path, const QgsWmsCapabilitiesProperty &capabilitiesProperty, QgsDataSourceURI dataSourceUri, const QgsWmsLayerProperty &layerProperty )
    : QgsLayerItem( parent, name, path, QString(), QgsLayerItem::Raster, "wms" )
    , mCapabilitiesProperty( capabilitiesProperty )
    , mDataSourceUri( dataSourceUri )
    , mLayerProperty( layerProperty )
{
  mSupportedCRS = mLayerProperty.crs;
  mSupportFormats = mCapabilitiesProperty.capability.request.getMap.format;
  QgsDebugMsg( "uri = " + mDataSourceUri.encodedUri() );

  mUri = createUri();

  // Populate everything, it costs nothing, all info about layers is collected
  Q_FOREACH ( const QgsWmsLayerProperty &layerProperty, mLayerProperty.layer )
  {
    // Attention, the name may be empty
    QgsDebugMsg( QString::number( layerProperty.orderId ) + " " + layerProperty.name + " " + layerProperty.title );
    QString pathName = layerProperty.name.isEmpty() ? QString::number( layerProperty.orderId ) : layerProperty.name;
    QgsWMSLayerItem * layer = new QgsWMSLayerItem( this, layerProperty.title, mPath + "/" + pathName, mCapabilitiesProperty, dataSourceUri, layerProperty );
    //mChildren.append( layer );
    addChildItem( layer );
  }

  mIconName = "mIconWms.svg";

  setState( Populated );
}

QgsWMSLayerItem::~QgsWMSLayerItem()
{
}

QString QgsWMSLayerItem::createUri()
{
  if ( mLayerProperty.name.isEmpty() )
    return ""; // layer collection

  // Number of styles must match number of layers
  mDataSourceUri.setParam( "layers", mLayerProperty.name );
  QString style = mLayerProperty.style.size() > 0 ? mLayerProperty.style[0].name : "";
  mDataSourceUri.setParam( "styles", style );

  QString format;
  // get first supported by qt and server
  QVector<QgsWmsSupportedFormat> formats( QgsWmsProvider::supportedFormats() );
  Q_FOREACH ( const QgsWmsSupportedFormat &f, formats )
  {
    if ( mCapabilitiesProperty.capability.request.getMap.format.indexOf( f.format ) >= 0 )
    {
      format = f.format;
      break;
    }
  }
  mDataSourceUri.setParam( "format", format );

  QString crs;
  // get first known if possible
  QgsCoordinateReferenceSystem testCrs;
  Q_FOREACH ( const QString& c, mLayerProperty.crs )
  {
    testCrs.createFromOgcWmsCrs( c );
    if ( testCrs.isValid() )
    {
      crs = c;
      break;
    }
  }
  if ( crs.isEmpty() && mLayerProperty.crs.size() > 0 )
  {
    crs = mLayerProperty.crs[0];
  }
  mDataSourceUri.setParam( "crs", crs );
  //uri = rasterLayerPath + "|layers=" + layers.join( "," ) + "|styles=" + styles.join( "," ) + "|format=" + format + "|crs=" + crs;

  return mDataSourceUri.encodedUri();
}

// ---------------------------------------------------------------------------

QgsWMTSLayerItem::QgsWMTSLayerItem( QgsDataItem *parent,
                                    const QString &name,
                                    const QString &path,
                                    const QgsDataSourceURI &uri,
                                    const QString &id,
                                    const QString &format,
                                    const QString &style,
                                    const QString &tileMatrixSet,
                                    const QString &crs,
                                    const QString &title )
    : QgsLayerItem( parent, name, path, QString(), QgsLayerItem::Raster, "wms" )
    , mDataSourceUri( uri )
    , mId( id )
    , mFormat( format )
    , mStyle( style )
    , mTileMatrixSet( tileMatrixSet )
    , mCrs( crs )
    , mTitle( title )
{
  mUri = createUri();
  setState( Populated );
}

QgsWMTSLayerItem::~QgsWMTSLayerItem()
{
}

QString QgsWMTSLayerItem::createUri()
{
  // TODO dimensions

  QgsDataSourceURI uri( mDataSourceUri );
  uri.setParam( "layers", mId );
  uri.setParam( "styles", mStyle );
  uri.setParam( "format", mFormat );
  uri.setParam( "crs", mCrs );
  uri.setParam( "tileMatrixSet", mTileMatrixSet );
  return uri.encodedUri();
}

// ---------------------------------------------------------------------------
QgsWMSRootItem::QgsWMSRootItem( QgsDataItem* parent, QString name, QString path )
    : QgsDataCollectionItem( parent, name, path )
{
  mCapabilities |= Fast;
  mIconName = "mIconWms.svg";
  populate();
}

QgsWMSRootItem::~QgsWMSRootItem()
{
}

QVector<QgsDataItem*> QgsWMSRootItem::createChildren()
{
  QVector<QgsDataItem*> connections;

  Q_FOREACH ( const QString& connName, QgsWMSConnection::connectionList() )
  {
    QgsWMSConnection connection( connName );
    QgsDataItem * conn = new QgsWMSConnectionItem( this, connName, mPath + "/" + connName, connection.uri().encodedUri() );

    connections.append( conn );
  }
  return connections;
}

QList<QAction*> QgsWMSRootItem::actions()
{
  QList<QAction*> lst;

  QAction* actionNew = new QAction( tr( "New Connection..." ), this );
  connect( actionNew, SIGNAL( triggered() ), this, SLOT( newConnection() ) );
  lst.append( actionNew );

  return lst;
}


QWidget * QgsWMSRootItem::paramWidget()
{
  QgsWMSSourceSelect *select = new QgsWMSSourceSelect( 0, 0, true, true );
  connect( select, SIGNAL( connectionsChanged() ), this, SLOT( connectionsChanged() ) );
  return select;
}

void QgsWMSRootItem::connectionsChanged()
{
  refresh();
}

void QgsWMSRootItem::newConnection()
{
  QgsNewHttpConnection nc( 0 );

  if ( nc.exec() )
  {
    refresh();
  }
}


// ---------------------------------------------------------------------------

QGISEXTERN void registerGui( QMainWindow *mainWindow )
{
  QgsTileScaleWidget::showTileScale( mainWindow );
}

QGISEXTERN QgsWMSSourceSelect * selectWidget( QWidget * parent, Qt::WindowFlags fl )
{
  return new QgsWMSSourceSelect( parent, fl );
}

QGISEXTERN int dataCapabilities()
{
  return  QgsDataProvider::Net;
}

QGISEXTERN QgsDataItem * dataItem( QString thePath, QgsDataItem* parentItem )
{
  QgsDebugMsg( "thePath = " + thePath );
  if ( thePath.isEmpty() )
  {
    return new QgsWMSRootItem( parentItem, "WMS", "wms:" );
  }

  // path schema: wms:/connection name (used by OWS)
  if ( thePath.startsWith( "wms:/" ) )
  {
    QString connectionName = thePath.split( '/' ).last();
    if ( QgsWMSConnection::connectionList().contains( connectionName ) )
    {
      QgsWMSConnection connection( connectionName );
      return new QgsWMSConnectionItem( parentItem, "WMS", thePath, connection.uri().encodedUri() );
    }
  }

  return 0;
}


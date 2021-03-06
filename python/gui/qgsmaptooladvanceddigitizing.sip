/***************************************************************************
    qgsmaptooladvanceddigitizing.h  - map tool with event in map coordinates
    ----------------------
    begin                : October 2014
    copyright            : (C) Denis Rouzaud
    email                : denis.rouzaud@gmail.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/**
 * @brief The QgsMapToolAdvancedDigitizing class is a QgsMapTool whcih gives event directly in map coordinates and allows filtering its events.
 * Events from QgsMapTool are caught and their QMouseEvent are transformed into QgsMapMouseEvent (with map coordinates).
 * Events are then forwarded to corresponding virtual methods which can be reimplemented in subclasses.
 * An event filter can be set on the map tool to filter and modify the events in map coordinates (@see QgsMapToolMapEventFilter).
 * @note at the moment, the event filter is used by the CAD tools (@see QgsCadDocWidget).
 * @note the event filter definition is not exposed in python API to avoid any unexpected behavior.
 */
class QgsMapToolAdvancedDigitizing : QgsMapToolEdit
{
%TypeHeaderCode
#include "qgsmaptooladvanceddigitizing.h"
%End
  public:
    enum CaptureMode
    {
      CaptureNone,
      CapturePoint,
      CaptureLine,
      CapturePolygon
    };

    explicit QgsMapToolAdvancedDigitizing( QgsMapCanvas* canvas, QgsAdvancedDigitizingDockWidget* cadDockWidget );

    ~QgsMapToolAdvancedDigitizing();

    //! catch the mouse press event, filters it, transforms it to map coordinates and send it to virtual method
    void canvasPressEvent( QgsMapMouseEvent* e );
    //! catch the mouse release event, filters it, transforms it to map coordinates and send it to virtual method
    void canvasReleaseEvent( QgsMapMouseEvent* e );
    //! catch the mouse move event, filters it, transforms it to map coordinates and send it to virtual method
    void canvasMoveEvent( QgsMapMouseEvent* e );

    /**
     * The capture mode
     *
     * @return Capture mode
     */
    CaptureMode mode() const;

    /**
     * Registers this maptool with the cad dock widget
     */
    void activate();

    /**
     * Unregisters this maptool from the cad dock widget
     */
    void deactivate();

    QgsAdvancedDigitizingDockWidget* cadDockWidget() const;

  protected:
    /**
     * this method when subclassing this class.
     * This will receive adapted events from the cad system whenever a
     * canvasPressEvent is triggered and it's not hidden by the cad's
     * construction mode.
     *
     * @param e Mouse events prepared by the cad system
     */
    virtual void cadCanvasPressEvent( QgsMapMouseEvent* e );


    /**
     * this method when subclassing this class.
     * This will receive adapted events from the cad system whenever a
     * canvasReleaseEvent is triggered and it's not hidden by the cad's
     * construction mode.
     *
     * @param e Mouse events prepared by the cad system
     */
    virtual void cadCanvasReleaseEvent( QgsMapMouseEvent* e );


    /**
     * this method when subclassing this class.
     * This will receive adapted events from the cad system whenever a
     * canvasMoveEvent is triggered and it's not hidden by the cad's
     * construction mode.
     *
     * @param e Mouse events prepared by the cad system
     */
    virtual void cadCanvasMoveEvent( QgsMapMouseEvent* e );
};

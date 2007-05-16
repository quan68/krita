/*
 * imagesize.cc -- Part of Krita
 *
 * Copyright (c) 2004 Boudewijn Rempt (boud@valdyas.org)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <math.h>

#include <stdlib.h>

#include <QSlider>
#include <QPoint>
#include <QRect>

#include <klocale.h>
#include <kiconloader.h>
#include <kcomponentdata.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <kgenericfactory.h>
#include <kstandardaction.h>
#include <kactioncollection.h>

#include <kis_config.h>
#include <kis_image.h>
#include <kis_layer.h>
#include <kis_global.h>
#include <kis_statusbar.h>
#include <kis_types.h>
#include <kis_view2.h>
#include <kis_selection.h>
#include <kis_selection_manager.h>
#include <kis_transaction.h>
#include <kis_image_manager.h>
#include <kis_layer_manager.h>
#include <kis_transform_visitor.h>

#include "imagesize.h"
#include "dlg_imagesize.h"
#include "dlg_layersize.h"
#include "kis_filter_strategy.h"

typedef KGenericFactory<ImageSize> ImageSizeFactory;
K_EXPORT_COMPONENT_FACTORY( kritaimagesize, ImageSizeFactory( "krita" ) )

ImageSize::ImageSize(QObject *parent, const QStringList &)
    : KParts::Plugin(parent)
{
    if ( parent->inherits("KisView2") )
    {
        setComponentData(ImageSizeFactory::componentData());

        setXMLFile(KStandardDirs::locate("data","kritaplugins/imagesize.rc"), true);

        KAction *action  = new KAction(i18n("Scale To New Size..."), this);
        actionCollection()->addAction("imagesize", action );
        action->setShortcut(QKeySequence(Qt::SHIFT+Qt::Key_S));
        connect(action, SIGNAL(triggered()), this, SLOT(slotImageSize()));

    action  = new KAction(i18n("Scale &Layer..."), this);
    actionCollection()->addAction("layersize", action );
        connect(action, SIGNAL(triggered()), this, SLOT(slotLayerSize()));

        m_view = (KisView2*) parent;
        // Selection manager takes ownership?
    action  = new KAction(i18n("&Scale Selection..."), this);
    actionCollection()->addAction("selectionscale", action );
        Q_CHECK_PTR(action);
        connect(action, SIGNAL(triggered()), this, SLOT(slotSelectionScale()));

        m_view ->selectionManager()->addSelectionAction(action);
    }
}

ImageSize::~ImageSize()
{
    m_view = 0;
}

void ImageSize::slotImageSize()
{
    KisImageSP image = m_view->image();

    if (!image) return;

    KisConfig cfg;

    DlgImageSize * dlgImageSize = new DlgImageSize(m_view, image->width(), image->height(), image->yRes());
    dlgImageSize->setObjectName("ImageSize");
    Q_CHECK_PTR(dlgImageSize);

    if (dlgImageSize->exec() == QDialog::Accepted) {
        qint32 w = dlgImageSize->width();
        qint32 h = dlgImageSize->height();

        if(w !=image->width() || h != image->height())
            m_view->imageManager()->scaleCurrentImage((double)w / ((double)(image->width())),
                        (double)h / ((double)(image->height())), dlgImageSize->filterType());
    }

    delete dlgImageSize;
}

void ImageSize::slotLayerSize()
{
    KisImageSP image = m_view->image();

    if (!image) return;

    DlgLayerSize * dlgLayerSize = new DlgLayerSize(m_view, "LayerSize");
    Q_CHECK_PTR(dlgLayerSize);

    dlgLayerSize->setCaption(i18n("Layer Size"));

    KisConfig cfg;

    KisPaintDeviceSP dev = image->activeDevice();
    QRect rc = dev->exactBounds();

    dlgLayerSize->setWidth(rc.width());
    dlgLayerSize->setHeight(rc.height());

    if (dlgLayerSize->exec() == QDialog::Accepted) {
        qint32 w = dlgLayerSize->width();
        qint32 h = dlgLayerSize->height();

        m_view->layerManager()->scaleLayer((double)w / ((double)(rc.width())),
                                           (double)h / ((double)(rc.height())),
                                           dlgLayerSize->filterType());
    }
    delete dlgLayerSize;
}

void ImageSize::slotSelectionScale()
{
    KisImageSP image = m_view->image();

    if (!image) return;

    KisPaintDeviceSP layer = image->activeDevice();

    if (!layer) return;

    if (!layer->hasSelection()) return;


    DlgLayerSize * dlgSize = new DlgLayerSize(m_view, "SelectionScale");
    Q_CHECK_PTR(dlgSize);

    dlgSize->setCaption(i18n("Scale Selection"));

    KisConfig cfg;

    QRect rc = layer->selection()->selectedRect();

    dlgSize->setWidth(rc.width());
    dlgSize->setHeight(rc.height());

    if (dlgSize->exec() == QDialog::Accepted) {
        qint32 w = dlgSize->width();
        qint32 h = dlgSize->height();
        KisTransformWorker worker(layer->selection().data(), 
                (double)w / ((double)(rc.width())),
                (double)h / ((double)(rc.height())),
                0, 0, 0.0, 0, 0, m_view->statusBar()->progress(), 
                dlgSize->filterType()
                );
        worker.run();
    }
    delete dlgSize;
}


#include "imagesize.moc"

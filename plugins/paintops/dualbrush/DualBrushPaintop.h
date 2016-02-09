/*
 *  Copyright (c) 2016 Boudewijn Rempt <boud@valdyas.org>
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

#ifndef KIS_DUALBRUSH_PAINTOP_H_
#define KIS_DUALBRUSH_PAINTOP_H_

#include <brushengine/kis_paintop.h>
#include <kis_types.h>

#include "DualBrush.h"
#include "DualBrushPaintopSettings.h"

class KisPainter;

class KisDualBrushPaintOp : public KisPaintOp
{

public:

    KisDualBrushPaintOp(const KisDualBrushPaintOpSettings *settings, KisPainter * painter, KisNodeSP node, KisImageSP image);
    virtual ~KisDualBrushPaintOp();

    KisSpacingInformation paintAt(const KisPaintInformation& info);

private:
    KisPaintDeviceSP m_dab;
    DualBrushBrush *m_DualBrushBrush;
    KisPressureOpacityOption m_opacityOption;
    DualBrushProperties m_properties;
};

#endif // KIS_DUALBRUSH_PAINTOP_H_

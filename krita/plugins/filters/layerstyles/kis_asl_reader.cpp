/*
 *  Copyright (c) 2015 Dmitry Kazakov <dimula73@gmail.com>
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

#include "kis_asl_reader.h"

#include "kis_dom_utils.h"

#include <stdexcept>
#include <string>

#include <QDomDocument>
#include <QIODevice>
#include <QBuffer>

#include "psd_utils.h"
#include "psd.h"
#include "compression.h"
#include "kis_offset_on_exit_verifier.h"

#include "kis_asl_writer_utils.h"

namespace Private {

/**
 * Default value for variabled read from a file
 */
#define GARBAGE_VALUE_MARK 999


/**
 * Exception that is emitted when any parse error appear.
 * Thanks to KisOffsetOnExitVerifier parsing can be continued
 * most of the time, based on the offset values written in PSD.
 */

struct ASLParseException : public std::runtime_error
{
    ASLParseException(const QString &msg)
        : std::runtime_error(msg.toAscii().data())
    {
    }
};

#define SAFE_READ_EX(device, varname)                                   \
    if (!psdread(device, &varname)) {                                   \
        QString msg = QString("Failed to read \'%1\' tag!").arg(#varname); \
        throw ASLParseException(msg);                                   \
    }

#define SAFE_READ_SIGNATURE_EX(device, varname, expected)               \
    if (!psdread(device, &varname) || varname != expected) {            \
        QString msg = QString("Failed to check signature \'%1\' tag!\n" \
                              "Value: \'%2\' Expected: \'%3\'")         \
            .arg(#varname).arg(varname).arg(expected);                  \
        throw ASLParseException(msg);                                   \
    }

/**
 * String fetch functions
 *
 * ASL has 4 types of strings:
 *
 * - fixed length (4 bytes)
 * - variable length (length (4 bytes) + string (var))
 * - pascal (length (1 byte) + string (var))
 * - unicode string (length (4 bytes) + null-terminated unicode string (var)
 */

QString readStringCommon(QIODevice *device, int length)
{
    QByteArray data;
    data.resize(length);
    qint64 dataRead = device->read(data.data(), length);

    if (dataRead != length) {
        QString msg =
            QString("Failed to read a string! "
                    "Bytes read: %1 Expected: %2")
            .arg(dataRead).arg(length);
        throw ASLParseException(msg);
    }

    return QString(data);
}

QString readFixedString(QIODevice *device) {
    return readStringCommon(device, 4);
}

QString readVarString(QIODevice *device) {
    quint32 length = 0;
    SAFE_READ_EX(device, length);

    if (!length) {
        length = 4;
    }

    return readStringCommon(device, length);
}

QString readPascalString(QIODevice *device) {
    quint8 length = 0;
    SAFE_READ_EX(device, length);

    return readStringCommon(device, length);
}

QString readUnicodeString(QIODevice *device) {
    QString string;

    if (!psdread_unicodestring(device, string)) {
        QString msg = QString("Failed to read a unicode string!");
        throw ASLParseException(msg);
    }

    return string;
}

/**
 * Numerical fetch functions
 *
 * We read numbers and convert them to strings to be able to store
 * them in XML.
 */

QString readDoubleAsString(QIODevice *device) {
    double value = 0.0;
    SAFE_READ_EX(device, value);

    return KisDomUtils::Private::numberToString(value);
}

QString readIntAsString(QIODevice *device) {
    quint32 value = 0.0;
    SAFE_READ_EX(device, value);

    return KisDomUtils::Private::numberToString(value);
}

QString readBoolAsString(QIODevice *device) {
    quint8 value = 0.0;
    SAFE_READ_EX(device, value);

    return KisDomUtils::Private::numberToString(value);
}

/**
 * XML generation functions
 *
 * Add a node and fill the corresponding attributes
 */

QDomElement appendXMLNodeCommon(const QString &key, const QString &value, const QString &type, QDomElement *parent, QDomDocument *doc)
{
    QDomElement el = doc->createElement("node");
    if (!key.isEmpty()) {
        el.setAttribute("key", key);
    }
    el.setAttribute("type", type);
    el.setAttribute("value", value);
    parent->appendChild(el);

    return el;
}

QDomElement appendXMLNodeCommonNoValue(const QString &key, const QString &type, QDomElement *parent, QDomDocument *doc)
{
    QDomElement el = doc->createElement("node");
    if (!key.isEmpty()) {
        el.setAttribute("key", key);
    }
    el.setAttribute("type", type);
    parent->appendChild(el);

    return el;
}

void appendIntegerXMLNode(const QString &key, const QString &value, QDomElement *parent, QDomDocument *doc)
{
    appendXMLNodeCommon(key, value, "Integer", parent, doc);
}

void appendDoubleXMLNode(const QString &key, const QString &value, QDomElement *parent, QDomDocument *doc)
{
    appendXMLNodeCommon(key, value, "Double", parent, doc);
}

void appendTextXMLNode(const QString &key, const QString &value, QDomElement *parent, QDomDocument *doc)
{
    appendXMLNodeCommon(key, value, "Text", parent, doc);
}

void appendPointXMLNode(const QString &key, const QPointF &pt, QDomElement *parent, QDomDocument *doc)
{
    QDomElement el = appendXMLNodeCommonNoValue(key, "Descriptor", parent, doc);
    el.setAttribute("classId", "CrPt");
    el.setAttribute("name", "");

    appendDoubleXMLNode("Hrzn", KisDomUtils::Private::numberToString(pt.x()), &el, doc);
    appendDoubleXMLNode("Vrtc", KisDomUtils::Private::numberToString(pt.x()), &el, doc);
}

/**
 * ASL -> XML parsing functions
 */

void readDescriptor(QIODevice *device,
                    const QString &key,
                    QDomElement *parent,
                    QDomDocument *doc);

void readChildObject(QIODevice *device,
                     QDomElement *parent,
                     QDomDocument *doc,
                     bool skipKey = false)
{
    QString key;

    if (!skipKey) {
        key = readVarString(device);
    }

    QString OSType = readFixedString(device);

    //qDebug() << "Child" << ppVar(key) << ppVar(OSType);

    if (OSType == "obj ") {
        qFatal("no implemented");

    } else if (OSType == "Objc" || OSType == "GlbO") {
        readDescriptor(device, key, parent, doc);

    } else if (OSType == "VlLs") {
        quint32 numItems = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(device, numItems);

        QDomElement el = appendXMLNodeCommonNoValue(key, "List", parent, doc);
        for (quint32 i = 0; i < numItems; i++) {
            readChildObject(device, &el, doc, true);
        }

    } else if (OSType == "doub") {
        appendDoubleXMLNode(key, readDoubleAsString(device), parent, doc);

    } else if (OSType == "UntF") {
        const QString unit = readFixedString(device);
        const QString value = readDoubleAsString(device);

        QDomElement el = appendXMLNodeCommon(key, value, "UnitFloat", parent, doc);
        el.setAttribute("unit", unit);

    } else if (OSType == "TEXT") {
        QString unicodeString = readUnicodeString(device);
        appendTextXMLNode(key, unicodeString, parent, doc);

    } else if (OSType == "enum") {
        const QString typeId = readVarString(device);
        const QString value = readVarString(device);

        QDomElement el = appendXMLNodeCommon(key, value, "Enum", parent, doc);
        el.setAttribute("typeId", typeId);

    } else if (OSType == "long") {
        appendIntegerXMLNode(key, readIntAsString(device), parent, doc);

    } else if (OSType == "bool") {
        const QString value = readBoolAsString(device);
        appendXMLNodeCommon(key, value, "Boolean", parent, doc);

    } else if (OSType == "type") {
        qFatal("no implemented");
    } else if (OSType == "GlbC") {
        qFatal("no implemented");
    } else if (OSType == "alis") {
        qFatal("no implemented");
    } else if (OSType == "tdta") {
        qFatal("no implemented");
    }
}

void readDescriptor(QIODevice *device,
                    const QString &key,
                    QDomElement *parent,
                    QDomDocument *doc)
{
    QString name = readUnicodeString(device);
    QString classId = readVarString(device);

    quint32 numChildren = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(device, numChildren);

    QDomElement el = appendXMLNodeCommonNoValue(key, "Descriptor", parent, doc);
    el.setAttribute("classId", classId);
    el.setAttribute("name", name);

    //qDebug() << "Descriptor" << ppVar(key) << ppVar(classId) << ppVar(numChildren);

    for (quint32 i = 0; i < numChildren; i++) {
        readChildObject(device, &el, doc);
    }
}

QImage readVirtualArrayList(QIODevice *device,
                            int numPlanes)
{
    quint32 arrayVersion = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(device, arrayVersion);

    if (arrayVersion != 3) {
        throw ASLParseException("VAList version is not '3'!");
    }

    quint32 arrayLength = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(device, arrayLength);

    SETUP_OFFSET_VERIFIER(vaEndVerifier, device, arrayLength, 100);

    quint32 x0, y0, x1, y1;
    SAFE_READ_EX(device, y0);
    SAFE_READ_EX(device, x0);
    SAFE_READ_EX(device, y1);
    SAFE_READ_EX(device, x1);
    QRect arrayRect(x0, y0, x1 - x0, y1 - y0);

    quint32 numberOfChannels = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(device, numberOfChannels);

    if (numberOfChannels != 24) {
        throw ASLParseException("VAList: Krita doesn't support ASL files with 'numberOfChannels' flag not equal to 24 (it is not documented)!");
    }

    // qDebug() << ppVar(arrayVersion);
    // qDebug() << ppVar(arrayLength);
    // qDebug() << ppVar(arrayRect);
    // qDebug() << ppVar(numberOfChannels);

    if (numPlanes != 1 && numPlanes != 3) {
        throw ASLParseException("VAList: unsupported number of planes!");
    }

    QVector<QByteArray> dataPlanes;
    dataPlanes.resize(3);

    for (int i = 0; i < numPlanes; i++) {
        quint32 arrayWritten = GARBAGE_VALUE_MARK;
        if (!psdread(device, &arrayWritten) || !arrayWritten) {
            throw ASLParseException("VAList plane has not-written flag set!");
        }

        quint32 arrayPlaneLength = GARBAGE_VALUE_MARK;
        if (!psdread(device, &arrayPlaneLength) || !arrayPlaneLength) {
            throw ASLParseException("VAList has plane length set to zero!");
        }

        SETUP_OFFSET_VERIFIER(planeEndVerifier, device, arrayPlaneLength, 0);
        qint64 nextPos = device->pos() + arrayPlaneLength;

        quint32 pixelDepth1 = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(device, pixelDepth1);

        quint32 x0, y0, x1, y1;
        SAFE_READ_EX(device, y0);
        SAFE_READ_EX(device, x0);
        SAFE_READ_EX(device, y1);
        SAFE_READ_EX(device, x1);
        QRect planeRect(x0, y0, x1 - x0, y1 - y0);

        if (planeRect != arrayRect) {
            throw ASLParseException("VAList: planes are not uniform. Not supported yet!");
        }

        quint16 pixelDepth2 = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(device, pixelDepth2);

        quint8 useCompression = 9;
        SAFE_READ_EX(device, useCompression);

        // qDebug() << "plane index:" << ppVar(i);
        // qDebug() << ppVar(arrayWritten);
        // qDebug() << ppVar(arrayPlaneLength);
        // qDebug() << ppVar(pixelDepth1);
        // qDebug() << ppVar(planeRect);
        // qDebug() << ppVar(pixelDepth2);
        // qDebug() << ppVar(useCompression);

        if (pixelDepth1 != pixelDepth2) {
            throw ASLParseException("VAList: two pixel depths of the plane are not equal (it is not documented)!");
        }

        if (pixelDepth1 != 8) {
            throw ASLParseException("VAList: supported pixel depth of the plane in 8 only!");
        }

        const int dataLength = planeRect.width() * planeRect.height();

        if (useCompression == Compression::Uncompressed) {
            dataPlanes[i] = device->read(dataLength);

        } else if (useCompression == Compression::RLE) {

            const int numRows = planeRect.height();

            QVector<quint16> rowSizes;
            rowSizes.resize(numRows);

            for (int row = 0; row < numRows; row++) {
                quint16 rowSize = GARBAGE_VALUE_MARK;
                SAFE_READ_EX(device, rowSize);
                rowSizes[row] = rowSize;
            }

            for (int row = 0; row < numRows; row++) {
                const quint16 rowSize = rowSizes[row];

                QByteArray compressedData = device->read(rowSize);

                if (compressedData.size() != rowSize) {
                    throw ASLParseException("VAList: failed to read compressed data!");
                }

                QByteArray uncompressedData = Compression::uncompress(planeRect.width(), compressedData, Compression::RLE);

                if (uncompressedData.size() != planeRect.width()) {
                    throw ASLParseException("VAList: failed to decompress data!");
                }

                dataPlanes[i].append(uncompressedData);
            }
        } else {
            throw ASLParseException("VAList: ZIP compression is not implemented yet!");
        }

        if (dataPlanes[i].size() != dataLength) {
            throw ASLParseException("VAList: failed to read/uncompress data plane!");
        }

        device->seek(nextPos);
    }

    QImage image(arrayRect.size(), QImage::Format_ARGB32);

    const int dataLength = arrayRect.width() * arrayRect.height();
    quint8 *dstPtr = image.bits();

    for (int i = 0; i < dataLength; i++) {
        for (int j = 2; j >= 0; j--) {
            int plane = qMin(numPlanes, j);
            *dstPtr++ = dataPlanes[plane][i];
        }
        *dstPtr++ = 0xFF;
    }

#if 0
     static int i = -1; i++;
     QString filename = QString("pattern_image_%1.png").arg(i);
     qDebug() << "### dumping pattern image" << ppVar(filename);
     image.save(filename);
#endif

    return image;
}

qint64 readPattern(QIODevice *device,
                   QDomElement *parent,
                   QDomDocument *doc)
{
    quint32 patternSize = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(device, patternSize);

    // patterns are always aligned by 4 bytes
    patternSize = KisAslWriterUtils::alignOffsetCeil(patternSize, 4);

    SETUP_OFFSET_VERIFIER(patternEndVerifier, device, patternSize, 0);

    quint32 patternVersion = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(device, patternVersion);

    if (patternVersion != 1) {
        throw ASLParseException("Pattern version is not \'1\'");
    }

    quint32 patternImageMode = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(device, patternImageMode);

    quint16 patternHeight = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(device, patternHeight);

    quint16 patternWidth = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(device, patternWidth);

    QString patternName;
    psdread_unicodestring(device, patternName);

    QString patternUuid = readPascalString(device);

    // qDebug() << "--";
    // qDebug() << ppVar(patternSize);
    // qDebug() << ppVar(patternImageMode);
    // qDebug() << ppVar(patternHeight);
    // qDebug() << ppVar(patternWidth);
    // qDebug() << ppVar(patternName);
    // qDebug() << ppVar(patternUuid);


    int numPlanes = 0;
    psd_color_mode mode = static_cast<psd_color_mode>(patternImageMode);

    switch (mode) {
    case MultiChannel:
    case Grayscale:
        numPlanes = 1;
        break;
    case RGB:
        numPlanes = 3;
        break;
    default: {
        QString msg = QString("Unsupported image mode: %1!").arg(mode);
        throw ASLParseException(msg);
    }
    }

    /**
     * Create XML data
     */

    QDomElement pat = doc->createElement("node");

    pat.setAttribute("classId", "KisPattern");
    pat.setAttribute("type", "Descriptor");
    pat.setAttribute("name", "");

    QBuffer patternBuf;
    patternBuf.open(QIODevice::WriteOnly);

    { // ensure we don't keep resources for too long
        QString fileName = QString("%1.pat").arg(patternUuid);
        QImage patternImage = readVirtualArrayList(device, numPlanes);
        KoPattern realPattern(patternImage, patternName, fileName);
        realPattern.saveToDevice(&patternBuf);
    }

    /**
     * We are loading the pattern and convert it into ARGB right away,
     * so we need not store real image mode and size of the pattern
     * externally.
     */
    appendTextXMLNode("Nm  ", patternName, &pat, doc);
    appendTextXMLNode("Idnt", patternUuid, &pat, doc);

    QDomCDATASection dataSection = doc->createCDATASection(qCompress(patternBuf.buffer()).toBase64());

    QDomElement dataElement = doc->createElement("node");
    dataElement.setAttribute("type", "KisPatternData");
    dataElement.setAttribute("key", "Data");

    dataElement.appendChild(dataSection);
    pat.appendChild(dataElement);
    parent->appendChild(pat);

    return sizeof(patternSize) + patternSize;
}

QDomDocument readFileImpl(QIODevice *device)
{
    QDomDocument doc;
    QDomElement root = doc.createElement("asl");
    doc.appendChild(root);


    {
        quint16 stylesVersion = GARBAGE_VALUE_MARK;
        SAFE_READ_SIGNATURE_EX(device, stylesVersion, 2);
    }

    {
        quint32 aslSignature = GARBAGE_VALUE_MARK;
        const quint32 refSignature = 0x3842534c; // '8BSL' in little-endian
        SAFE_READ_SIGNATURE_EX(device, aslSignature, refSignature);
    }

    {
        quint16 patternsVersion = GARBAGE_VALUE_MARK;
        SAFE_READ_SIGNATURE_EX(device, patternsVersion, 3);
    }

    // Patterns

    {
        quint32 patternsSize = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(device, patternsSize);

        if (patternsSize > 0) {

            SETUP_OFFSET_VERIFIER(patternsSectionVerifier, device, patternsSize, 0);

            QDomElement patternsRoot = doc.createElement("node");
            patternsRoot.setAttribute("type", "List");
            patternsRoot.setAttribute("key", "Patterns");
            root.appendChild(patternsRoot);

            try {
                qint64 bytesRead = 0;
                while (bytesRead < patternsSize) {
                    qint64 chunk = readPattern(device, &patternsRoot, &doc);
                    bytesRead += chunk;
                }
            } catch (ASLParseException &e) {
                qWarning() << "WARNING: ASL (emb. pattern):" << e.what();
            }
        }
    }

    // Styles

    quint32 numStyles = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(device, numStyles);

    quint32 bytesToRead = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(device, bytesToRead);

    {
        quint32 stylesFormatVersion = GARBAGE_VALUE_MARK;
        SAFE_READ_SIGNATURE_EX(device, stylesFormatVersion, 16);
    }

    readDescriptor(device, "", &root, &doc);

    {
        quint32 stylesFormatVersion = GARBAGE_VALUE_MARK;
        SAFE_READ_SIGNATURE_EX(device, stylesFormatVersion, 16);
    }

    readDescriptor(device, "", &root, &doc);

    return doc;
}

} // namespace

QDomDocument KisAslReader::readFile(QIODevice *device)
{
    QDomDocument doc;

    try {
        doc = Private::readFileImpl(device);
    } catch (Private::ASLParseException &e) {
        qWarning() << "WARNING: ASL:" << e.what();
    }

    return doc;
}

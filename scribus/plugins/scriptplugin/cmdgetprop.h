/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/
#ifndef CMDGETPROP_H
#define CMDGETPROP_H

// Pulls in <Python.h> first
#include "cmdvar.h"

/** Query-Functions */

/*! docstring */
PyDoc_STRVAR(scribus_getobjecttype__doc__,
QT_TR_NOOP("getObjectType([\"name\"]) -> string\n\
\n\
Get type of object \"name\" as a string.\n\
\n\
The possible return values are:\n\
\n\
'TextFrame', 'PathText', 'ImageFrame',\n\
'Line', 'Polygon', 'Polyline',\n\
'LatexFrame', 'OSGFrame', 'Symbol',\n\
'Group', 'RegularPolygon', 'Arc',\n\
'Spiral', 'Table', 'NoteFrame',\n\
'Multiple'\n\
"));
/** Get Object Type of name. */
PyObject *scribus_getobjecttype(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getfillcolor__doc__,
QT_TR_NOOP("getFillColor([\"name\"]) -> string\n\
\n\
Returns the name of the fill color of the object \"name\".\n\
If \"name\" is not given the currently selected item is used.\n\
"));
/*! Returns fill color of the object */
PyObject *scribus_getfillcolor(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getfillshade__doc__,
QT_TR_NOOP("getFillShade([\"name\"]) -> integer\n\
\n\
Returns the shading value of the fill color of the object \"name\".\n\
If \"name\" is not given the currently selected item is used.\n\
"));
/*! Returns fill shade of the object */
PyObject* scribus_getfillshade(PyObject* /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getfilltransparency__doc__,
QT_TR_NOOP("getFillTransparency([\"name\"]) -> float\n\
\n\
Returns the fill transparency of the object \"name\". If \"name\"\n\
is not given the currently selected Item is used.\n\
"));
/*! Returns fill transparency of the object */
PyObject *scribus_getfilltransparency(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getfillblendmode__doc__,
QT_TR_NOOP("getFillBlendmode([\"name\"]) -> integer\n\
\n\
Returns the fill blendmode of the object \"name\". If \"name\"\n\
is not given the currently selected Item is used.\n\
"));
/*! Returns fill blendmode of the object */
PyObject *scribus_getfillblendmode(PyObject * /*self*/, PyObject* args);

PyDoc_STRVAR(scribus_getgradstop__doc__,
QT_TR_NOOP("getGradientStop(index, [\"name\"]) -> (\"color\", opacity, shade)\n\
\n\
Returns a (\"color\", opacity, shade) tuple containing the stop at index on the gradient of the object\n\
\"name\". If \"name\" is not given the currently selected item is used.\n\
"));
PyObject* scribus_getgradstop(PyObject* /*self*/, PyObject* args);

PyDoc_STRVAR(scribus_getgradstopscount__doc__,
QT_TR_NOOP("getGradientStopsCount([\"name\"]) -> integer\n\
\n\
Returns the number of stops on the gradient of the object\n\
\"name\". If \"name\" is not given the currently selected item is used.\n\
"));
PyObject* scribus_getgradstopscount(PyObject* /*self*/, PyObject* args);

PyDoc_STRVAR(scribus_getgradvector__doc__,
QT_TR_NOOP("getGradientVector([\"name\"]) -> (startX, startY, endX, endY)\n\
\n\
Returns a (startX, startY, endX, endY) tuple containing the gradient vector of the object\n\
\"name\". If \"name\" is not given the currently selected item is used.\n\
"));
PyObject* scribus_getgradvector(PyObject* /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getcustomlinestyle__doc__,
QT_TR_NOOP("getCustomLineStyle([\"name\"]) -> string\n\
\n\
Returns the styleName of custom line style for the object. If object's \"name\" is not given the\n\
currently selected item is used.\n\
"));
/*! Returns custom style of the line */
PyObject *scribus_getcustomlinestyle(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getlinecolor__doc__,
QT_TR_NOOP("getLineColor([\"name\"]) -> string\n\
\n\
Returns the name of the line color of the object \"name\".\n\
If \"name\" is not given the currently selected item is used.\n\
"));
/*! Returns color of the line */
PyObject *scribus_getlinecolor(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getlinetransparency__doc__,
QT_TR_NOOP("getLineTransparency([\"name\"]) -> float\n\
\n\
Returns the line transparency of the object \"name\". If \"name\"\n\
is not given the currently selected Item is used.\n\
"));
/*! Returns line transparency of the object */
PyObject *scribus_getlinetransparency(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getlineblendmode__doc__,
QT_TR_NOOP("getLineBlendmode([\"name\"]) -> integer\n\
\n\
Returns the line blendmode of the object \"name\". If \"name\"\n\
is not given the currently selected Item is used.\n\
"));
/*! Returns line blendmode of the object */
PyObject *scribus_getlineblendmode(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getlinewidth__doc__,
QT_TR_NOOP("getLineWidth([\"name\"]) -> integer\n\
\n\
Returns the line width of the object \"name\". If \"name\"\n\
is not given the currently selected Item is used.\n\
"));
/*! Returns width of the line */
PyObject *scribus_getlinewidth(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getlineshade__doc__,
QT_TR_NOOP("getLineShade([\"name\"]) -> integer\n\
\n\
Returns the shading value of the line color of the object \"name\".\n\
If \"name\" is not given the currently selected item is used.\n\
"));
/*! Returns shading of the line */
PyObject *scribus_getlineshade(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getlinejoin__doc__,
QT_TR_NOOP("getLineJoin([\"name\"]) -> integer (see constants)\n\
\n\
Returns the line join style of the object \"name\". If \"name\" is not given\n\
the currently selected item is used.  The join types are:\n\
JOIN_BEVEL, JOIN_MITTER, JOIN_ROUND\n\
"));
/*! Returns join type of the line */
PyObject *scribus_getlinejoin(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getlinecap__doc__,
QT_TR_NOOP("getLineCap([\"name\"]) -> integer (see constants)\n\
\n\
Returns the line cap style of the object \"name\". If \"name\" is not given the\n\
currently selected item is used. The cap types are:\n\
CAP_FLAT, CAP_ROUND, CAP_SQUARE\n\
"));
/*! Returns cap type of the line */
PyObject *scribus_getlinecap(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getlinestyle__doc__,
QT_TR_NOOP("getLineStyle([\"name\"]) -> integer (see constants)\n\
\n\
Returns the line style of the object \"name\". If \"name\" is not given the\n\
currently selected item is used. Line style constants are:\n\
LINE_DASH, LINE_DASHDOT, LINE_DASHDOTDOT, LINE_DOT, LINE_SOLID\n\
"));
/*! Returns style type of the line */
PyObject *scribus_getlinestyle(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getcornerradius__doc__,
QT_TR_NOOP("getCornerRadius([\"name\"]) -> integer\n\
\n\
Returns the corner radius of the object \"name\". The radius is\n\
expressed in points. If \"name\" is not given the currently\n\
selected item is used.\n\
"));
/*! Returns corner radius of the object */
PyObject *scribus_getcornerradius(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getimagecolorspace__doc__,
QT_TR_NOOP("getImageColorSpace([\"name\"]) -> integer\n\
\n\
Returns the color space for the image loaded in image frame \"name\" as \n\
one of following integer constants: CSPACE_RGB (0), CSPACE_CMYK (1), \n\
CSPACE_GRAY (2), CSPACE_DUOTONE (3) or CSPACE_MONOCHROME (4).\n\
Returns CSPACE_UNDEFINED (-1) if no image is loaded in the frame.\n\
If \"name\" is not given the currently selected item is used.\n\
"));
PyObject *scribus_getimagecolorspace(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getimagefile__doc__,
QT_TR_NOOP("getImageFile([\"name\"]) -> string\n\
\n\
Returns the filename for the image in the image frame. If \"name\" is not\n\
given the currently selected item is used.\n\
"));
/*! Returns image name of the object */
PyObject *scribus_getimagefile(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getimageoffset__doc__,
	QT_TR_NOOP("getImageOffset([\"name\"]) -> (x,y)\n\
\n\
Returns a (x, y) tuple containing the offset values in point unit of the image\n\
frame \"name\".  If \"name\" is not given the currently selected item is used.\n\
"));
/*! Returns image scale of the object */
PyObject *scribus_getimageoffset(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getimagepage__doc__,
	QT_TR_NOOP("getImagePage([\"name\"]) -> (x,y)\n\
\n\
Return the page for multiple page images (like PDFs) in the image frame \"name\".\n\
0 means that the value is set to \"auto\".\n\
If \"name\" is not given the currently selected item is used.\n\
"));
/*! Returns the current of page of the object */
PyObject *scribus_getimagepage(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getimagepagecount__doc__,
	QT_TR_NOOP("getImagePageCount([\"name\"]) -> (x,y)\n\
\n\
Return the number of pages for multiple page images (like PDFs) in the image frame \"name\".\n\
If \"name\" is not given the currently selected item is used.\n\
"));
/*! Returns the number of pages of the object */
PyObject *scribus_getimagepagecount(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getimagepreviewresolution__doc__,
QT_TR_NOOP("getImagePreviewResolution([\"name\"]) -> integer (Scribus resolution constant)\n\
\n\
Gets preview resolution of the picture in the image frame \"name\".\n\
If \"name\" is not given the currently selected item is used.\n\
The returned value is one of:\n\
- IMAGE_PREVIEW_RESOLUTION_FULL,\n\
- IMAGE_PREVIEW_RESOLUTION_NORMAL,\n\
- IMAGE_PREVIEW_RESOLUTION_LOW,\n\
\n\
May raise WrongFrameTypeError if the target frame is not an image frame.\n\
"));
/*! Returns image preview resolution of the object */
PyObject *scribus_getimagepreviewresolution(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getimagescale__doc__,
	QT_TR_NOOP("getImageScale([\"name\"]) -> (x,y)\n\
\n\
Returns a (x, y) tuple containing the scaling values of the image frame\n\
\"name\".  If \"name\" is not given the currently selected item is used.\n\
"));
/*! Returns image scale of the object */
PyObject *scribus_getimagescale(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getposition__doc__,
QT_TR_NOOP("getPosition([\"name\"]) -> (x,y)\n\
\n\
Returns a (x, y) tuple with the position of the object \"name\".\n\
If \"name\" is not given the currently selected item is used.\n\
The position is expressed in the actual measurement unit of the document\n\
- see UNIT_<type> for reference.\n\
"));
/*! Returns position of the object */
PyObject *scribus_getposition(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getsize__doc__,
QT_TR_NOOP("getSize([\"name\"]) -> (width,height)\n\
\n\
Returns a (width, height) tuple with the size of the object \"name\".\n\
If \"name\" is not given the currently selected item is used. The size is\n\
expressed in the current measurement unit of the document - see UNIT_<type>\n\
for reference.\n\
"));
/*! Returns size of the object */
PyObject *scribus_getsize(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getrotation__doc__,
QT_TR_NOOP("getRotation([\"name\"]) -> integer\n\
\n\
Returns the rotation of the object \"name\". The value is expressed in degrees,\n\
and clockwise is positive. If \"name\" is not given the currently selected item\n\
is used.\n\
"));
/*! Returns rotation of the object */
PyObject *scribus_getrotation(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getboundingbox__doc__,
QT_TR_NOOP("getBoundingBox([\"name\"]) -> (x, y, width, height)\n\
\n\
Returns a (x, y, width, height) tuple with the position and size of the object \"name\".\n\
If \"name\" is not given the currently selected item is used. The size is\n\
expressed in the current measurement unit of the document - see UNIT_<type>\n\
for reference.\n\
"));
/*! Returns size of the object */
PyObject *scribus_getboundingbox(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getvisualboundingbox__doc__,
QT_TR_NOOP("getVisualBoundingBox([\"name\"]) -> (x, y, width, height)\n\
\n\
Returns a (x, y, width, height) tuple corresponding to the visual bounding box\n\
of the object \"name\".\n\
If \"name\" is not given the currently selected item is used. The size is\n\
expressed in the current measurement unit of the document - see UNIT_<type>\n\
for reference.\n\
"));
/*! Returns size of the object */
PyObject *scribus_getvisualboundingbox(PyObject * /*self*/, PyObject* args);

/*! docstring */
PyDoc_STRVAR(scribus_getallobjects__doc__,
QT_TR_NOOP("getAllObjects([type, page, \"layer\"]) -> list\n\
\n\
Returns a list containing the names of all objects of specified type and located\n\
on specified page and/or layer.\n\
This function accepts several optional keyword arguments:\n\
- type (optional): integer corresponding to item type, by default all items will be returned.\n\
- page (optional): index of page on which returned objects are located, by default the current page.\n\
         The page index starts at 0 and goes to the total number of pages - 1.\n\
- layer (optional): name of layer on which returned objects are located, by default\n\
         the function returns items located on all layers.\n\
May throw ValueError if page index or layer name is invalid.\n\
"));
/*! Returns a list with all objects in page */
PyObject *scribus_getallobjects(PyObject * /*self*/, PyObject* args, PyObject *keywds);

/*! docstring */
PyDoc_STRVAR(scribus_getobjectattributes__doc__,
QT_TR_NOOP("getObjectAttributes([\"name\"]) -> list\n\
\n\
Returns a list containing all attributes of object \"name\".\n\
"));
PyObject *scribus_getobjectattributes(PyObject * /*self*/, PyObject* args);

#endif


/* Copyright (C) 2003-2013 Runtime Revolution Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "dispatch.h"
#include "image.h"
#include "stack.h"
#include "util.h"

#include "globals.h"

#include "mctheme.h"

#include "context.h"
#include "metacontext.h"
#include "bitmapeffect.h"
#include "path.h"

inline bool operator != (const MCColor& a, const MCColor& b)
{
	return a . red != b . red || a . green != b . green || a . blue != b . blue;
}

MCMetaContext::MCMetaContext(const MCRectangle& p_page)
{
	f_quality = QUALITY_DEFAULT;
	f_function = GXcopy;
	f_opacity = 255;
	f_clip = p_page;
	f_font = NULL;
	f_stroke = NULL;
	f_fill_foreground = NULL;
	f_fill_background = NULL;
	f_stroke_used = false;
	f_fill_foreground_used = false;
	f_fill_background_used = false;
	f_state_stack = NULL;
	begin(true);
}

MCMetaContext::~MCMetaContext(void)
{
	while(f_state_stack != NULL)
		end();
}


void MCMetaContext::begin(bool p_overlap)
{
	if (!p_overlap && f_opacity == 255 && (f_function == GXcopy || f_function == GXblendSrcOver) && f_state_stack -> root -> group . effects == NULL)
	{
		f_state_stack -> nesting += 1;
		return;
	}

	MCMarkState *t_new_state;
	t_new_state = new MCMarkState;
	
	MCMark *t_root_mark;
	t_root_mark = NULL;
	if (t_new_state != NULL)
		t_root_mark = new_mark(MARK_TYPE_GROUP, false, false);
		
	if (t_root_mark != NULL)
	{
		t_root_mark -> group . head = NULL;
		t_root_mark -> group . tail = NULL;
		t_root_mark -> group . opacity = f_opacity;
		t_root_mark -> group . function = f_function;
		t_root_mark -> group . effects = NULL;
		
		t_new_state -> parent = f_state_stack;
		t_new_state -> nesting = 0;
		t_new_state -> root = t_root_mark;

		f_state_stack = t_new_state;
	}
	else
	{
		f_state_stack -> nesting += 1;
		
		if (t_new_state != NULL)
			delete t_new_state;
	}
}

bool MCMetaContext::begin_with_effects(MCBitmapEffectsRef p_effects, const MCRectangle& p_shape)
{
	// Compute the region of the shape required to correctly render the given
	// clip.
	MCRectangle t_layer_clip;
	MCBitmapEffectsComputeClip(p_effects, p_shape, getclip(), t_layer_clip);

	// If the clip is empty, return.
	if (t_layer_clip . width == 0 || t_layer_clip . height == 0)
		return false;

	// Begin a new layer - making sure it won't be nested.
	begin(true);

	// If we didn't manage to create a new layer, then we are done.
	if (f_state_stack -> nesting != 0)
		return true;

	// Add in the effects
	f_state_stack -> root -> group . effects = p_effects;
	f_state_stack -> root -> group . effects_shape = p_shape;

	// Set the clip to be used by the source - note that this is different
	// from the clip stored in the mask because that's the region of the
	// destination we want to render.
	setclip(t_layer_clip);

	return true;
}

void MCMetaContext::end(void)
{
	assert(f_state_stack != NULL);

	if (f_state_stack -> nesting > 0)
	{
		f_state_stack -> nesting -= 1;
		return;
	}

	MCMarkState *t_state;
	t_state = f_state_stack;
	f_state_stack = f_state_stack -> parent;
	delete t_state;
}

MCContextType MCMetaContext::gettype(void) const
{
	return CONTEXT_TYPE_PRINTER;
}

bool MCMetaContext::changeopaque(bool p_new_value)
{
	return false;
}

void MCMetaContext::setprintmode(void)
{
}

void MCMetaContext::setclip(const MCRectangle& rect)
{
	f_clip = rect;
}

const MCRectangle& MCMetaContext::getclip(void) const
{
	return f_clip;
}

void MCMetaContext::clearclip(void)
{
	f_clip = f_state_stack -> root -> clip;
}

void MCMetaContext::setorigin(int2 x, int2 y)
{
	assert(false);
}

void MCMetaContext::clearorigin(void)
{
	assert(false);
}

void MCMetaContext::setquality(uint1 quality)
{
}

void MCMetaContext::setfunction(uint1 function)
{
	f_function = function;
}

uint1 MCMetaContext::getfunction(void)
{
	return f_function;
}

void MCMetaContext::setopacity(uint1 opacity)
{
	f_opacity = opacity;
}

uint1 MCMetaContext::getopacity(void)
{
	return f_opacity;
}

void MCMetaContext::setforeground(const MCColor& c)
{
	if (f_fill_foreground == NULL || c != f_fill_foreground -> colour)
	{
		new_fill_foreground();
		
		if (f_fill_foreground != NULL)
			f_fill_foreground -> colour = c;
	}
}

void MCMetaContext::setbackground(const MCColor& c)
{
	if (f_fill_background == NULL || c != f_fill_background -> colour)
	{
		new_fill_background();
	
		if (f_fill_background != NULL)
			f_fill_background -> colour = c;
	}
}

void MCMetaContext::setdashes(uint2 offset, const uint1 *dashes, uint2 ndashes)
{
	new_stroke();
	
	if (f_stroke != NULL)
	{
		f_stroke -> dash . offset = offset;
		f_stroke -> dash . length = ndashes;
		f_stroke -> dash . data = new_array<uint1>(ndashes);
		memcpy(f_stroke -> dash . data, dashes, ndashes);
	}
}

void MCMetaContext::setfillstyle(uint2 style, Pixmap p, int2 x, int2 y)
{
	if (f_fill_foreground == NULL || style != f_fill_foreground -> style || p != f_fill_foreground -> pattern || x != f_fill_foreground -> origin . x || y != f_fill_foreground -> origin . y)
	{
		new_fill_foreground();
	
		if (f_fill_foreground != NULL)
		{
			if (style != FillTiled || p != NULL)
			{
				f_fill_foreground -> style = style;
				f_fill_foreground -> pattern = p;
				f_fill_foreground -> origin . x = x;
				f_fill_foreground -> origin . y = y;
			}
			else
			{
				f_fill_foreground -> style = FillSolid;
				f_fill_foreground -> pattern = p;
				f_fill_foreground -> origin . x = 0;
				f_fill_foreground -> origin . y = 0;
			}
		}
	}
}

void MCMetaContext::getfillstyle(uint2& style, Pixmap& p, int2& x, int2& y)
{
	if (f_fill_foreground != NULL)
	{
		style = f_fill_foreground -> style;
		p = f_fill_foreground -> pattern;
		x = f_fill_foreground -> origin . x;
		y = f_fill_foreground -> origin . y;
	}
	else
	{
		style = FillSolid;
		p = NULL;
		x = 0;
		y = 0;
	}
}

void MCMetaContext::setgradient(MCGradientFill *p_gradient)
{
	if (f_fill_foreground == NULL || p_gradient != f_fill_foreground -> gradient)
	{
		new_fill_foreground();
		
		if (f_fill_foreground != NULL)
		{
			f_fill_foreground -> gradient = p_gradient;
		}
	}
}

void MCMetaContext::setlineatts(uint2 linesize, uint2 linestyle, uint2 capstyle, uint2 joinstyle)
{
	if (f_stroke == NULL || linesize != f_stroke -> width || linestyle != f_stroke -> style || capstyle != f_stroke -> cap || joinstyle != f_stroke -> join)
		new_stroke();
	
	if (f_stroke != NULL)
	{
		f_stroke -> width = linesize;
		f_stroke -> style = linestyle;
		f_stroke -> cap = capstyle;
		f_stroke -> join = joinstyle;
	}
}

void MCMetaContext::setmiterlimit(real8 p_limit)
{
	if (f_stroke == NULL || p_limit != f_stroke->miter_limit)
		new_stroke();

	if (f_stroke != NULL)
	{
		f_stroke->miter_limit = p_limit;
	}
}

void MCMetaContext::drawline(int2 x1, int2 y1, int2 x2, int2 y2)
{
	MCMark *t_mark;
	
	t_mark = new_mark(MARK_TYPE_LINE, true, false);
	if (t_mark != NULL)
	{
		t_mark -> line . start . x = x1;
		t_mark -> line . start . y = y1;
		t_mark -> line . end . x = x2;
		t_mark -> line . end . y = y2;
	}
}

void MCMetaContext::drawlines(MCPoint *points, uint2 npoints, bool p_closed)
{
	polygon_mark(true, false, points, npoints, p_closed);
}

void MCMetaContext::drawsegments(MCSegment *segments, uint2 nsegs)
{
	for(uint2 t_segment = 0; t_segment < nsegs; ++t_segment)
		drawline(segments[t_segment] . x1, segments[t_segment] . y1, segments[t_segment] . x2, segments[t_segment] . y2);
}

void MCMetaContext::drawtext(int2 x, int2 y, const char *s, uint2 length, MCFontStruct *f, Boolean image, bool p_unicode_override)
{
	// MW-2009-12-22: Make sure we don't generate 0 length text mark records
	if (length == 0)
		return;
	
	MCMark *t_mark;
	t_mark = new_mark(MARK_TYPE_TEXT, false, true);
	if (t_mark != NULL)
	{
		// MW-2012-02-28: [[ Bug ]] Use the fontstruct from the parameter as 'setfont()' is no
		//   longer used.
		t_mark -> text . font = f;
		t_mark -> text . position . x = x;
		t_mark -> text . position . y = y;
		if (image && f_fill_background != NULL)
		{
			t_mark -> text . background = f_fill_background;
			f_fill_background_used = true;
		}
		else
			t_mark -> text . background = NULL;
		t_mark -> text . data = new_array<char>(length);
		t_mark -> text . length = length;
		if (t_mark -> text . data != NULL)
			memcpy(t_mark -> text . data, s, length);
		t_mark -> text . unicode_override = p_unicode_override;
	}
}

void MCMetaContext::drawrect(const MCRectangle& rect)
{
	rectangle_mark(true, false, rect);
}
	
void MCMetaContext::fillrect(const MCRectangle& rect)
{
	rectangle_mark(false, true, rect); 
}

void MCMetaContext::fillrects(MCRectangle *rects, uint2 nrects)
{
	for(uint4 t_index = 0; t_index < nrects; ++t_index)
		rectangle_mark(false, true, rects[t_index]);
}

void MCMetaContext::fillpolygon(MCPoint *points, uint2 npoints)
{
	polygon_mark(false, true, points, npoints, true);
}

void MCMetaContext::drawroundrect(const MCRectangle& rect, uint2 radius)
{
	round_rectangle_mark(true, false, rect, radius);
}

void MCMetaContext::fillroundrect(const MCRectangle& rect, uint2 radius)
{
	round_rectangle_mark(false, true, rect, radius);
}

void MCMetaContext::drawarc(const MCRectangle& rect, uint2 start, uint2 angle)
{
	arc_mark(true, false, rect, start, angle, false);
}

void MCMetaContext::drawsegment(const MCRectangle& rect, uint2 start, uint2 angle)
{
	arc_mark(true, false, rect, start, angle, true);
}

void MCMetaContext::fillarc(const MCRectangle& rect, uint2 start, uint2 angle)
{
	arc_mark(false, true, rect, start, angle, true);
}


void MCMetaContext::drawpath(MCPath *path)
{
	path_mark(false, true, path, false);
}

void MCMetaContext::fillpath(MCPath *path, bool p_evenodd)
{
	path_mark(true, false, path, p_evenodd);
}


void MCMetaContext::draweps(real8 sx, real8 sy, int2 angle, real8 xscale, real8 yscale, int2 tx, int2 ty, const char *prolog, const char *psprolog, uint4 psprologlength, const char *ps, uint4 length, const char *fontname, uint2 fontsize, uint2 fontstyle, MCFontStruct *font, const MCRectangle& trect)
{
}

void MCMetaContext::drawpict(uint1 *data, uint4 length, bool embed, const MCRectangle& drect, const MCRectangle& crect)
{
	MCMark *t_mark;
	t_mark = new_mark(MARK_TYPE_METAFILE, false, false);
	if (t_mark != NULL)
	{
		if (embed)
		{
			uint1 *t_new_data;
			t_new_data = new_array<uint1>(length);
			memcpy(t_new_data, data, length);

			t_mark -> metafile . data = t_new_data;
			t_mark -> metafile . data_length = length;
			t_mark -> metafile . embedded = true;
		}
		else
		{
			t_mark -> metafile . data = data;
			t_mark -> metafile . data_length = length;
			t_mark -> metafile . embedded = false;
		}
		t_mark -> metafile . src_area = crect;
		t_mark -> metafile . dst_area = drect;
	}
}

void MCMetaContext::drawimage(const MCImageDescriptor& p_image, int2 sx, int2 sy, uint2 sw, uint2 sh, int2 dx, int2 dy)
{
	MCMark *t_mark;
	bool t_need_group;

	t_need_group = (MCImageBitmapHasTransparency(p_image . bitmap) || f_function != GXcopy || f_opacity != 255);

	MCRectangle t_old_clip;
	t_old_clip = getclip();
	
	MCRectangle t_clip;
	MCU_set_rect(t_clip, dx, dy, sw, sh);
	t_clip = MCU_intersect_rect(t_clip, f_clip);
	setclip(t_clip);

	if (t_need_group)
		begin(true);
		
	t_mark = new_mark(MARK_TYPE_IMAGE, false, false);
	if (t_mark != NULL)
	{
		t_mark -> image . descriptor = p_image;
		t_mark -> image . sx = sx;
		t_mark -> image . sy = sy;
		t_mark -> image . sw = sw;
		t_mark -> image . sh = sh;
		t_mark -> image . dx = dx;
		t_mark -> image . dy = dy;
	}
	
	if (t_need_group)
		end();

	setclip(t_old_clip);
}

void MCMetaContext::drawlink(const char *p_link, const MCRectangle& p_region)
{
	MCMark *t_mark;
	t_mark = new_mark(MARK_TYPE_LINK, false, false);
	if (t_mark != NULL)
	{
		t_mark -> link . region = p_region;
		t_mark -> link . text = new_array<char>(strlen(p_link) + 1);
		if (t_mark -> link . text != NULL)
			strcpy(t_mark -> link . text, p_link);
	}
}

int4 MCMetaContext::textwidth(MCFontStruct *f, const char *s, uint2 l, bool p_unicode_override)
{
	return MCscreen->textwidth(f, s, l, p_unicode_override);
}

void MCMetaContext::applywindowshape(MCWindowShape *p_mask, unsigned int p_update_width, unsigned int p_update_height)
{
	assert(false);
}


void MCMetaContext::drawtheme(MCThemeDrawType type, MCThemeDrawInfo* p_info)
{
	MCMark *t_mark;

	begin(true);
	
	t_mark = new_mark(MARK_TYPE_THEME, false, false);
	if (t_mark != NULL)
	{
		t_mark -> theme . type = type;

		// MW-2011-09-14: [[ Bug 9719 ]] Query the current MCTheme object for the MCThemeDrawInfo size.
		memcpy(&t_mark -> theme . info, p_info, MCcurtheme != nil ? MCcurtheme -> getthemedrawinfosize() : 0);
	}
	
	end();
}

/* OVERHAUL - REVISIT - change source parameter */
void MCMetaContext::copyarea(Drawable p_src, uint4 p_dx, uint4 p_dy, uint4 p_sx, uint4 p_sy, uint4 p_sw, uint4 p_sh)
{
	MCImageDescriptor t_image;
	memset(&t_image, 0, sizeof(MCImageDescriptor));
	
	uint2 w, h, d;
	MCscreen -> getpixmapgeometry(p_src, w, h, d);

	MCBitmap *t_old_bitmap = nil;
	MCImageBitmap *t_bitmap = nil;
	/* UNCHECKED */ t_old_bitmap = MCscreen->getimage(p_src, 0, 0, w, h);
	/* UNCHECKED */ MCImageBitmapCreateWithOldBitmap(t_old_bitmap, t_bitmap);
	MCImageBitmapSetAlphaValue(t_bitmap, 0xFF);

	t_image . bitmap = t_bitmap;

	drawimage(t_image, p_sx, p_sy, p_sw, p_sh, p_dx, p_dy);

	MCImageFreeBitmap(t_bitmap);
	if (t_old_bitmap != nil)
		MCscreen -> destroyimage(t_old_bitmap);
}

void MCMetaContext::combine(Pixmap p_src, int4 p_dx, int4 p_dy, int4 p_sx, int4 p_sy, uint4 p_sw, uint4 p_sh)
{
}

MCBitmap *MCMetaContext::lock(void)
{
	assert(false);
	return NULL;
}

void MCMetaContext::unlock(MCBitmap *)
{
	assert(false);
}

void MCMetaContext::clear(const MCRectangle *rect)
{
	assert(false);
}

MCRegionRef MCMetaContext::computemaskregion(void)
{
	assert(false);
	return NULL;
}


uint2 MCMetaContext::getdepth(void) const
{
	return 32;
}

const MCColor& MCMetaContext::getblack(void) const
{
	return MCscreen -> black_pixel;
}

const MCColor& MCMetaContext::getwhite(void) const
{
	return MCscreen -> white_pixel;
}

const MCColor& MCMetaContext::getgray(void) const
{
	return MCscreen -> gray_pixel;
}

const MCColor& MCMetaContext::getbg(void) const
{
	if (getdepth() == 32)
		return MCscreen -> background_pixel;
	
	return MCscreen -> white_pixel;
}

static bool mark_indirect(MCContext *p_context, MCMark *p_mark, MCMark *p_upto_mark, const MCRectangle& p_clip)
{
	MCRectangle t_clip;
	t_clip = MCU_intersect_rect(p_mark -> clip, p_clip);
	
	if (t_clip . width == 0 || t_clip . height == 0)
		return false;

	bool t_finished;
	t_finished = false;

	if (p_mark == p_upto_mark)
		t_finished = true;
		
	if (p_mark -> type == MARK_TYPE_GROUP)
	{
		p_context -> setopacity(p_mark -> group . opacity);
		p_context -> setfunction(p_mark -> group . function);
		p_context -> setclip(t_clip);

		bool t_have_layer;
		t_have_layer = true;

		if (p_mark -> group . effects == NULL)
			p_context -> begin(true);
		else
			t_have_layer = p_context -> begin_with_effects(p_mark -> group . effects, p_mark -> group . effects_shape);

		if (t_have_layer)
		{
			p_context -> setopacity(255);
			p_context -> setfunction(GXcopy);

			for(MCMark *t_group_mark = p_mark -> group . head; t_group_mark != NULL; t_group_mark = t_group_mark -> next)
			{
				t_finished = mark_indirect(p_context, t_group_mark, p_upto_mark, t_clip);
				if (t_finished)
					break;
			}
		
			p_context -> end();
		}
	}
	else
	{
		if (p_mark -> stroke != NULL)
		{
			if (p_mark -> stroke -> dash . length != 0)
				p_context -> setdashes(p_mark -> stroke -> dash . offset, p_mark -> stroke -> dash . data, p_mark -> stroke -> dash . length);
			p_context -> setlineatts(p_mark -> stroke -> width, p_mark -> stroke -> style, p_mark -> stroke -> cap, p_mark -> stroke -> join);
		}
		
		if (p_mark -> fill != NULL)
		{
			p_context -> setforeground(p_mark -> fill -> colour);
			p_context -> setfillstyle(p_mark -> fill -> style, p_mark -> fill -> pattern, p_mark -> fill -> origin . x, p_mark -> fill -> origin . y);

			p_context->setgradient(p_mark->fill->gradient);	
		}
		
		if (p_mark->fill != NULL && p_mark->fill->gradient != NULL)
			p_context->setquality(QUALITY_SMOOTH);
		else
			p_context->setquality(QUALITY_DEFAULT);

		p_context -> setclip(t_clip);
	
		switch(p_mark -> type)
		{
			case MARK_TYPE_LINE:
				p_context -> drawline(p_mark -> line . start . x, p_mark -> line . start . y, p_mark -> line . end . x, p_mark -> line . end . y);
			break;
			
			case MARK_TYPE_POLYGON:
				if (p_mark -> stroke != NULL)
					p_context -> drawlines(p_mark -> polygon . vertices, p_mark -> polygon . count);
				else
					p_context -> fillpolygon(p_mark -> polygon . vertices, p_mark -> polygon . count);
			break;
			
			case MARK_TYPE_TEXT:
				if (p_mark -> text . background != NULL)
					p_context -> setbackground(p_mark -> text . background -> colour);
				p_context -> drawtext(p_mark -> text . position . x, p_mark -> text . position . y, (const char *)p_mark -> text . data, p_mark -> text . length, p_mark -> text . font, p_mark -> text . background != NULL, p_mark -> text . unicode_override);
			break;
			
			case MARK_TYPE_RECTANGLE:
				if (p_mark -> stroke != NULL)
					p_context -> drawrect(p_mark -> rectangle . bounds);
				else
					p_context -> fillrect(p_mark -> rectangle . bounds);
			break;
			
			case MARK_TYPE_ROUND_RECTANGLE:
				if (p_mark -> stroke != NULL)
					p_context -> drawroundrect(p_mark -> round_rectangle . bounds, p_mark -> round_rectangle . radius);
				else
					p_context -> fillroundrect(p_mark -> round_rectangle . bounds, p_mark -> round_rectangle . radius);
			break;
			
			case MARK_TYPE_ARC:
				if (p_mark -> stroke != NULL)
				{
					if (p_mark -> arc . complete)
						p_context -> drawsegment(p_mark -> arc . bounds, p_mark -> arc . start, p_mark -> arc . angle);
					else
						p_context -> drawarc(p_mark -> arc . bounds, p_mark -> arc . start, p_mark -> arc . angle);
				}
				else
					p_context -> fillarc(p_mark -> arc . bounds, p_mark -> arc . start, p_mark -> arc . angle);
			break;
			
			case MARK_TYPE_IMAGE:
				p_context -> drawimage(p_mark -> image . descriptor, p_mark -> image . sx, p_mark -> image . sy, p_mark -> image . sw, p_mark -> image . sh, p_mark -> image . dx, p_mark -> image . dy);
			break;
			
			case MARK_TYPE_METAFILE:
				p_context -> drawpict((uint1 *)p_mark -> metafile . data, p_mark -> metafile . data_length, false, p_mark -> metafile . dst_area, p_mark -> metafile . src_area);
			break;
			
			case MARK_TYPE_THEME:
				p_context -> drawtheme(p_mark -> theme . type, (MCThemeDrawInfo*)&p_mark -> theme . info);
			break;

			case MARK_TYPE_PATH:
			{
				MCPath *t_path;
				t_path = MCPath::create_path(p_mark -> path . commands, p_mark -> path . command_count, p_mark -> path . ordinates, p_mark -> path . ordinate_count);
				if (p_mark -> stroke != NULL)
					p_context -> drawpath(t_path);
				else
					p_context -> fillpath(t_path, p_mark -> path . evenodd);
				t_path -> release();
			}
			break;
		}
	}

	return t_finished;
}

void MCMetaContext::execute(void)
{
	executegroup(f_state_stack -> root);
}

void MCMetaContext::executegroup(MCMark *p_group_mark)
{
	for(MCMark *t_mark = p_group_mark -> group . head; t_mark != NULL; t_mark = t_mark -> next)
	{
		if (!candomark(t_mark))
		{
			MCContext *t_context;
			t_context = nil;

			// Compute the region of the destination we need to rasterize.
			MCRectangle t_dst_clip;
			if (t_mark -> type != MARK_TYPE_GROUP || t_mark -> group . effects == NULL)
				t_dst_clip = t_mark -> clip;
			else
				MCBitmapEffectsComputeBounds(t_mark -> group . effects, t_mark -> group . effects_shape, t_dst_clip);

			// Get a raster context for the given clip - but only if there is something to
			// render.
			if (t_dst_clip . width != 0 && t_dst_clip . height != 0)
				t_context = begincomposite(t_dst_clip);
			
			if (t_context != nil)
			{
				// First render just the group we are interested in, so we can clip out any pixels not
				// affected by it.
				mark_indirect(t_context, t_mark, nil, t_dst_clip);

				// Compute the region touched by non-transparent pixels.
				MCRegionRef t_clip_region;
				t_clip_region = t_context -> computemaskregion();
				t_context -> clear(nil);

				// MW-2007-11-28: [[ Bug 4873 ]] Failure to reset the context state here means the first
				//   objects rendered up until a group are all wrong!
				t_context -> setfunction(GXcopy);
				t_context -> setopacity(255);
				t_context -> clearclip();

				// Render all marks from the bottom up to and including the current mark - clipped
				// by the dst bounds.
				for(MCMark *t_raster_mark = f_state_stack -> root -> group . head; t_raster_mark != t_mark -> next; t_raster_mark = t_raster_mark -> next)
					if (mark_indirect(t_context, t_raster_mark, t_mark, t_dst_clip))
						break;
				
				endcomposite(t_context, t_clip_region);
			}
		}
		else
			domark(t_mark);
	}
}

void MCMetaContext::new_fill_foreground(void)
{
	if (f_fill_foreground_used || f_fill_foreground == NULL)
	{
		f_fill_foreground = f_heap . allocate<MCMarkFill>();
		if (f_fill_foreground != NULL)
		{
			f_fill_foreground -> style = FillSolid;
			f_fill_foreground -> colour = getblack();
			f_fill_foreground -> pattern = NULL;
			f_fill_foreground -> origin . x = 0;
			f_fill_foreground -> origin . y = 0;
			f_fill_foreground -> gradient = NULL;
		}
		
		f_fill_foreground_used = false;
	}
}

void MCMetaContext::new_fill_background(void)
{
	if (f_fill_background_used || f_fill_background == NULL)
	{
		f_fill_background = f_heap . allocate<MCMarkFill>();
		if (f_fill_background != NULL)
		{
			f_fill_background -> style = FillSolid;
			f_fill_background -> colour = getwhite();
			f_fill_background -> pattern = NULL;
			f_fill_background -> origin . x = 0;
			f_fill_background -> origin . y = 0;
			f_fill_background -> gradient = NULL;
		}
		
		f_fill_background_used = false;
	}
}

void MCMetaContext::new_stroke(void)
{
	if (f_stroke_used || f_stroke == NULL)
	{
		f_stroke = f_heap . allocate<MCMarkStroke>();
		if (f_stroke != NULL)
		{
			f_stroke -> width = 0;
			f_stroke -> style = LineSolid;
			f_stroke -> cap = CapButt;
			f_stroke -> join = JoinBevel;
			f_stroke -> dash . data = NULL;
			f_stroke -> dash . length = 0;
			f_stroke -> dash . offset = 0;
			f_stroke -> miter_limit = 10.0;
		}
		
		f_stroke_used = false;
	}
}

template<typename T> T *MCMetaContext::new_array(uint4 p_size)
{
	return f_heap . heap_t::allocate_array<T>(p_size);
}

MCMark *MCMetaContext::new_mark(uint4 p_type, bool p_stroke, bool p_fill)
{
	static uint4 s_mark_sizes[] =
	{
		0,
		sizeof(MCMarkLine),
		sizeof(MCMarkPolygon),
		sizeof(MCMarkText),
		sizeof(MCMarkRectangle),
		sizeof(MCMarkRoundRectangle),
		sizeof(MCMarkArc),
		sizeof(MCMarkImage),
		sizeof(MCMarkMetafile),
		0,
		sizeof(MCMarkTheme),
		sizeof(MCMarkGroup),
		sizeof(MCMarkLink),
		sizeof(MCMarkPath),
	};
	
	MCMark *t_mark;
	t_mark = f_heap . allocate<MCMark>(sizeof(MCMarkHeader) + s_mark_sizes[p_type]);
	if (t_mark != NULL)
	{
		if (p_stroke)
		{
			if (f_stroke == NULL)
				new_stroke();
		}
		
		if (p_stroke || p_fill)
		{
				if (f_fill_foreground == NULL)
					new_fill_foreground();
		}
		
		t_mark -> type = (MCMarkType)p_type;
		t_mark -> stroke = (p_stroke ? f_stroke : NULL);
		t_mark -> fill = (p_stroke || p_fill ? f_fill_foreground : NULL);
		
		if (p_stroke)
			f_stroke_used = true;
		
		if (p_fill || p_stroke)
			f_fill_foreground_used = true;
			
		t_mark -> clip = f_clip;
		
		if (f_state_stack != NULL)
		{
			assert(f_state_stack -> root != NULL && f_state_stack -> root -> type == MARK_TYPE_GROUP);

			t_mark -> previous = f_state_stack -> root -> group . tail;
			t_mark -> next = NULL;
			
			if (t_mark -> previous != NULL)
				t_mark -> previous -> next = t_mark;
			else
				f_state_stack -> root -> group . head = t_mark;
				
			f_state_stack -> root -> group . tail = t_mark;
		}
	}
	
	return t_mark;
}

void MCMetaContext::rectangle_mark(bool p_stroke, bool p_fill, const MCRectangle& rect)
{
	MCMark *t_mark;
	t_mark = new_mark(MARK_TYPE_RECTANGLE, p_stroke, p_fill);
	if (t_mark != NULL)
		t_mark -> rectangle . bounds = rect;
}

void MCMetaContext::round_rectangle_mark(bool p_stroke, bool p_fill, const MCRectangle& rect, uint2 radius)
{
	MCMark *t_mark;
	t_mark = new_mark(MARK_TYPE_ROUND_RECTANGLE, p_stroke, p_fill);
	if (t_mark != NULL)
	{
		t_mark -> round_rectangle . bounds = rect;
		t_mark -> round_rectangle . radius = radius;
	}
}

void MCMetaContext::polygon_mark(bool p_stroke, bool p_fill, MCPoint *p_vertices, uint2 p_arity, bool p_closed)
{
	MCMark *t_mark;
	
	t_mark = new_mark(MARK_TYPE_POLYGON, p_stroke, p_fill);
	if (t_mark != NULL)
	{
		t_mark -> polygon . count = p_arity;
		t_mark -> polygon . vertices = new_array<MCPoint>(p_arity);
		if (t_mark -> polygon . vertices != NULL)
			memcpy(t_mark -> polygon . vertices, p_vertices, sizeof(MCPoint) * p_arity);
		t_mark -> polygon . closed = p_closed;
	}
}

void MCMetaContext::arc_mark(bool p_stroke, bool p_fill, const MCRectangle& p_bounds, uint2 p_start, uint2 p_angle, bool p_complete)
{
	MCMark *t_mark;
	
	t_mark = new_mark(MARK_TYPE_ARC, p_stroke, p_fill);
	if (t_mark != NULL)
	{
		t_mark -> arc . bounds = p_bounds;
		t_mark -> arc . start = p_start;
		t_mark -> arc . angle = p_angle;
		t_mark -> arc . complete = p_complete;
	}
}

void MCMetaContext::path_mark(bool p_stroke, bool p_fill, MCPath *p_path, bool p_evenodd)
{
	MCMark *t_mark;
	t_mark = new_mark(MARK_TYPE_PATH, p_stroke, p_fill);
	if (t_mark != NULL)
	{
		p_path -> get_lengths(t_mark -> path . command_count, t_mark -> path . ordinate_count);
		t_mark -> path . evenodd = p_evenodd;
		t_mark -> path . commands = new_array<uint1>(t_mark -> path . command_count);
		t_mark -> path . ordinates = new_array<int4>(t_mark -> path . ordinate_count);
		if (t_mark -> path . commands != NULL)
			memcpy(t_mark -> path . commands, p_path -> get_commands(), t_mark -> path . command_count);
		if (t_mark -> path . ordinates != NULL)
			memcpy(t_mark -> path . ordinates, p_path -> get_ordinates(), t_mark -> path . ordinate_count * sizeof(int4));
	}
}
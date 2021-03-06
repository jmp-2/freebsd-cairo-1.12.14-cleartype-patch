--- src/cairo-ft-font.c.orig	2014-09-25 15:37:17 UTC
+++ src/cairo-ft-font.c
@@ -59,7 +59,7 @@
 #if HAVE_FT_GLYPHSLOT_EMBOLDEN
 #include FT_SYNTHESIS_H
 #endif
-
+#define FIR_FILTER 1
 #if HAVE_FT_LIBRARY_SETLCDFILTER
 #include FT_LCD_FILTER_H
 #endif
@@ -625,6 +625,7 @@
 
     _cairo_ft_unscaled_font_fini (unscaled);
 }
+static const int   fir_filter[5] = { 0x1C, 0x38, 0x55, 0x38, 0x1C };
 
 static cairo_bool_t
 _has_unlocked_face (const void *entry)
@@ -1119,7 +1120,7 @@
     unsigned char *data;
     int format = CAIRO_FORMAT_A8;
     cairo_image_surface_t *image;
-
+    cairo_bool_t subpixel = FALSE;
     width = bitmap->width;
     height = bitmap->rows;
 
@@ -1189,15 +1190,217 @@
 
 	    format = CAIRO_FORMAT_A8;
 	} else {
-	    /* if we get there, the  data from the source bitmap
-	     * really comes from _fill_xrender_bitmap, and is
-	     * made of 32-bit ARGB or ABGR values */
-	    assert (own_buffer != 0);
-	    assert (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY);
+	    unsigned char*  line;
+	    unsigned char*  bufBitmap;
+	    int		    pitch;
+	    unsigned char   *data_rgba;
+	    unsigned int    width_rgba, stride_rgba;
+	    int		    vmul = 1;
+	    int		    hmul = 1;
 
-	    data = bitmap->buffer;
+	    switch (font_options->subpixel_order) {
+	    case CAIRO_SUBPIXEL_ORDER_DEFAULT:
+	    case CAIRO_SUBPIXEL_ORDER_RGB:
+	    case CAIRO_SUBPIXEL_ORDER_BGR:
+	    default:
+		width /= 3;
+		hmul = 3;
+		break;
+	    case CAIRO_SUBPIXEL_ORDER_VRGB:
+	    case CAIRO_SUBPIXEL_ORDER_VBGR:
+		vmul = 3;
+		height /= 3;
+		break;
+	    }
+	    /*
+	     * Filter the glyph to soften the color fringes
+	     */
+	    width_rgba = width;
 	    stride = bitmap->pitch;
+	    stride_rgba = (width_rgba * 4 + 3) & ~3;
+	    data_rgba = calloc (1, stride_rgba * height);
+
+	    /* perform in-place FIR filtering in either the horizontal or
+	     * vertical direction. We're going to modify the RGB graymap,
+	     * but that's ok, because we either own it, or its part of
+	     * the FreeType glyph slot, which will not be used anymore.
+	     */
+	    pitch  = bitmap->pitch;
+	    line   = (unsigned char*)bitmap->buffer;
+	    if ( pitch < 0 )
+		line -= pitch*(height-1);
+
+	    bufBitmap = line;
+
+	    switch (font_options->subpixel_order) {
+	    case CAIRO_SUBPIXEL_ORDER_DEFAULT:
+	    case CAIRO_SUBPIXEL_ORDER_RGB:
+	    case CAIRO_SUBPIXEL_ORDER_BGR:
+	    {
+		int  h;
+
+		for ( h = height; h > 0; h--, line += pitch ) {
+		    int             pix[6] = { 0, 0, 0, 0, 0, 0 };
+		    unsigned char*  p      = line;
+		    unsigned char*  limit  = line + width*3;
+		    int             nn, val, val2;
+
+		    val = p[0];
+		    for (nn = 0; nn < 3; nn++)
+			pix[2 + nn] += val * fir_filter[nn];
+
+		    val = p[1];
+		    for (nn = 0; nn < 4; nn++)
+			pix[1 + nn] += val * fir_filter[nn];
+
+		    p += 2;
+
+		    for ( ; p  < limit; p++ ) {
+			val = p[0];
+			for (nn = 0; nn < 5; nn++)
+			    pix[nn] += val * fir_filter[nn];
+
+			val2  = pix[0] / 256;
+			val2 |= -(val2 >> 8);
+			p[-2]  = (unsigned char)val2;
+
+			for (nn = 0; nn < 5; nn++)
+			    pix[nn] = pix[nn + 1];
+		    }
+		    for (nn = 0; nn < 2; nn++ ) {
+			val2  = pix[nn] / 256;
+			val2 |= -(val2 >> 8);
+			p[nn - 2] = (unsigned char)val2;
+		    }
+		}
+	    }
+	    break;
+	    case CAIRO_SUBPIXEL_ORDER_VRGB:
+	    case CAIRO_SUBPIXEL_ORDER_VBGR:
+	    {
+		int  w;
+
+		for (w = 0; w < width; w++ ) {
+		    int  pix[6] = { 0, 0, 0, 0, 0, 0 };
+		    unsigned char*  p     = bufBitmap + w;
+		    unsigned char*  limit = bufBitmap + w + height*3*pitch;
+		    int             nn, val, val2;
+
+		    val = p[0];
+		    for (nn = 0; nn < 3; nn++)
+			pix[2 + nn] += val*fir_filter[nn];
+
+		    val = p[pitch];
+		    for (nn = 0; nn < 4; nn++ )
+			pix[1 + nn] += val * fir_filter[nn];
+
+		    p += 2*pitch;
+		    for ( ; p < limit; p += pitch ) {
+			val = p[0];
+			for (nn = 0; nn < 5; nn++ )
+			    pix[nn] += val * fir_filter[nn];
+
+			val2  = pix[0] / 256;
+			val2 |= -(val2 >> 8);
+			p[-2 * pitch] = (unsigned char)val2;
+
+			for (nn = 0; nn < 5; nn++)
+			    pix[nn] = pix[nn+1];
+		    }
+
+		    for (nn = 0; nn < 2; nn++) {
+			val2  = pix[nn] / 256;
+			val2 |= -(val2 >> 8);
+			p[(nn - 2) * pitch] = (unsigned char)val2;
+		    }
+		}
+	    }
+	    break;
+	    default:  /* shouldn't happen */
+		break;
+	    }
+
+	    /* now copy the resulting graymap into an ARGB32 image */
+	    {
+		unsigned char*  in_line  = bufBitmap;
+		unsigned char*  out_line = data_rgba;
+		int             h        = height;
+
+		switch (font_options->subpixel_order) {
+		case CAIRO_SUBPIXEL_ORDER_DEFAULT:
+		case CAIRO_SUBPIXEL_ORDER_RGB:
+		    for ( ; h > 0; h--, in_line += pitch, out_line += stride_rgba) {
+			unsigned char*  in  = in_line;
+			int*            out = (int*)out_line;
+			int             w;
+
+			for (w = width; w > 0; w--, in += 3, out += 1) {
+			    int  r = in[0];
+			    int  g = in[1];
+			    int  b = in[2];
+
+			    out[0] = (g << 24) | (r << 16) | (g << 8) | b;
+			}
+		    }
+		    break;
+		case CAIRO_SUBPIXEL_ORDER_BGR:
+		    for ( ; h > 0; h--, in_line += pitch, out_line += stride_rgba) {
+			unsigned char*  in  = in_line;
+			int*            out = (int*)out_line;
+			int             w;
+
+			for (w = width; w > 0; w--, in += 3, out += 1) {
+			    int  r = in[2];
+			    int  g = in[1];
+			    int  b = in[0];
+
+			    out[0] = (g << 24) | (r << 16) | (g << 8) | b;
+			}
+		    }
+		    break;
+		case CAIRO_SUBPIXEL_ORDER_VRGB:
+		    for ( ; h > 0; h--, in_line += pitch*3, out_line += stride_rgba) {
+			unsigned char*  in  = in_line;
+			int*            out = (int*)out_line;
+			int             w;
+
+			for (w = width; w > 0; w--, in += 1, out += 1) {
+			    int  r = in[0];
+			    int  g = in[pitch];
+			    int  b = in[pitch*2];
+
+			    out[0] = (g << 24) | (r << 16) | (g << 8) | b;
+			}
+		    }
+		    break;
+		case CAIRO_SUBPIXEL_ORDER_VBGR:
+		    for ( ; h > 0; h--, in_line += pitch*3, out_line += stride_rgba) {
+			unsigned char*  in  = in_line;
+			int*            out = (int*)out_line;
+			int             w;
+
+			for (w = width; w > 0; w--, in += 1, out += 1) {
+			    int  r = in[2*pitch];
+			    int  g = in[pitch];
+			    int  b = in[0];
+
+			    out[0] = (g << 24) | (r << 16) | (g << 8) | b;
+			}
+		    }
+		    break;
+		}
+	    }
+
+	    if (own_buffer)
+		free (bitmap->buffer);
+	    data = data_rgba;
+	    stride = stride_rgba;
 	    format = CAIRO_FORMAT_ARGB32;
+	    subpixel = TRUE;
+	    break;
+        
+	
+
 	}
 	break;
     case FT_PIXEL_MODE_GRAY2:
@@ -1249,63 +1452,18 @@
 		       cairo_font_options_t	 *font_options,
 		       cairo_image_surface_t	**surface)
 {
-    int rgba = FC_RGBA_UNKNOWN;
-    int lcd_filter = FT_LCD_FILTER_LEGACY;
+ 
     FT_GlyphSlot glyphslot = face->glyph;
     FT_Outline *outline = &glyphslot->outline;
     FT_Bitmap bitmap;
     FT_BBox cbox;
-    unsigned int width, height;
+     FT_Matrix matrix;
+    int hmul = 1;
+    int vmul = 1;
+    unsigned int width, height, stride;
+    cairo_bool_t subpixel = FALSE;
     cairo_status_t status;
-    FT_Error fterror;
-    FT_Library library = glyphslot->library;
-    FT_Render_Mode render_mode = FT_RENDER_MODE_NORMAL;
-
-    switch (font_options->antialias) {
-    case CAIRO_ANTIALIAS_NONE:
-	render_mode = FT_RENDER_MODE_MONO;
-	break;
-
-    case CAIRO_ANTIALIAS_SUBPIXEL:
-    case CAIRO_ANTIALIAS_BEST:
-	switch (font_options->subpixel_order) {
-	    case CAIRO_SUBPIXEL_ORDER_DEFAULT:
-	    case CAIRO_SUBPIXEL_ORDER_RGB:
-	    case CAIRO_SUBPIXEL_ORDER_BGR:
-		render_mode = FT_RENDER_MODE_LCD;
-		break;
-
-	    case CAIRO_SUBPIXEL_ORDER_VRGB:
-	    case CAIRO_SUBPIXEL_ORDER_VBGR:
-		render_mode = FT_RENDER_MODE_LCD_V;
-		break;
-	}
-
-	switch (font_options->lcd_filter) {
-	case CAIRO_LCD_FILTER_NONE:
-	    lcd_filter = FT_LCD_FILTER_NONE;
-	    break;
-	case CAIRO_LCD_FILTER_DEFAULT:
-	case CAIRO_LCD_FILTER_INTRA_PIXEL:
-	    lcd_filter = FT_LCD_FILTER_LEGACY;
-	    break;
-	case CAIRO_LCD_FILTER_FIR3:
-	    lcd_filter = FT_LCD_FILTER_LIGHT;
-	    break;
-	case CAIRO_LCD_FILTER_FIR5:
-	    lcd_filter = FT_LCD_FILTER_DEFAULT;
-	    break;
-	}
-
-	break;
-
-    case CAIRO_ANTIALIAS_DEFAULT:
-    case CAIRO_ANTIALIAS_GRAY:
-    case CAIRO_ANTIALIAS_GOOD:
-    case CAIRO_ANTIALIAS_FAST:
-	render_mode = FT_RENDER_MODE_NORMAL;
-    }
-
+  
     FT_Outline_Get_CBox (outline, &cbox);
 
     cbox.xMin &= -64;
@@ -1315,21 +1473,22 @@
 
     width = (unsigned int) ((cbox.xMax - cbox.xMin) >> 6);
     height = (unsigned int) ((cbox.yMax - cbox.yMin) >> 6);
-
+    stride = (width * hmul + 3) & ~3;
     if (width * height == 0) {
 	cairo_format_t format;
 	/* Looks like fb handles zero-sized images just fine */
-	switch (render_mode) {
-	case FT_RENDER_MODE_MONO:
+	switch (font_options->antialias) {
+	case CAIRO_ANTIALIAS_NONE:
 	    format = CAIRO_FORMAT_A1;
 	    break;
-	case FT_RENDER_MODE_LCD:
-	case FT_RENDER_MODE_LCD_V:
+	case CAIRO_ANTIALIAS_SUBPIXEL:
+	case CAIRO_ANTIALIAS_BEST:
 	    format= CAIRO_FORMAT_ARGB32;
 	    break;
-	case FT_RENDER_MODE_LIGHT:
-	case FT_RENDER_MODE_NORMAL:
-	case FT_RENDER_MODE_MAX:
+	case CAIRO_ANTIALIAS_DEFAULT:
+	case CAIRO_ANTIALIAS_GRAY:
+	case CAIRO_ANTIALIAS_GOOD:
+	case CAIRO_ANTIALIAS_FAST:
 	default:
 	    format = CAIRO_FORMAT_A8;
 	    break;
@@ -1341,73 +1500,85 @@
 	    return (*surface)->base.status;
     } else {
 
-	int bitmap_size;
-
-	switch (render_mode) {
-	case FT_RENDER_MODE_LCD:
-	    if (font_options->subpixel_order == CAIRO_SUBPIXEL_ORDER_BGR)
-		rgba = FC_RGBA_BGR;
-	    else
-		rgba = FC_RGBA_RGB;
-	    break;
+	matrix.xx = matrix.yy = 0x10000L;
+	matrix.xy = matrix.yx = 0;
 
-	case FT_RENDER_MODE_LCD_V:
-	    if (font_options->subpixel_order == CAIRO_SUBPIXEL_ORDER_VBGR)
-		rgba = FC_RGBA_VBGR;
-	    else
-		rgba = FC_RGBA_VRGB;
+	
+	switch (font_options->antialias) {
+	case CAIRO_ANTIALIAS_NONE:
+	    bitmap.pixel_mode = FT_PIXEL_MODE_MONO;
+	    bitmap.num_grays  = 1;
+	    stride = ((width + 31) & -32) >> 3;
 	    break;
 
-	case FT_RENDER_MODE_MONO:
-	case FT_RENDER_MODE_LIGHT:
-	case FT_RENDER_MODE_NORMAL:
-	case FT_RENDER_MODE_MAX:
-	default:
+	
+	case CAIRO_ANTIALIAS_DEFAULT:
+	case CAIRO_ANTIALIAS_GRAY:
+	case CAIRO_ANTIALIAS_GOOD:
+	case CAIRO_ANTIALIAS_FAST:
+	    bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
+	    bitmap.num_grays  = 256;
+	    stride = (width + 3) & -4;
 	    break;
-	}
-
-#if HAVE_FT_LIBRARY_SETLCDFILTER
-	FT_Library_SetLcdFilter (library, lcd_filter);
+        case CAIRO_ANTIALIAS_SUBPIXEL:
+	case CAIRO_ANTIALIAS_BEST:
+	    switch (font_options->subpixel_order) {
+	    case CAIRO_SUBPIXEL_ORDER_RGB:
+	    case CAIRO_SUBPIXEL_ORDER_BGR:
+	    case CAIRO_SUBPIXEL_ORDER_DEFAULT:
+	    default:
+		matrix.xx *= 3;
+		hmul = 3;
+		subpixel = TRUE;
+#ifdef FIR_FILTER
+		cbox.xMin -= 64;
+		cbox.xMax += 64;
+		width    += 2;
 #endif
-
-	fterror = FT_Render_Glyph (face->glyph, render_mode);
-
-#if HAVE_FT_LIBRARY_SETLCDFILTER
-	FT_Library_SetLcdFilter (library, FT_LCD_FILTER_NONE);
+		break;
+	    case CAIRO_SUBPIXEL_ORDER_VRGB:
+	    case CAIRO_SUBPIXEL_ORDER_VBGR:
+		matrix.yy *= 3;
+		vmul = 3;
+		subpixel = TRUE;
+#ifdef FIR_FILTER
+		cbox.yMin -= 64;
+		cbox.yMax += 64;
+		height    += 2;
 #endif
-
-	if (fterror != 0)
-		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
-
-	bitmap_size = _compute_xrender_bitmap_size (&bitmap,
-						    face->glyph,
-						    render_mode);
-	if (bitmap_size < 0)
-	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
-
-	bitmap.buffer = calloc (1, bitmap_size);
+break;
+	    }
+	    FT_Outline_Transform (outline, &matrix);
+	    bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
+	    bitmap.num_grays  = 256;
+	    stride = (width * hmul + 3) & -4;
+	}
+	bitmap.pitch = stride;
+	bitmap.width = width * hmul;
+	bitmap.rows = height * vmul;
+	bitmap.buffer = calloc (stride, bitmap.rows);
 	if (bitmap.buffer == NULL)
-		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
-
-	_fill_xrender_bitmap (&bitmap, face->glyph, render_mode,
-			      (rgba == FC_RGBA_BGR || rgba == FC_RGBA_VBGR));
+	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
 
-	/* Note:
-	 * _get_bitmap_surface will free bitmap.buffer if there is an error
-	 */
+        FT_Outline_Translate (outline, -cbox.xMin*hmul, -cbox.yMin*vmul);
+ 
+        if (FT_Outline_Get_Bitmap (glyphslot->library, outline, &bitmap) != 0) {
+	    free (bitmap.buffer);
+	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
+	}
 	status = _get_bitmap_surface (&bitmap, TRUE, font_options, surface);
-	if (unlikely (status))
+	if (status)
 	    return status;
-
-	/* Note: the font's coordinate system is upside down from ours, so the
-	 * Y coordinate of the control box needs to be negated.  Moreover, device
-	 * offsets are position of glyph origin relative to top left while xMin
-	 * and yMax are offsets of top left relative to origin.  Another negation.
-	 */
-	cairo_surface_set_device_offset (&(*surface)->base,
-					 (double)-glyphslot->bitmap_left,
-					 (double)+glyphslot->bitmap_top);
     }
+    /*
+     * Note: the font's coordinate system is upside down from ours, so the
++     * Y coordinate of the control box needs to be negated.  Moreover, device
++     * offsets are position of glyph origin relative to top left while xMin
++     * and yMax are offsets of top left relative to origin.  Another negation.
++     */
+    cairo_surface_set_device_offset (&(*surface)->base,
+				     floor (-(double) cbox.xMin / 64.0),
+				     floor (+(double) cbox.yMax / 64.0));
 
     return CAIRO_STATUS_SUCCESS;
 }
@@ -1768,8 +1939,13 @@
     if (options->base.hint_style == CAIRO_HINT_STYLE_DEFAULT)
 	options->base.hint_style = other->base.hint_style;
 
-    if (other->base.hint_style == CAIRO_HINT_STYLE_NONE)
-	options->base.hint_style = CAIRO_HINT_STYLE_NONE;
+         if (other->base.hint_style == CAIRO_HINT_STYLE_NONE ||
+               other->base.hint_style == CAIRO_HINT_STYLE_SLIGHT ||
+               other->base.hint_style == CAIRO_HINT_STYLE_MEDIUM ||
+               other->base.hint_style == CAIRO_HINT_STYLE_FULL) {
+       options->base.hint_style = other->base.hint_style;
+       }
+
 
     if (options->base.lcd_filter == CAIRO_LCD_FILTER_DEFAULT)
 	options->base.lcd_filter = other->base.lcd_filter;

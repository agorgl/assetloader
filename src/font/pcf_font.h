/*********************************************************************************************************************/
/*                                                  /===-_---~~~~~~~~~------____                                     */
/*                                                 |===-~___                _,-'                                     */
/*                  -==\\                         `//~\\   ~~~~`---.___.-~~                                          */
/*              ______-==|                         | |  \\           _-~`                                            */
/*        __--~~~  ,-/-==\\                        | |   `\        ,'                                                */
/*     _-~       /'    |  \\                      / /      \      /                                                  */
/*   .'        /       |   \\                   /' /        \   /'                                                   */
/*  /  ____  /         |    \`\.__/-~~ ~ \ _ _/'  /          \/'                                                     */
/* /-'~    ~~~~~---__  |     ~-/~         ( )   /'        _--~`                                                      */
/*                   \_|      /        _)   ;  ),   __--~~                                                           */
/*                     '~~--_/      _-~/-  / \   '-~ \                                                               */
/*                    {\__--_/}    / \\_>- )<__\      \                                                              */
/*                    /'   (_/  _-~  | |__>--<__|      |                                                             */
/*                   |0  0 _/) )-~     | |__>--<__|     |                                                            */
/*                   / /~ ,_/       / /__>---<__/      |                                                             */
/*                  o o _//        /-~_>---<__-~      /                                                              */
/*                  (^(~          /~_>---<__-      _-~                                                               */
/*                 ,/|           /__>--<__/     _-~                                                                  */
/*              ,//('(          |__>--<__|     /                  .----_                                             */
/*             ( ( '))          |__>--<__|    |                 /' _---_~\                                           */
/*          `-)) )) (           |__>--<__|    |               /'  /     ~\`\                                         */
/*         ,/,'//( (             \__>--<__\    \            /'  //        ||                                         */
/*       ,( ( ((, ))              ~-__>--<_~-_  ~--____---~' _/'/        /'                                          */
/*     `~/  )` ) ,/|                 ~-_~>--<_/-__       __-~ _/                                                     */
/*   ._-~//( )/ )) `                    ~~-'_/_/ /~~~~~~~__--~                                                       */
/*    ;'( ')/ ,)(                              ~~~~~~~~~~                                                            */
/*   ' ') '( (/                                                                                                      */
/*     '   '  `                                                                                                      */
/*********************************************************************************************************************/
#ifndef _PCF_FONT_H_
#define _PCF_FONT_H_

#include <assets/font/texture_font.h>

/*-----------------------------------------------------------------
 * The Header
 *-----------------------------------------------------------------*/
/* The file header contains 32 bit integers stored with the least significant byte first. */
struct pcf_header {
    char header[4];         /* Always "\1fcp" */
    uint32_t table_count;
    struct toc_entry {
        int32_t type;       /* See below, indicates which table */
        int32_t format;     /* See below, indicates how the data are formatted in the table */
        int32_t size;       /* In bytes */
        int32_t offset;     /* from start of file */
    }* tables; /* (table_count times) */
};

/* The type field may be one of: */
#define PCF_PROPERTIES       (1<<0)
#define PCF_ACCELERATORS     (1<<1)
#define PCF_METRICS          (1<<2)
#define PCF_BITMAPS          (1<<3)
#define PCF_INK_METRICS      (1<<4)
#define PCF_BDF_ENCODINGS    (1<<5)
#define PCF_SWIDTHS          (1<<6)
#define PCF_GLYPH_NAMES      (1<<7)
#define PCF_BDF_ACCELERATORS (1<<8)

/* The format field may be one of: */
#define PCF_DEFAULT_FORMAT      0x00000000
#define PCF_INKBOUNDS           0x00000200
#define PCF_ACCEL_W_INKBOUNDS   0x00000100
#define PCF_COMPRESSED_METRICS  0x00000100

/* The format field may be modified by: */
#define PCF_GLYPH_PAD_MASK  (3<<0)      /* See the bitmap table for explanation */
#define PCF_BYTE_MASK       (1<<2)      /* If set then Most Sig Byte First */
#define PCF_BIT_MASK        (1<<3)      /* If set then Most Sig Bit First */
#define PCF_SCAN_UNIT_MASK  (3<<4)      /* See the bitmap table for explanation */

/* The format will be repeated as the first word in each table.
 * Most tables only have one format (default), but some have alternates.
 * The high order three bytes of the format describe the gross format.
 * The way integers and bits are stored can be altered by adding one of the mask bits above.
 * All tables begin on a 32bit boundary (and will be padded with zeroes). */

/*-----------------------------------------------------------------
 * Properties Table
 *-----------------------------------------------------------------*/
struct pcf_props_table {
    int32_t format;        /* Always stored with least significant byte first! */
    uint32_t nprops;       /* Stored in whatever byte order is specified in the format */
    struct props {
        uint32_t name_offset;  /* Offset into the following string table */
        int8_t is_str_prop;    /* Boolean flag */
        int32_t value;         /* The value for integer props, the offset for string props */
    }* props;              /* nprops length */
    char* padding;         /* pad to next int32 boundary (size = (nprops & 3) == 0 ? 0 :(4 - (nprops & 3))) */
    int string_size;       /* total size of all strings (including their terminating nulls) */
    char* strings;         /* string_size length */
    char padding2[];       /* Also padding? */
};

/* These properties are the Font Atoms that X provides to users.
 * Many are described in xc/doc/specs/XLFD/xlfd.tbl.ms or here
 * (the X protocol does not limit these atoms so others could be defined for some fonts). */
/* To find the name of a property: strings + props[i].name_offset */

/*-----------------------------------------------------------------
 * Metrics Data
 *-----------------------------------------------------------------*/
/* Several of the tables (PCF_METRICS, PCF_INK_METRICS, and within the accelerator tables)
 * contain metrics data which may be in either compressed (PCF_COMPRESSED_METRICS)
 * or uncompressed (DEFAULT) formats. The compressed format uses bytes to contain values,
 * while the uncompressed uses shorts. The (compressed) bytes are unsigned bytes which are
 * offset by 0x80 (so the actual value will be (getc(pcf_file) - 0x80).
 * The data is stored as: */
struct pcf_metrics_compressed {
    uint8_t left_sided_bearing;
    uint8_t right_side_bearing;
    uint8_t character_width;
    uint8_t character_ascent;
    uint8_t character_descent;
    /* Implied character attributes field = 0 */
};

struct pcf_metrics_uncompressed {
    int16_t left_sided_bearing;
    int16_t right_side_bearing;
    int16_t character_width;
    int16_t character_ascent;
    int16_t character_descent;
    uint16_t character_attributes;
};

/*-----------------------------------------------------------------
 * Accelerator Tables
 *-----------------------------------------------------------------*/
/* These data provide various bits of information about the font as a whole.
 * This data structure is used by two tables PCF_ACCELERATORS and PCF_BDF_ACCELERATORS.
 * The tables may either be in DEFAULT format or in PCF_ACCEL_W_INKBOUNDS
 * (in which case they will have some extra metrics data at the end.
 * The accelerator tables look like: */
struct pcf_accel_table {
    /* Always stored with least significant byte first! */
    int32_t format;
    /* If for all i, max(metrics[i].rightSideBearing - metrics[i].characterWidth) <= minbounds.leftSideBearing,
       Means the perchar field of the XFontStruct can be NULL */
    uint8_t no_overlap;
    /* constant_metrics true and forall characters:
        the left side bearing == 0
        the right side bearing == the character's width
        the character's ascent == the font's ascent
        the character's descent == the font's descent */
    uint8_t constant_metrics;
    uint8_t terminal_font; /* ??? */
    /* Monospace font like courier */
    uint8_t constant_width;
    /* Means that all inked bits are within the rectangle with x between [0,charwidth]
       and y between [-descent,ascent]. So no ink overlaps another char when drawing */
    uint8_t ink_inside;
    /* True if the ink metrics differ from the metrics somewhere */
    uint8_t ink_metrics;
    /* 0 => left to right, 1 => right to left */
    uint8_t draw_direction;
    uint8_t padding;
    /* Byte order as specified in format */
    int32_t font_ascent;
    int32_t font_descent;
    /* ??? */
    int32_t max_overlap;
    /* Metrics */
    struct pcf_metrics_compressed minbounds;
    struct pcf_metrics_compressed maxbounds;
    /* If format is PCF_ACCEL_W_INKBOUNDS then include the following fields
       Otherwise those fields are not in the file and should be filled by duplicating min/maxbounds above */
    struct pcf_metrics_uncompressed ink_minbounds;
    struct pcf_metrics_uncompressed ink_maxbounds;
};

/* BDF Accelerators should be preferred to plain Accelerators if both tables are present.
 * BDF Accelerators contain data that refers only to the encoded characters in the font
 * (while the simple Accelerator table includes all glyphs), therefore the BDF Accelerators are more accurate. */

/*-----------------------------------------------------------------
 * Metrics Tables
 *-----------------------------------------------------------------*/
/* There are two different metrics tables, PCF_METRICS and PCF_INK_METRICS,
 * the former contains the size of the stored bitmaps, while the latter contains
 * the minimum bounding box. The two may contain the same data, but many CJK fonts
 * pad the bitmaps so all bitmaps are the same size. The table format may be either
 * DEFAULT or PCF_COMPRESSED_METRICS (see the section on Metrics Data for an explanation). */
struct pcf_metrics_tables {
    /* Always stored with least significant byte first! */
    int32_t format;
    union {
        struct {
            /* if the format is compressed */
            int16_t metrics_count;
            struct pcf_metrics_compressed* metrics; /* [metrics_count] */
        } comp;
        struct {
            /* else if format is default (uncompressed) */
            int32_t metrics_count;
            struct pcf_metrics_uncompressed* metrics; /* [metrics_count] */
        } ucomp;
    } mt;
};

/*-----------------------------------------------------------------
 * The Bitmap Table
 *-----------------------------------------------------------------*/
/* The bitmap table has type PCF_BITMAPS. Its format must be PCF_DEFAULT. */
struct pcf_bitmap_table {
    /* Always stored with least significant byte first! */
    int32_t format;
    /* Byte ordering depends on format, should be the same as the metrics count */
    int32_t glyph_count;
    /* Byte offsets to bitmap data (glyph_count length) */
    int32_t* offsets;
    /* The size the bitmap data will take up depending on various padding options
       which one is actually used in the file is given by (format & 3) */
    int32_t bitmap_sizes[4];
    /* The bitmap data. format contains flags that indicate:
        - the byte order (format & 4 => LSByte first)
        - the bit order (format & 8 => LSBit first)
        - how each row in each glyph's bitmap is padded (format & 3)
           0=>bytes, 1=>shorts, 2=>ints
        - what the bits are stored in (bytes, shorts, ints) (format >> 4) & 3
           0=>bytes, 1=>shorts, 2=>ints */
    char* bitmap_data; /* (bitmapsizes[format & 3] length) */
};

/*-----------------------------------------------------------------
 * The Encoding Table
 *-----------------------------------------------------------------*/
/* The encoding table has type PCF_BDF_ENCODINGS. Its format must be PCF_DEFAULT. */
struct pcf_encoding_table {
    int32_t format; 		    /* Always stored with least significant byte first! */
    int16_t min_char_or_byte2;	/* As in XFontStruct */
    int16_t max_char_or_byte2;	/* As in XFontStruct */
    int16_t min_byte1;		    /* As in XFontStruct */
    int16_t max_byte1;		    /* As in XFontStruct */
    int16_t default_char;		/* As in XFontStruct */
    /* Gives the glyph index that corresponds to each encoding value
       a value of 0xffff means no glyph for that encoding */
    int16_t* glyphindeces;      /* length = (max_char_or_byte2 - min_char_or_byte2 + 1) * (max_byte1 - min_byte1 + 1) */
};
/* For single byte encodings min_byte1 == max_byte1 == 0,
 * and encoded values are between [min_char_or_byte2, max_char_or_byte2].
 * The glyph index corresponding to an encoding is glyphindex[encoding - min_char_or_byte2].
 * Otherwise [min_byte1,max_byte1] specifies the range allowed for the first (high order) byte
 * of a two byte encoding, while [min_char_or_byte2, max_char_or_byte2] is the range of the second byte.
 * The glyph index corresponding to a double byte encoding (enc1, enc2)
 * is glyph_index[(enc1 - min_byte1) * (max_char_or_byte2 - min_char_or_byte2 + 1) + enc2-min_char_or_byte2].
 * Not all glyphs need to be encoded. Not all encodings need to be associated with glyphs. */

/*-----------------------------------------------------------------
 * The Scalable Widths Table
 *-----------------------------------------------------------------*/
/* The encoding table has type PCF_SWIDTHS. Its format must be PCF_DEFAULT. */
struct pcf_scalable_widths_table {
    int32_t format;      /* Always stored with least significant byte first! */
    int32_t glyph_count; /* Byte ordering depends on format, should be the same as the metrics count */
    int32_t* swidths;    /* Byte offsets to bitmap data (glyph_count length) */
};
/* The scalable width of a character is the width of the corresponding postscript character in em-units (1/1000ths of an em). */

/*-----------------------------------------------------------------
 * The Glyph Names Table
 *-----------------------------------------------------------------*/
/* The encoding table has type PCF_GLYPH_NAMES. Its format must be PCF_DEFAULT. */
struct pcf_glyph_names_table {
    int32_t format;          /* Always stored with least significant byte first! */
    int32_t glyph_count;     /* Byte ordering depends on format, should be the same as the metrics count */
    int32_t* offsets;        /* Byte offsets to string data (glyph_count length) */
    int32_t string_size;
    char* string;            /* (string_size length) */
};
/* The postscript name associated with each character. */

/* Public interface */
int pcf_font_init(texture_font_t* self);
int pcf_font_load_glyph(texture_font_t* self, const char* codepoint);

#endif /* ! _PCF_FONT_H_ */

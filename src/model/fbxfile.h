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
#ifndef _FBXFILE_H_
#define _FBXFILE_H_

#include <stdlib.h>
#include <stdint.h>

/*------------------------------------------*
 * Node Record Format                       |
 *------------------------------------------*
 * 4 bytes           | End Offset           |
 * 4 bytes           | Num Properties       |
 * 4 bytes           | Property List Length |
 * 1 bytes           | Name Length          |
 * Name Length bytes | Name                 |
 * Variable bytes    | Property Data        |
 * Variable bytes    | Nested Records Data  |
 * 13 bytes          | Padding Block        |
 *------------------------------------------*/

/*----------------------------*
 * Property Record Format     |
 *----------------------------*
 * 1 byte         | Type code |
 * Variable bytes | Data      |
 *----------------------------*/

/*------------------------------------*
 * Array Data Format                  |
 *------------------------------------*
 * 4 bytes        | Array Length      |
 * 4 bytes        | Encoding          |
 * 4 bytes        | Compressed Length |
 * Variable bytes | Contents          |
 *------------------------------------*/

/* If Encoding is 0, the Contents is just ArrayLength times the array data type.
 * If Encoding is 1, the Contents is a deflate/zip-compressed buffer of length
 * CompressedLength bytes. The buffer can for example be decoded using zlib. */

/* For String and Raw data types we have 4 bytes of length following
 * with length bytes of their data */

/*-----------------------------------------------------------------
 * Types
 *-----------------------------------------------------------------*/
struct parser_state {
    unsigned char* data;
    unsigned char* cur;
    unsigned char* bufend;
};

/* FBX Property type */
enum fbx_pt {
    /* Primitive types */
    fbx_pt_short,  /* 2 byte signed integer */
    fbx_pt_bool,   /* 1 byte boolean */
    fbx_pt_int,    /* 4 byte signed integer */
    fbx_pt_float,  /* 4 byte single precision IEEE 754 number */
    fbx_pt_double, /* 8 byte double precision IEEE 754 number */
    fbx_pt_long,   /* 8 byte signed integer */
    /* Array types */
    fbx_pt_float_arr,
    fbx_pt_double_arr,
    fbx_pt_long_arr,
    fbx_pt_int_arr,
    fbx_pt_bool_arr,
    /* Other */
    fbx_pt_string, /* Not null terminated, may contain nulls */
    fbx_pt_raw,
    fbx_pt_invalid
};

/* FBX Property */
struct fbx_property {
    char code;
    enum fbx_pt type;
    union {
        uint8_t b;
        int16_t s;
        int32_t i;
        int64_t l;
        float f;
        double d;
        void* p;
        uint8_t* bp;
        int16_t* sp;
        int32_t* ip;
        int64_t* lp;
        float* fp;
        double* dp;
        const char* str;
        unsigned char* raw;
    } data;
    uint32_t length;
    /* Indicates that current property was an encoded array,
     * and that additional heap memory was allocated for its
     * decoded data that must be freed later */
    int enc_arr;
};

/* FBX Record */
struct fbx_record {
    char* name;
    struct fbx_property* properties; /* Array of properties */
    uint32_t num_props;
    struct fbx_record* subrecords;   /* Linked list of subrecords */
    struct fbx_record* next;         /* Next record in list */
};

/* FBX File */
struct fbx_file {
    unsigned int version;
    struct fbx_record* root;
};

/*-----------------------------------------------------------------
 * Functions
 *-----------------------------------------------------------------*/
int fbx_read_header(struct parser_state* ps, struct fbx_file* fbx);
struct fbx_record* fbx_read_root_record(struct parser_state* ps);
void fbx_record_destroy(struct fbx_record* fbxr);
struct fbx_record* fbx_find_subrecord_with_name(struct fbx_record* rec, const char* name);
struct fbx_record* fbx_find_sibling_with_name(struct fbx_record* rec, const char* name);
size_t fbx_pt_unit_size(enum fbx_pt pt);
void fbx_record_print(struct fbx_record* rec, int depth);

#endif /* ! _FBXFILE_H_ */

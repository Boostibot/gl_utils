#pragma once
#include "gl.h"
#include "../lib/image.h"

typedef struct GL_Pixel_Format {
    GLuint type;
    GLuint format;
    GLuint internal_format;
    i32 pixel_size;
    i32 channels;
    Image_Pixel_Format equivalent;

    bool unrepresentable;
} GL_Pixel_Format;

GL_Pixel_Format gl_pixel_format_from_pixel_format(Image_Pixel_Format pixel_format, isize channels)
{
    GL_Pixel_Format error_format = {0};
    error_format.unrepresentable = true;
    
    GL_Pixel_Format out = {0};
    i32 channel_size = 0;
    
    switch(channels)
    {
        case 1: out.format = GL_RED; break;
        case 2: out.format =  GL_RG; break;
        case 3: out.format =  GL_RGB; break;
        case 4: out.format =  GL_RGBA; break;
        
        default: return error_format;
    }

    switch(pixel_format)
    {
        case PIXEL_FORMAT_U8: {
            channel_size = 1;
            out.type = GL_UNSIGNED_BYTE;
            switch(channels)
            {
                case 1: out.internal_format = GL_R8UI; break;
                case 2: out.internal_format = GL_RG8UI; break;
                case 3: out.internal_format = GL_RGB8UI; break;
                case 4: out.internal_format = GL_RGBA8UI; break;
            }
        } break;
        
        case PIXEL_FORMAT_U16: {
            channel_size = 2;
            out.type = GL_UNSIGNED_SHORT;
            switch(channels)
            {
                case 1: out.internal_format = GL_R16UI; break;
                case 2: out.internal_format = GL_RG16UI; break;
                case 3: out.internal_format = GL_RGB16UI; break;
                case 4: out.internal_format = GL_RGBA16UI; break;
            }
        } break;
        
        case PIXEL_FORMAT_U32: {
            channel_size = 4;
            out.type = GL_UNSIGNED_INT;
            switch(channels)
            {
                case 1: out.internal_format = GL_R32UI; break;
                case 2: out.internal_format = GL_RG32UI; break;
                case 3: out.internal_format = GL_RGB32UI; break;
                case 4: out.internal_format = GL_RGBA32UI; break;
            }
        } break;
        
        case PIXEL_FORMAT_F32: {
            channel_size = 4;
            out.type = GL_FLOAT;
            switch(channels)
            {
                case 1: out.internal_format = GL_R32F; break;
                case 2: out.internal_format = GL_RG32F; break;
                case 3: out.internal_format = GL_RGB32F; break;
                case 4: out.internal_format = GL_RGBA32F; break;
            }
        } break;
        
        case PIXEL_FORMAT_U24: 
        default: return error_format;
    }

    out.channels = (i32) channels;
    out.pixel_size = out.channels * channel_size;
    out.equivalent = pixel_format;
    return out;
}

Image_Pixel_Format pixel_format_from_gl_pixel_format(GL_Pixel_Format gl_format, isize* channels)
{
    switch(gl_format.internal_format)
    {
        case GL_R8UI: *channels = 1; return PIXEL_FORMAT_U8;
        case GL_RG8UI: *channels = 2; return PIXEL_FORMAT_U8;
        case GL_RGB8UI: *channels = 3; return PIXEL_FORMAT_U8;
        case GL_RGBA8UI: *channels = 4; return PIXEL_FORMAT_U8;

        case GL_R16UI: *channels = 1; return PIXEL_FORMAT_U16;
        case GL_RG16UI: *channels = 2; return PIXEL_FORMAT_U16;
        case GL_RGB16UI: *channels = 3; return PIXEL_FORMAT_U16;
        case GL_RGBA16UI: *channels = 4; return PIXEL_FORMAT_U16;

        case GL_R32UI: *channels = 1; return PIXEL_FORMAT_U32;
        case GL_RG32UI: *channels = 2; return PIXEL_FORMAT_U32;
        case GL_RGB32UI: *channels = 3; return PIXEL_FORMAT_U32;
        case GL_RGBA32UI: *channels = 4; return PIXEL_FORMAT_U32;

        case GL_R32F: *channels = 1; return PIXEL_FORMAT_F32;
        case GL_RG32F: *channels = 2; return PIXEL_FORMAT_F32;
        case GL_RGB32F: *channels = 3; return PIXEL_FORMAT_F32;
        case GL_RGBA32F: *channels = 4; return PIXEL_FORMAT_F32;
    }
    
    *channels = gl_format.channels;
    switch(gl_format.type)
    {
        case GL_UNSIGNED_BYTE: return PIXEL_FORMAT_U8;
        case GL_UNSIGNED_SHORT: return PIXEL_FORMAT_U16;
        case GL_UNSIGNED_INT: return PIXEL_FORMAT_U32;
        case GL_FLOAT: return PIXEL_FORMAT_F32;
    }
    
    *channels = 0;
    return (Image_Pixel_Format) 0; 
}

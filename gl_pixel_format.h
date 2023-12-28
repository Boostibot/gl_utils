#pragma once
#include "gl.h"
#include "../lib/image.h"

typedef struct GL_Pixel_Format {
    GLuint channel_type;    //GL_FLOAT, GL_UNSIGNED_BYTE, ...
    GLuint access_format;   //GL_RGBA, GL_RED, ...
    GLuint storage_format;  //GL_RGBA16UI, GL_R8UI, ...
    i32 pixel_size;         //4*N, 1*N, ... where N is sizeof channel_type
    i32 channels;           //4, 1, ... 

    Image_Pixel_Format equivalent;
    bool unrepresentable;   //true if cannot be cleanly converted to equivalent.
} GL_Pixel_Format;

GL_Pixel_Format gl_pixel_format_from_pixel_format(Image_Pixel_Format pixel_format, isize channels)
{
    GL_Pixel_Format error_format = {0};
    error_format.unrepresentable = true;
    
    GL_Pixel_Format out = {0};
    i32 channel_size = 0;
    
    switch(channels)
    {
        case 1: out.access_format = GL_RED; break;
        case 2: out.access_format =  GL_RG; break;
        case 3: out.access_format =  GL_RGB; break;
        case 4: out.access_format =  GL_RGBA; break;
        
        default: return error_format;
    }

    switch(pixel_format)
    {
        case PIXEL_FORMAT_U8: {
            channel_size = 1;
            out.channel_type = GL_UNSIGNED_BYTE;
            switch(channels)
            {
                case 1: out.storage_format = GL_R8; break;
                case 2: out.storage_format = GL_RG8; break;
                case 3: out.storage_format = GL_RGB8; break;
                case 4: out.storage_format = GL_RGBA8; break;
            }
        } break;
        
        case PIXEL_FORMAT_U16: {
            channel_size = 2;
            out.channel_type = GL_UNSIGNED_SHORT;
            switch(channels)
            {
                case 1: out.storage_format = GL_R16; break;
                case 2: out.storage_format = GL_RG16; break;
                case 3: out.storage_format = GL_RGB16; break;
                case 4: out.storage_format = GL_RGBA16; break;
            }
        } break;
        
        case PIXEL_FORMAT_U32: {
            channel_size = 4;
            out.channel_type = GL_UNSIGNED_INT;
            switch(channels)
            {
                case 1: out.storage_format = GL_R32UI; break;
                case 2: out.storage_format = GL_RG32UI; break;
                case 3: out.storage_format = GL_RGB32UI; break;
                case 4: out.storage_format = GL_RGBA32UI; break;
            }
        } break;
        
        case PIXEL_FORMAT_F32: {
            channel_size = 4;
            out.channel_type = GL_FLOAT;
            switch(channels)
            {
                case 1: out.storage_format = GL_R32F; break;
                case 2: out.storage_format = GL_RG32F; break;
                case 3: out.storage_format = GL_RGB32F; break;
                case 4: out.storage_format = GL_RGBA32F; break;
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
    switch(gl_format.storage_format)
    {
        case GL_R8: case GL_R8I: case GL_R8UI: 
            *channels = 1; return PIXEL_FORMAT_U8;
        case GL_RG8: case GL_RG8I: case GL_RG8UI: 
            *channels = 2; return PIXEL_FORMAT_U8;
        case GL_RGB8: case GL_RGB8I: case GL_RGB8UI: 
            *channels = 3; return PIXEL_FORMAT_U8;
        case GL_RGBA8: case GL_RGBA8I: case GL_RGBA8UI: 
            *channels = 4; return PIXEL_FORMAT_U8;

        case GL_R16: case GL_R16I: case GL_R16UI: 
            *channels = 1; return PIXEL_FORMAT_U16;
        case GL_RG16: case GL_RG16I: case GL_RG16UI: 
            *channels = 2; return PIXEL_FORMAT_U16;
        case GL_RGB16: case GL_RGB16I: case GL_RGB16UI: 
            *channels = 3; return PIXEL_FORMAT_U16;
        case GL_RGBA16: case GL_RGBA16I: case GL_RGBA16UI: 
            *channels = 4; return PIXEL_FORMAT_U16;

        case GL_R32UI: case GL_R32I: 
            *channels = 1; return PIXEL_FORMAT_U32;
        case GL_RG32UI: case GL_RG32I: 
            *channels = 2; return PIXEL_FORMAT_U32;
        case GL_RGB32UI: case GL_RGB32I: 
            *channels = 3; return PIXEL_FORMAT_U32;
        case GL_RGBA32UI: case GL_RGBA32I: 
            *channels = 4; return PIXEL_FORMAT_U32;

        case GL_R32F: *channels = 1; return PIXEL_FORMAT_F32;
        case GL_RG32F: *channels = 2; return PIXEL_FORMAT_F32;
        case GL_RGB32F: *channels = 3; return PIXEL_FORMAT_F32;
        case GL_RGBA32F: *channels = 4; return PIXEL_FORMAT_F32;
    }
    
    *channels = gl_format.channels;
    switch(gl_format.channel_type)
    {
        case GL_UNSIGNED_BYTE: return PIXEL_FORMAT_U8;
        case GL_UNSIGNED_SHORT: return PIXEL_FORMAT_U16;
        case GL_UNSIGNED_INT: return PIXEL_FORMAT_U32;
        case GL_FLOAT: return PIXEL_FORMAT_F32;
    }
    
    *channels = 0;
    return (Image_Pixel_Format) 0; 
}

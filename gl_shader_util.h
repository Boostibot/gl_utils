#pragma once

#pragma warning(disable:4464)
#include "../lib/file.h"
#include "../lib/hash_index.h"
#include "math.h"
#include "gl.h"
#pragma warning(default:4464)

#define SHADER_UTIL_CHANEL "SHADER"

typedef enum {
    SHADER_TYPE_RENDER,
    SHADER_TYPE_COMPUTE,
} Shader_Type;

typedef enum {
    SHADER_COMPILATION_COMPILE,
    SHADER_COMPILATION_LINK,
} Shader_Comiplation;

typedef enum {
    SHADER_ERROR_OK = 0,
    SHADER_ERROR_COMPILE_VERTEX,
    SHADER_ERROR_COMPILE_FRAGMENT,
    SHADER_ERROR_COMPILE_GEOMETRY,
    SHADER_ERROR_COMPILE_COMPUTE,
    SHADER_ERROR_LINK,
} Shader_Compilation_Error;

typedef struct Render_Shader
{
    Allocator* allocator;

    // Uses 32bit hash to cache uniform locations.
    // We dont do any hash colision resolutiion and instead
    // check with check_probabiity by iterating over all 
    // uniforms that there are no hash collisions. 
    // This is about twice as fast as querrying using opengl.
    //
    // Should a collision occur we will simply change the uniform
    // (or change the seed)
    Hash_Index32 uniform_hash;
    String_Builder_Array uniforms;
    f64 check_probabiity;

    GLuint shader;
    String_Builder name;
    Shader_Type type;

    i32 work_group_size_x;
    i32 work_group_size_y;
    i32 work_group_size_z;
} Render_Shader;

void render_shader_deinit(Render_Shader* shader)
{
    if(shader->shader != 0)
    {
        glUseProgram(0);
        glDeleteProgram(shader->shader);
    }
    array_deinit(&shader->name);
    hash_index32_deinit(&shader->uniform_hash);
    builder_array_deinit(&shader->uniforms);
    memset(shader, 0, sizeof(*shader));
}


void render_shader_preinit(Render_Shader* shader, Allocator* alloc, String name)
{
    render_shader_deinit(shader);

    if(alloc == NULL)
        alloc = allocator_get_default();

    shader->allocator = alloc;
    shader->check_probabiity = 1.0 / 10000;
    shader->name = builder_from_string(name, alloc);
    array_init(&shader->uniforms, alloc);
    hash_index32_init(&shader->uniform_hash, alloc);
}


const char* translate_shader_error(u32 code, void* context)
{
    (void) context;
    switch(code)
    {
        case SHADER_ERROR_OK:               return "SHADER_ERROR_OK";
        case SHADER_ERROR_COMPILE_VERTEX:   return "SHADER_ERROR_COMPILE_VERTEX";
        case SHADER_ERROR_COMPILE_FRAGMENT: return "SHADER_ERROR_COMPILE_FRAGMENT";
        case SHADER_ERROR_COMPILE_GEOMETRY: return "SHADER_ERROR_COMPILE_GEOMETRY";
        case SHADER_ERROR_COMPILE_COMPUTE:  return "SHADER_ERROR_COMPILE_COMPUTE";
        case SHADER_ERROR_LINK:             return "SHADER_ERROR_LINK";
        default:                            return "<SHADER_ERROR_UNKNOWN>";
    }
}

u32 shader_error_module()
{
    static u32 shader_error_module = 0;
    if(shader_error_module == 0)
        shader_error_module = error_system_register_module(translate_shader_error, "shader_util.h", NULL);

    return shader_error_module;
}

bool shader_check_compile_error(GLuint shader, Shader_Comiplation compilation, String_Builder* error)
{
    int  success = true;
    if(compilation == SHADER_COMPILATION_LINK)
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if(!success)
        {
            if(error != NULL)
            {
                array_resize(error, 1024);
                glGetProgramInfoLog(shader, (GLsizei) error->size, NULL, error->data);
                array_resize(error, strlen(error->data));
            }
        }
    }
    else
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            if(error != NULL)
            {
                array_resize(error, 1024);
                glGetShaderInfoLog(shader, (GLsizei) error->size, NULL, error->data);
                array_resize(error, strlen(error->data));
            }
        }
    }
    

    return (bool) success;
}

Error compute_shader_init(Render_Shader* shader, const char* source, String name, String_Builder* error_string)
{
    Error error = {0};
    render_shader_deinit(shader);

    GLuint program = 0;
    GLuint compute = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(compute, 1, &source, NULL);
    glCompileShader(compute);
    if(!shader_check_compile_error(compute, SHADER_COMPILATION_COMPILE, error_string))
        error = error_make(shader_error_module(), SHADER_ERROR_COMPILE_COMPUTE);
    
    if(error_is_ok(error))
    { 
        // shader Program
        program = glCreateProgram();
        glAttachShader(program, compute);
        glLinkProgram(program);
        if(!shader_check_compile_error(program, SHADER_COMPILATION_LINK, error_string))
            error = error_make(shader_error_module(), SHADER_ERROR_LINK);
    }
    // delete the shaders as they're linked into our program now and no longer necessary
    glDeleteShader(compute);

    if(error_is_ok(error))
    { 
        render_shader_preinit(shader, NULL, name);
        shader->shader = program;
        shader->type = SHADER_TYPE_RENDER;
    }
    else
    {
        glDeleteProgram(program);
    }

    return error;
}

Error render_shader_init(Render_Shader* shader, const char* vertex, const char* fragment, const char* geometry, String name, String_Builder* error_string)
{
    PERF_COUNTER_START(c);
    
    render_shader_deinit(shader);
    ASSERT(vertex != NULL && fragment != NULL);

    Error out_error = {0};
    GLuint shader_program = 0;
    GLuint shader_types[3] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER};
    GLuint shaders[3] = {0};
    const char* shader_sources[3] = {vertex, fragment, geometry};
    bool has_geometry = geometry != NULL && strlen(geometry) != 0;

    for(i32 i = 0; i < 3 && error_is_ok(out_error); i++)
    {
        //geometry shader doesnt have to exist
        if(shader_types[i] == GL_GEOMETRY_SHADER && has_geometry == false)
            break;

        shaders[i] = glCreateShader(shader_types[i]);
        glShaderSource(shaders[i], 1, &shader_sources[i], NULL);
        glCompileShader(shaders[i]);

        //Check for errors
        if(!shader_check_compile_error(shaders[i], SHADER_COMPILATION_COMPILE, error_string))
            out_error = error_make(shader_error_module(), SHADER_ERROR_COMPILE_VERTEX + i);
    }
        
    if(error_is_ok(out_error))
    {
        shader_program = glCreateProgram();
        glAttachShader(shader_program, shaders[0]);
        glAttachShader(shader_program, shaders[1]);
        if(has_geometry)
            glAttachShader(shader_program, shaders[2]);
            
        glLinkProgram(shader_program);
        if(!shader_check_compile_error(shader_program, SHADER_COMPILATION_LINK, error_string))
            out_error = error_make(shader_error_module(), SHADER_ERROR_LINK);
    }

    glDeleteShader(shaders[0]);
    glDeleteShader(shaders[1]);
    glDeleteShader(shaders[2]);

    if(error_is_ok(out_error))
    {
        render_shader_preinit(shader, NULL, name);
        shader->shader = shader_program;
        shader->type = SHADER_TYPE_RENDER;
    }
    else
        glDeleteProgram(shader_program);
    
    PERF_COUNTER_END(c);

    return out_error;
}


String_Builder render_shader_source_prepend(String data, String prepend, Allocator* allocator)
{
    String_Builder composed = {allocator};
    if(prepend.size == 0 || data.size == 0)
    {
        builder_append(&composed, data);
    }
    else
    {
        isize version_i = string_find_first(data, STRING("#version"), 0);
        isize after_version = 0;
        //if it contains no version directive start at start
        if(version_i == -1)
            after_version = 0;
        //if it does start just after the next line break
        else
        {
            ASSERT(version_i <= data.size);
            after_version = string_find_first_char(data, '\n', version_i);
            if(after_version == -1)
                after_version = data.size - 1;

            after_version += 1;
        }
            
        ASSERT(after_version <= data.size);

        String before_insertion = string_head(data, after_version);
        String after_insertion = string_tail(data, after_version);
        builder_append(&composed, before_insertion);
        builder_append(&composed, prepend);
        builder_append(&composed, STRING("\n"));
        builder_append(&composed, after_insertion);
    }

    return composed;
}


typedef struct {
    i32 max_group_invocations;
    i32 max_group_count[3];
    i32 max_group_size[3];
} Compute_Shader_Limits;

Compute_Shader_Limits compute_shader_query_limits()
{
    static Compute_Shader_Limits querried = {-1};
    if(querried.max_group_invocations < 0)
    {
        STATIC_ASSERT(sizeof(GLint) == sizeof(i32));
	    for (int i = 0; i < 3; i++) 
        {
		    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &querried.max_group_count[i]);
		    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, i, &querried.max_group_size[i]);

            querried.max_group_size[i] = MAX(querried.max_group_size[i], 1);
            querried.max_group_count[i] = MAX(querried.max_group_count[i], 1);
	    }	
	    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &querried.max_group_invocations);

        querried.max_group_invocations = MAX(querried.max_group_invocations, 1);
    }

    return querried;
}


Error compute_shader_init_from_disk(Render_Shader* shader, String source_path, isize work_group_x, isize work_group_y, isize work_group_z)
{
    Compute_Shader_Limits limits = compute_shader_query_limits();
    work_group_x = MIN(work_group_x, limits.max_group_size[0]);
    work_group_y = MIN(work_group_y, limits.max_group_size[1]);
    work_group_z = MIN(work_group_z, limits.max_group_size[2]);

    String name = path_get_name_from_path(source_path);
    String prepend = format_ephemeral(
        "#define CUSTOM_DEFINES \n"
        "#define WORK_GROUP_SIZE_X %lli \n"
        "#define WORK_GROUP_SIZE_Y %lli \n"
        "#define WORK_GROUP_SIZE_Z %lli \n",
        work_group_x, work_group_y, work_group_z
        );
        
    Allocator* scratch = allocator_get_scratch();
    String_Builder source = {scratch};
    Error error = file_read_entire(source_path, &source);
    String_Builder prepended_source = render_shader_source_prepend(string_from_builder(source), prepend, scratch);
    String_Builder error_string = {scratch};

    //LOG_DEBUG(SHADER_UTIL_CHANEL, "Compute shader source:\n%s", prepended_source.data);
    error = ERROR_AND(error) compute_shader_init(shader, cstring_from_builder(prepended_source), name, &error_string);

    if(!error_is_ok(error))
    {
        LOG_ERROR(SHADER_UTIL_CHANEL, "render_shader_init_from_disk() failed with: " ERROR_FMT, ERROR_PRINT(error));
        log_group_push();
            LOG_ERROR(SHADER_UTIL_CHANEL, "source:\"" STRING_FMT "\"", STRING_PRINT(source_path));
            LOG_ERROR(SHADER_UTIL_CHANEL, "error:\n" STRING_FMT, STRING_PRINT(error_string));
        log_group_pop();
    }
    else
    {
        shader->work_group_size_x = (i32) work_group_x;
        shader->work_group_size_y = (i32) work_group_y;
        shader->work_group_size_z = (i32) work_group_z;
    }

    array_deinit(&source);
    array_deinit(&prepended_source);
    array_deinit(&error_string);

    return error;
}

Error render_shader_init_from_disk_split(Render_Shader* shader, String vertex_path, String fragment_path, String geometry_path)
{
    PERF_COUNTER_START(c);
    Allocator* scratch = allocator_get_scratch();
    String_Builder error_text = {scratch};
    String_Builder vertex_source = {scratch};
    String_Builder fragment_source = {scratch};
    String_Builder geometry_source = {scratch};
    
    String name = path_get_name_from_path(fragment_path);
    Error vertex_error = file_read_entire(vertex_path, &vertex_source);
    Error fragment_error = file_read_entire(fragment_path, &fragment_source);
    Error geomtery_error = {0};

    if(geometry_path.size > 0)
        geomtery_error = file_read_entire(geometry_path, &geometry_source);

    Error compile_error = ERROR_AND(vertex_error) ERROR_AND(fragment_error) geomtery_error;
    if(error_is_ok(compile_error))
    {
        compile_error = render_shader_init(shader,
            cstring_from_builder(vertex_source),
            cstring_from_builder(fragment_source),
            cstring_from_builder(geometry_source),
            name,
            &error_text);
    }
        
    if(!error_is_ok(compile_error))
    {
        LOG_ERROR(SHADER_UTIL_CHANEL, "render_shader_init_from_disk() failed with: " ERROR_FMT, ERROR_PRINT(compile_error));
        log_group_push();
            LOG_ERROR(SHADER_UTIL_CHANEL, "vertex:   \"" STRING_FMT "\" " ERROR_FMT, STRING_PRINT(vertex_path), ERROR_PRINT(vertex_error));
            LOG_ERROR(SHADER_UTIL_CHANEL, "fragment: \"" STRING_FMT "\" " ERROR_FMT, STRING_PRINT(fragment_path), ERROR_PRINT(fragment_error));
            LOG_ERROR(SHADER_UTIL_CHANEL, "geometry: \"" STRING_FMT "\" " ERROR_FMT, STRING_PRINT(geometry_path), ERROR_PRINT(geomtery_error));
            LOG_ERROR(SHADER_UTIL_CHANEL, "error:\n");
            log_group_push();
                LOG_ERROR(SHADER_UTIL_CHANEL, STRING_FMT, STRING_PRINT(error_text));
            log_group_pop();
        log_group_pop();
    }

    array_deinit(&vertex_source);
    array_deinit(&fragment_source);
    array_deinit(&geometry_source);
    array_deinit(&error_text);
    
    PERF_COUNTER_END(c);
    return compile_error;
}

Error render_shader_init_from_disk(Render_Shader* shader, String path)
{
    LOG_INFO("shader", "loading: " STRING_FMT, STRING_PRINT(path));

    PERF_COUNTER_START(c);
    Allocator* scratch = allocator_get_scratch();
    String_Builder error_text = {scratch};
    String_Builder source_text = {scratch};

    String name = path_get_name_from_path(path);
    Error error = file_read_entire(path, &source_text);
    String source = string_from_builder(source_text);
    
    String_Builder vertex_source = render_shader_source_prepend(source, STRING("#define VERT"), scratch);
    String_Builder fragment_source = render_shader_source_prepend(source, STRING("#define FRAG"), scratch);
    String_Builder geometry_source = {scratch};

    if(string_find_first(source, STRING("#ifdef GEOM"), 0) != -1)
        geometry_source = render_shader_source_prepend(source, STRING("#define GEOM"), scratch);

    error = ERROR_AND(error) render_shader_init(shader,
        cstring_from_builder(vertex_source),
        cstring_from_builder(fragment_source),
        cstring_from_builder(geometry_source),
        name,
        &error_text);

    if(!error_is_ok(error))
    {
        LOG_ERROR(SHADER_UTIL_CHANEL, "render_shader_init_from_disk() failed with: " ERROR_FMT, ERROR_PRINT(error));
        log_group_push();
            LOG_ERROR(SHADER_UTIL_CHANEL, "path:   \"" STRING_FMT "\"", STRING_PRINT(path));
            LOG_ERROR(SHADER_UTIL_CHANEL, "error:\n" STRING_FMT, STRING_PRINT(error_text));
        log_group_pop();
    }


    array_deinit(&vertex_source);
    array_deinit(&fragment_source);
    array_deinit(&geometry_source);
    array_deinit(&error_text);
    array_deinit(&source_text);
    
    PERF_COUNTER_END(c);
    return error;
}

static GLuint current_used_shader = 0;
void render_shader_use(const Render_Shader* shader)
{
    ASSERT(shader->shader != 0);
    if(shader->shader != current_used_shader)
    {
        glUseProgram(shader->shader);
        current_used_shader = shader->shader;
    }
}

void render_shader_unuse(const Render_Shader* shader)
{
    ASSERT(shader->shader != 0);
    if(current_used_shader != 0)
    {
        glUseProgram(0);
        current_used_shader = 0;
    }
}

void compute_shader_dispatch(Render_Shader* compute_shader, isize size_x, isize size_y, isize size_z)
{
    GLuint num_groups_x = (GLuint) MAX(DIV_ROUND_UP(size_x, compute_shader->work_group_size_x), 1);
    GLuint num_groups_y = (GLuint) MAX(DIV_ROUND_UP(size_y, compute_shader->work_group_size_y), 1);
    GLuint num_groups_z = (GLuint) MAX(DIV_ROUND_UP(size_z, compute_shader->work_group_size_z), 1);

    render_shader_use(compute_shader);
	glDispatchCompute(num_groups_x, num_groups_y, num_groups_z);
}

GLint render_shader_get_uniform_location(Render_Shader* shader, const char* uniform)
{
    PERF_COUNTER_START(c);
    GLint location = 0;
    String uniform_str = string_make(uniform);
    u32 hash = hash32_murmur(uniform_str.data, uniform_str.size, 0);
    isize found = hash_index32_find(shader->uniform_hash, hash);

    if(found == -1)
    {
        render_shader_use(shader);
        location = glGetUniformLocation(shader->shader, uniform);
        if(location == -1)
            LOG_ERROR("RENDER", "failed to find uniform %-25s shader: %s", uniform, shader->name.data);

        array_push(&shader->uniforms, builder_from_cstring(uniform, shader->allocator));
        found = hash_index32_insert(&shader->uniform_hash, hash, location);
    }
    else
    {
        location = shader->uniform_hash.entries[found].value;
    }

    #ifdef DO_ASSERTS
    f64 random = random_f64();
    if(random <= shader->check_probabiity)
    {
        //LOG_DEBUG("RENDER", "Checking uniform %-25s for collisions shader: %s", uniform, shader->name.data);
        for(isize i = 0; i < shader->uniforms.size; i++)
        {
            String_Builder* curr_uniform = &shader->uniforms.data[i];
            u32 curr_hash = hash32_murmur(curr_uniform->data, curr_uniform->size, 0);
            if(curr_hash == hash && string_is_equal(string_from_builder(*curr_uniform), uniform_str) == false)
                LOG_DEBUG("RENDER", "uniform %s hash coliding with uniform %s in shader %s", uniform, curr_uniform->data, shader->name.data);
        }
    }
    #endif
    
    PERF_COUNTER_END(c);

    return location;
}


bool render_shader_set_i32(Render_Shader* shader, const char* name, i32 val)
{
    render_shader_use(shader);
    GLint location = render_shader_get_uniform_location(shader, name);
    if(location == -1) 
        return false;

    glUniform1i(location, (GLint) val);
    return true;
}
    
bool render_shader_set_f32(Render_Shader* shader, const char* name, f32 val)
{
    render_shader_use(shader);
    GLint location = render_shader_get_uniform_location(shader, name);
    if(location == -1)
        return false;

    glUniform1f(location, (GLfloat) val);
    return true;
}

bool render_shader_set_vec3(Render_Shader* shader, const char* name, Vec3 val)
{
    render_shader_use(shader);
    GLint location = render_shader_get_uniform_location(shader, name);
    if(location == -1)
        return false;

    glUniform3fv(location, 1, AS_FLOATS(val));
    return true;
}

bool render_shader_set_mat4(Render_Shader* shader, const char* name, Mat4 val)
{
    render_shader_use(shader);
    GLint location = render_shader_get_uniform_location(shader, name);
    if(location == -1)
        return false;

    glUniformMatrix4fv(location, 1, GL_FALSE, AS_FLOATS(val));
    return true;
}
    
bool render_shader_set_mat3(Render_Shader* shader, const char* name, Mat3 val)
{
    render_shader_use(shader);
    GLint location = render_shader_get_uniform_location(shader, name);
    if(location == -1)
        return false;

    glUniformMatrix3fv(location, 1, GL_FALSE, AS_FLOATS(val));
    return true;
}

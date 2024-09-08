#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gl.h"

typedef enum {
    SHADER_TYPE_RENDER,
    SHADER_TYPE_COMPUTE,
} Shader_Type;

typedef struct GL_Shader {
    GLuint handle;

    char name[64];

    int32_t block_size_size_x;
    int32_t block_size_size_y;
    int32_t block_size_size_z;
} GL_Shader;

enum {MAX_SHADER_STAGES = 7};
typedef struct Shader_Errors {
    const char* sources[MAX_SHADER_STAGES];
    const char* stage_names[MAX_SHADER_STAGES];
    GLuint      stages[MAX_SHADER_STAGES];
    int _;
    union {
        const char* errors[MAX_SHADER_STAGES];
        struct {
            const char* link;  
            const char* vertex;  
            const char* fragment;  
            const char* geometry;  
            const char* compute;  
            const char* tess_control;  
            const char* tess_eval;  
        };
    };

    char        data[4096];
} Shader_Errors;

const char* shader_stage_to_string(GLuint stage)
{
    switch(stage)
    {
        case GL_LINK_STATUS:            return "link";
        case GL_VERTEX_SHADER:          return "vertex";
        case GL_FRAGMENT_SHADER:        return "fragment";
        case GL_GEOMETRY_SHADER:        return "geometry";
        case GL_COMPUTE_SHADER:         return "compute";
        case GL_TESS_CONTROL_SHADER:    return "tesselation-control";
        case GL_TESS_EVALUATION_SHADER: return "tesselation-eval";
        default:                        return "<invalid>";
    }
}

GLuint shader_compile(const char* sources[], GLuint shader_stages[], int sources_count, Shader_Errors* errors)
{
    if(sources_count > MAX_SHADER_STAGES)
        return 0;

    if(errors)
        memset(errors, 0, sizeof *errors);

    GLuint shaders[MAX_SHADER_STAGES] = {0};
    int okay = true;
    size_t error_messages_from = 0;
    for(int i = 0; i < sources_count; i++)
    {
        GLuint stage = shader_stages[i];
        shaders[i] = glCreateShader(shader_stages[i]);
        glShaderSource(shaders[i], 1, &sources[i], NULL);
        glCompileShader(shaders[i]);

        int success = true;
        glGetShaderiv(shaders[i], GL_COMPILE_STATUS, &success);
        if(errors && !success && error_messages_from < sizeof(errors->data))
        {
            int index = 0;
            switch(stage)
            {
                default:                        index = 0; break;
                case GL_VERTEX_SHADER:          index = 1; break;
                case GL_FRAGMENT_SHADER:        index = 2; break;
                case GL_GEOMETRY_SHADER:        index = 3; break;
                case GL_COMPUTE_SHADER:         index = 4; break;
                case GL_TESS_CONTROL_SHADER:    index = 5; break;
                case GL_TESS_EVALUATION_SHADER: index = 6; break;
            }

            glGetShaderInfoLog(shaders[i], (GLsizei) (sizeof(errors->data) - error_messages_from), NULL, errors->data + error_messages_from);
            errors->errors[index] = errors->data + error_messages_from;
            errors->stages[index] = stage;
            errors->sources[index] = sources[i];
            errors->stage_names[index] = shader_stage_to_string(stage);

            error_messages_from += strlen(errors->errors[index]) + 1;
        }

        okay = okay && success;
    }

    GLuint pogram = 0;
    if(okay)
    {
        pogram = glCreateProgram();
        for(int i = 0; i < sources_count; i++)
            glAttachShader(pogram, shaders[i]);
        
        glLinkProgram(pogram);

        int success = true;
        glGetProgramiv(pogram, GL_LINK_STATUS, &success);
        if(errors && !success && error_messages_from < sizeof(errors->data))
        {
            glGetProgramInfoLog(pogram, (GLsizei) (sizeof(errors->data) - error_messages_from), NULL, errors->data + error_messages_from);
            errors->errors[0] = errors->data + error_messages_from;
            errors->stages[0] = GL_LINK_STATUS;
            errors->stage_names[0] = shader_stage_to_string(GL_LINK_STATUS);
        }
    }
    
    for(int i = 0; i < sources_count; i++)
        glDeleteShader(shaders[i]);

    return pogram;
}

GLuint shader_compile_render(const char* vertex, const char* fragment, const char* geometry_or_null, Shader_Errors* errors_or_null)
{
    const char* sources[] = {vertex, fragment, geometry_or_null};
    GLuint shader_types[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER};
    return shader_compile(sources, shader_types, geometry_or_null ? 3 : 2, errors_or_null);
}

GLuint shader_compile_compute(const char* source, Shader_Errors* errors_or_null)
{
    GLuint shader_type = GL_COMPUTE_SHADER;
    return shader_compile(&source, &shader_type, 1, errors_or_null);
}

typedef struct Compute_Shader_Limits {
    int32_t max_group_invocations;
    int32_t max_group_count[3];
    int32_t max_group_size[3];
} Compute_Shader_Limits;

Compute_Shader_Limits compute_shader_query_limits()
{
    static Compute_Shader_Limits querried = {-1};
    if(querried.max_group_invocations < 0)
    {
        STATIC_ASSERT(sizeof(GLint) == sizeof(int32_t));
	    for (GLuint i = 0; i < 3; i++) 
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

static const GL_Shader* current_used_shader = NULL;
static GLuint current_used_shader_handle = 0;
void render_shader_use(const GL_Shader* shader)
{
    ASSERT(shader->handle != 0);
    if(shader->handle != current_used_shader_handle)
    {
        glUseProgram(shader->handle);
        current_used_shader = shader;
        current_used_shader_handle = shader->handle;
    }
}

void render_shader_unuse(const GL_Shader* shader)
{
    ASSERT(shader->handle != 0);
    if(current_used_shader_handle != 0)
    {
        glUseProgram(0);
        current_used_shader = NULL;
        current_used_shader_handle = 0;
    }
}

void compute_shader_dispatch(GL_Shader* compute_shader, isize size_x, isize size_y, isize size_z)
{
    GLuint num_groups_x = (GLuint) MAX(DIV_CEIL(size_x, compute_shader->block_size_size_x), 1);
    GLuint num_groups_y = (GLuint) MAX(DIV_CEIL(size_y, compute_shader->block_size_size_y), 1);
    GLuint num_groups_z = (GLuint) MAX(DIV_CEIL(size_z, compute_shader->block_size_size_z), 1);

    render_shader_use(compute_shader);
	glDispatchCompute(num_groups_x, num_groups_y, num_groups_z);
}

bool render_shader_set_i32(GL_Shader* shader, const char* name, i32 val)
{
    render_shader_use(shader);
    GLint location = glGetUniformLocation(shader->handle, name);
    if(location == -1) 
        return false;

    glUniform1i(location, (GLint) val);
    return true;
}
    
bool render_shader_set_f32(GL_Shader* shader, const char* name, f32 val)
{
    render_shader_use(shader);
    GLint location = glGetUniformLocation(shader->handle, name);
    if(location == -1)
        return false;

    glUniform1f(location, (GLfloat) val);
    return true;
}

bool render_shader_set_vec3(GL_Shader* shader, const char* name, Vec3 val)
{
    render_shader_use(shader);
    GLint location = glGetUniformLocation(shader->handle, name);
    if(location == -1)
        return false;

    glUniform3fv(location, 1, val.floats);
    return true;
}
    
bool render_shader_set_mat3(GL_Shader* shader, const char* name, Mat3 val)
{
    render_shader_use(shader);
    GLint location = glGetUniformLocation(shader->handle, name);
    if(location == -1)
        return false;

    glUniformMatrix3fv(location, 1, GL_FALSE, val.floats);
    return true;
}

bool render_shader_set_mat4(GL_Shader* shader, const char* name, Mat4 val)
{
    render_shader_use(shader);
    GLint location = glGetUniformLocation(shader->handle, name);
    if(location == -1)
        return false;

    glUniformMatrix4fv(location, 1, GL_FALSE, val.floats);
    return true;
}

#include "../lib/file.h"
#include "../lib/hash.h"
#include "../lib/random.h"
#include "../lib/math.h"
#include "../lib/arena.h"
#include "../lib/vformat.h"
#include "../lib/hash.h"
#include "../lib/parse.h"


typedef struct Shader_File_Cache_Entry{
    String_Builder contents;
    String_Builder processed;
    Path_Builder       full_path;
    Platform_File_Info file_info;
    Platform_Error     file_error;
    bool has_version;
    bool has_contents;
    bool has_processed;
    bool okay;
    isize version_line;
    isize version_offset;
    isize version_after_offset;
} Shader_File_Cache_Entry;

typedef Array(Shader_File_Cache_Entry) Shader_File_Cache;
typedef Array(Path) Path_Array;

typedef struct _Shader_File_Recursion{
    Path       relative_to;
    Path_Array visited_paths;
    Log        log;
} _Shader_File_Recursion;

void _source_preprocess_log(_Shader_File_Recursion* recursion, const char* format, ...)
{
    SCRATCH_ARENA(arena) 
    {
        String_Builder message = builder_make(arena.alloc, 0);
        Path_Builder first = path_builder_make(arena.alloc, 0);
        Path_Builder last = path_builder_make(arena.alloc, 0);

        if(recursion->visited_paths.len > 0)
            path_make_relative_into(&first, recursion->relative_to, recursion->visited_paths.data[0]);

        format_append_into(&message, "'%s': ", last.data);

        va_list args;
        va_start(args, format);
        vformat_append_into(&message, format, args);
        va_end(args);
        
        if(recursion->visited_paths.len >= 2)
        {
            path_make_relative_into(&last, recursion->relative_to, *array_last(recursion->visited_paths));
            format_append_into(&message, " (included from '%s')", first.data);
        }

        LOG(recursion->log, "%s", message.data);
    }
}

#include "../lib/parse.h"
i32 _shader_file_load_into_cache_and_handle_inclusion_recursion(Shader_File_Cache* cache, _Shader_File_Recursion* recursion, Path current_dir, Path path)
{
    LOG_DEBUG("SHADER", "Requested load of shader '%.*s' relative to '%.*s'",  STRING_PRINT(path), STRING_PRINT(current_dir));
    enum {
        MAX_RECURSION = 1000, 
        EXPECTED_MAX_RECURSION = 20
    };
    
    i32 result = -1;
    SCRATCH_ARENA(arena) 
    {
        Path_Builder full_path = path_make_absolute(arena.alloc, current_dir, path);
        Path_Builder display_path = path_make_relative(arena.alloc, recursion->relative_to, full_path.path);
        Path full_path_directory = path_strip_to_containing_directory(full_path.path);
        
        array_reserve(&recursion->visited_paths, EXPECTED_MAX_RECURSION);
        array_push(&recursion->visited_paths, full_path.path);

        //Check for cyclical paths
        bool has_cyclical_include = false;
        for(isize i = 0; i < recursion->visited_paths.len - 1; i++)
        {
            Path visited = recursion->visited_paths.data[i];
            if(path_is_equal_except_prefix(full_path.path, visited))
            {
                result = (i32) i;
                String_Builder include_chain = {arena.alloc};
                for(isize j = i; j < recursion->visited_paths.len; j++)
                {
                    Path curr_path = recursion->visited_paths.data[i];
                    const char* relative = path_make_relative(arena.alloc, recursion->relative_to, curr_path).data;
                    if(j != i)
                        format_into(&include_chain, " -> '%s'", relative);
                    else
                        format_into(&include_chain, "'%s'", relative);
                }

                _source_preprocess_log(recursion, "Error: cyclical include detected! Include chain %s", include_chain.data);
                has_cyclical_include = true;
                break;
            }
        }
        
        if(has_cyclical_include == false)
        {
            //Look for full path in the cache
            for(isize i = 0; i < cache->len; i++)
            {
                Shader_File_Cache_Entry* entry = &cache->data[i];
                if(path_is_equal_except_prefix(full_path.path, entry->full_path.path))
                {
                    LOG_DEBUG("SHADER", "Found cached shader file '%s' has_contents:%i has_processed:%i", 
                        display_path.data, (int) entry->has_contents, (int) entry->has_processed);
                    
                    result = (i32) i;
                    break;
                }
            }
        
            //If not found add it
            if(result == -1)
            {
                Shader_File_Cache_Entry new_entry = {0};
                new_entry.full_path = path_builder_dup(cache->allocator, full_path);
                new_entry.contents = builder_make(cache->allocator, 0);
                new_entry.processed = builder_make(cache->allocator, 0);
                new_entry.okay = true;

                result = (i32) cache->len;
                array_push(cache, new_entry);
                
                LOG_DEBUG("SHADER", "Found new shader file '%s'",  display_path.data);
            }

            //Read the entry if not read already
            Shader_File_Cache_Entry* entry = &cache->data[result];
            if(entry->has_contents == false && entry->file_error == 0)
            {
                entry->file_error = file_read_entire_into_no_log(full_path.string, &entry->contents, &entry->file_info);
                entry->has_contents = entry->file_error == 0;
            
                if(entry->file_error)
                {
                    _source_preprocess_log(recursion, "Error: could not read file '%s': %s", display_path.data, platform_translate_error(entry->file_error));
                    entry->okay = false;
                }
                else
                    LOG_DEBUG("SHADER", "Added a new shader file '%s'", display_path.data);
            }
        
            //Process the file 
            if(entry->has_processed == false && entry->has_contents)
            {
                LOG_DEBUG("SHADER", "Parsing shader file '%s'", display_path.data);
                String source = entry->contents.string;

                builder_clear(&entry->processed);
                builder_reserve(&entry->processed, source.len + 256);
                for(Line_Iterator it = {0}; line_iterator_get_line(&it, source); )
                {
                    String line = it.line;
                    isize hash_i    = string_find_first(line, STRING("#"), 0);
                    isize version_i = hash_i + 1;
                    isize include_i = hash_i + 1;
                    if(hash_i != -1)
                    {
                        match_whitespace(line, &version_i);
                        version_i = include_i;
                    }

                    if(hash_i != -1 && match_sequence(line, &version_i, STRING("version")))
                    {
                        if(entry->has_version)
                            _source_preprocess_log(recursion, "Error: duplicate version string on line %i. Ignoring.", (int)it.line_number);
                        else
                        {
                            entry->has_version = true;
                            entry->version_line = it.line_number;
                            entry->version_offset = entry->processed.len;
                            builder_append_line(&entry->processed, line);
                            entry->version_after_offset = entry->processed.len;
                        }
                    }
                    else if(hash_i != -1 && match_sequence(line, &include_i, STRING("include")))
                    {
                        isize include_str_from = include_i;
                        match_whitespace(line, &include_str_from);
                        bool okay = match_char(line, &include_str_from, '"');
                            
                        isize include_str_to = include_str_from;
                        okay = okay && match_any_of_custom(line, &include_str_to, STRING("\""), MATCH_INVERTED);
                    
                        String str = string_safe_range(line, include_str_from, include_str_to);
                        LOG_DEBUG("SHADER", "Found include '%.*s'",  STRING_PRINT(str));
                        
                        Path include_path = path_parse(string_safe_range(line, include_str_from, include_str_to));
                        if(okay == false)
                        {
                            entry->okay = false;
                            _source_preprocess_log(recursion, "Error: malformed include statement '%.*s' on line %i. Ignoring.", STRING_PRINT(line), (int)it.line_number);
                        }
                        else if(recursion->visited_paths.len > MAX_RECURSION)
                        {
                            entry->okay = false;
                            _source_preprocess_log(recursion, "Error: recursion level %i too deep while including file '%.*s' on line on line %i", 
                                (int) recursion->visited_paths.len, STRING_PRINT(include_path.string), (int)it.line_number);
                        }
                        else
                        {
                            i32 nested_result = _shader_file_load_into_cache_and_handle_inclusion_recursion(cache, recursion, full_path_directory, include_path);
                            Shader_File_Cache_Entry* nested_entry = &cache->data[nested_result];

                            builder_append_line(&entry->processed, nested_entry->processed.string);
                            entry->okay = entry->okay && nested_entry->okay;
                        }
                    }
                    else
                    {
                        builder_append_line(&entry->processed, line);
                    }
                }

                entry->has_processed = true;
            }
        }

        array_pop(&recursion->visited_paths);
    }

    ASSERT(result != -1);
    return result;
}

i32 shader_file_load_into_cache_and_handle_inclusion(Shader_File_Cache* cache, Path current_dir, Path path)
{
    i32 result = 0;
    SCRATCH_ARENA(arena) 
    {
        _Shader_File_Recursion recursion = {0};
        recursion.log = log_error("SHADER");
        recursion.relative_to = current_dir;
        recursion.visited_paths.allocator = arena.alloc;

        result = _shader_file_load_into_cache_and_handle_inclusion_recursion(cache, &recursion, current_dir, path);
    }

    return result;
}

String_Builder shader_source_prepend(Allocator* alloc, Shader_File_Cache_Entry* entry, const String* prepends, isize prepend_count, String default_version)
{
    isize combined_size = 0;
    for(isize i = 0; i < prepend_count; i++)
        combined_size += prepends[i].len;

    String preprocessed = entry->processed.string;
    String_Builder prepended = builder_make(alloc, combined_size + preprocessed.len);
    builder_append(&prepended, string_head(preprocessed, entry->version_after_offset));

    if(entry->has_version == false)
        builder_append_line(&prepended, default_version);
    
    for(isize i = 0; i < prepend_count; i++)
        builder_append_line(&prepended, prepends[i]);

    builder_append(&prepended, string_tail(preprocessed, entry->version_after_offset));
    return prepended;
}

String_Builder add_line_numbers(Allocator* alloc, String string)
{
    String_Builder builder = builder_make(alloc, string.len*4/3 + 50);
    for(Line_Iterator it = {0}; line_iterator_get_line(&it, string); )
        format_append_into(&builder, "%4i %.*s\n", (int)it.line_number, STRING_PRINT(it.line));
    return builder;
}

bool compute_shader_init_from_disk(Shader_File_Cache* cache, GL_Shader* shader, String path, isize block_size_x, isize block_size_y, isize block_size_z)
{
    bool state = true;
    PROFILE_START();
    SCRATCH_ARENA(arena)
    {
        Path path_parsed = path_parse(path);
        i32 preprocessed_i = shader_file_load_into_cache_and_handle_inclusion(cache, path_get_current_working_directory(), path_parsed);
        Shader_File_Cache_Entry* entry = &cache->data[preprocessed_i];
        state = entry->okay;

        if(state)
        {
            Compute_Shader_Limits limits = compute_shader_query_limits();
            block_size_x = MIN(block_size_x, limits.max_group_size[0]);
            block_size_y = MIN(block_size_y, limits.max_group_size[1]);
            block_size_z = MIN(block_size_z, limits.max_group_size[2]);

            String name = path_get_filename_without_extension(path_parsed);
            String prepend = format(arena.alloc,
                "\n #define CUSTOM_DEFINES"
                "\n #define BLOCK_SIZE_X %lli"
                "\n #define BLOCK_SIZE_Y %lli"
                "\n #define BLOCK_SIZE_Z %lli",
                block_size_x, block_size_y, block_size_z
            ).string;
    
            String_Builder prepended = shader_source_prepend(arena.alloc, entry, &prepend, 1, STRING("#version 4.0"));
            Shader_Errors errors = {0};
            GLuint shader_handle = shader_compile_compute(prepended.data, &errors);
            if(shader_handle == 0)
            {   
                LOG_ERROR("SHADER", "Compilation of shader '%.*s' failed with errors: \n%s\n%s", STRING_PRINT(path), errors.compute, errors.link);
                LOG_INFO("SHADER", "Source: \n%s", add_line_numbers(arena.alloc, prepended.string).data);
            }
            else
            {
                shader->handle = shader_handle;
                string_to_null_terminated(shader->name, sizeof(shader->name), name);

                shader->block_size_size_x = (i32) block_size_x;
                shader->block_size_size_y = (i32) block_size_y;
                shader->block_size_size_z = (i32) block_size_z;
            }
        }
    }
    PROFILE_END();
    ASSERT(state);
    return state;
}

bool render_shader_init_from_disk_with_geometry(Shader_File_Cache* cache, GL_Shader* shader, String path, bool has_geometry)
{
    bool state = true;
    PROFILE_START();
    SCRATCH_ARENA(arena)
    {
        Path path_parsed = path_parse(path);
        i32 preprocessed_i = shader_file_load_into_cache_and_handle_inclusion(cache, path_get_current_working_directory(), path_parsed);
        Shader_File_Cache_Entry* entry = &cache->data[preprocessed_i];
        state = entry->okay;

        if(state)
        {
            String name = path_get_filename_without_extension(path_parsed);
            
            String vertex_prepend = STRING("#define VERT");
            String fragment_prepend = STRING("#define FRAG");
            String geometry_prepend = STRING("#define GEOM");
            String version = STRING("#version 4.0");

            String_Builder vertex_source = shader_source_prepend(arena.alloc, entry, &vertex_prepend, 1, version);
            String_Builder fragment_source = shader_source_prepend(arena.alloc, entry, &fragment_prepend, 1, version);
            String_Builder geometry_source = {0};
            
            if(has_geometry)
                geometry_source = shader_source_prepend(arena.alloc, entry, &geometry_prepend, 1, version);

            Shader_Errors errors = {0};
            GLuint shader_handle = shader_compile_render(vertex_source.data, fragment_source.data, geometry_source.data, &errors);
            if(shader_handle == 0)
            {   
                LOG_ERROR("SHADER", "Compilation of shader '%.*s' failed with errors: \n%s\n%s\n%s\n%s", STRING_PRINT(path), errors.vertex, errors.fragment, errors.vertex, errors.link);
                LOG_INFO("SHADER", "Source: \n%s", add_line_numbers(arena.alloc, vertex_source.string).data);
            }
            else
            {
                shader->handle = shader_handle;
                string_to_null_terminated(shader->name, sizeof(shader->name), name);
            }
        }
    }
    PROFILE_END();
    ASSERT(state);
    return state;
}


bool render_shader_init_from_disk(Shader_File_Cache* cache, GL_Shader* shader, String path)
{
    return render_shader_init_from_disk_with_geometry(cache, shader, path, false);
}

void shader_file_cache_deinit(Shader_File_Cache* file_cache)
{
    for(isize i = 0; i < file_cache->len; i++)
    {
        Shader_File_Cache_Entry* entry = &file_cache->data[i];
        builder_deinit(&entry->contents);
        builder_deinit(&entry->processed);
        path_builder_deinit(&entry->full_path);
    }
}

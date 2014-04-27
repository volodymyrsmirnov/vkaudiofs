//
//  vk_api.c
//  VKAudioFS
//
//  Created by Vladimir Smirnov on 4/24/14.
//  Copyright (c) 2014 Vladimir Smirnov. All rights reserved.
//

#include "config.h"

#include <string.h>
#include <pthread.h>

#include <glib.h>
#include <fuse.h>
#include <json.h>
#include <curl/curl.h>

#include "vk_api.h"

gsize vkaudiofs_write_data(gpointer ptr, gsize size, gsize nmemb, vkaudiofs_api_response *response_data)
{
    gchar *temp = NULL;
    gsize index = response_data->size;
    gsize bytes_number = size * nmemb;

    response_data->size += bytes_number;

    temp = g_realloc(response_data->data, response_data->size + 1);
    
    response_data->data = temp;
    memcpy((response_data->data + index), ptr, bytes_number);
    response_data->data[response_data->size] = '\0';
    
    return bytes_number;
}

gint vkaudiofs_call_api(gchar *method, gchar *query, gchar **response)
{
    CURL *curl = curl_easy_init();
    CURLcode response_status = CURLE_OK;
    gchar *api_query_url = g_strdup_printf("https://api.vk.com/method/%s?v=5.2.1&%s&access_token=%s", method, query, vkaudiofs_config.access_token);
    
    vkaudiofs_api_response response_data;
    response_data.size = 0;
    response_data.data = g_malloc(4096);
    response_data.data[0] = '\0';
    
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, api_query_url);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, FALSE);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback)vkaudiofs_write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        
        response_status = curl_easy_perform(curl);
        
        curl_easy_cleanup(curl);
    
        g_free(api_query_url);
        
        if (response_status == CURLE_OK)
        {
            *response = response_data.data;
            
            return VKAUDIOFS_API_RESULT_SUCCESS;
        }
    }
    
    return VKAUDIOFS_API_RESULT_ERROR;
}

gint vkaudiofs_get_audio_files(struct vkaudiofs_config_t *config)
{
    gint audio_query_result = VKAUDIOFS_API_RESULT_ERROR;
    json_object * response_item_json_obj = NULL;
    json_object *audio_response_object = NULL;
    gchar *audio_query_string = g_strdup_printf("owner_id=%d&offset=0&count=6000", config->user_id);
    gchar *audio_response_string = NULL;
    vkaudiofs_audio_file *audio_files = NULL;
    
    audio_query_result = vkaudiofs_call_api("audio.get", audio_query_string, &audio_response_string);
    
    g_free(audio_query_string);
    
    if (audio_query_result == VKAUDIOFS_API_RESULT_SUCCESS)
    {
        audio_response_object = json_tokener_parse(audio_response_string);
        audio_response_object = json_object_object_get(audio_response_object, "response");
        
        g_free(audio_response_string);
        
        if (audio_response_object == NULL)
        {
            return VKAUDIOFS_API_RESULT_ERROR;
        }
        
        audio_response_object = json_object_object_get(audio_response_object, "items");
        
        audio_files = g_malloc(sizeof(vkaudiofs_audio_file) * json_object_array_length(audio_response_object));
        
        for (gint idx = 0; idx < json_object_array_length(audio_response_object); idx++)
        {
            response_item_json_obj = json_object_array_get_idx(audio_response_object, idx);
            
            audio_files[idx].id = json_object_get_int(json_object_object_get(response_item_json_obj, "id"));
            audio_files[idx].artist = (gchar *)json_object_get_string(json_object_object_get(response_item_json_obj, "artist"));
            audio_files[idx].title = (gchar *)json_object_get_string(json_object_object_get(response_item_json_obj, "title"));
            audio_files[idx].url = (gchar *)json_object_get_string(json_object_object_get(response_item_json_obj, "url"));
            
            audio_files[idx].file_name = g_strdelimit(g_strdup_printf("%s - %s.%d.mp3", audio_files[idx].artist, audio_files[idx].title, audio_files[idx].id), "/ ", '_');
            
            audio_files[idx].curl_instance = NULL;

            pthread_mutex_init(&audio_files[idx].lock, NULL);
            
            g_hash_table_replace(config->files_id_table, &audio_files[idx].id, &audio_files[idx]);
            g_hash_table_replace(config->files_name_table, audio_files[idx].file_name, &audio_files[idx]);
        }
        
        config->files_number = g_hash_table_size(config->files_id_table);
                
        return VKAUDIOFS_API_RESULT_SUCCESS;
    }
    
    return VKAUDIOFS_API_RESULT_ERROR;
}

vkaudiofs_audio_file * vkaudiofs_get_file_by_name(gchar *file_name)
{
    return g_hash_table_lookup(vkaudiofs_config.files_name_table, file_name);
}

gsize vkaudiofs_content_length_header(gpointer ptr, gsize size, gsize nmemb, gpointer stream)
{
    gint result = 0;
    glong content_length = 0;

    result = sscanf((gchar *)ptr, "Content-Length: %ld\n", &content_length);
    
    if (result)
    {
        *((gsize *) stream) = content_length;
    }
    
    return size * nmemb;
}

gsize vkaudiofs_dummy_write(gchar *ptr, gsize size, gsize nmemb, gpointer userdata)
{
    (void) ptr;
    (void) userdata;
    
    return size * nmemb;
}

gsize vkaudiofs_get_remote_file_size(gchar *url)
{
    CURL *curl = curl_easy_init();
    CURLcode response_status = CURLE_OK;
    gsize content_length = 0;
    
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HEADER, 1);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, FALSE);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback)vkaudiofs_dummy_write);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, (curl_write_callback)vkaudiofs_content_length_header);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &content_length);
        
        response_status = curl_easy_perform(curl);
                
        curl_easy_cleanup(curl);
        
        if (response_status == CURLE_OK)
        {
             return content_length;
        }
    }
    
    return 0;
}

gsize vkaudiofs_get_remote_file(vkaudiofs_audio_file *audio_file, gsize size, off_t offset, gchar **buffer)
{
    CURLcode response_status = CURLE_OK;
    gchar *request_range = g_strdup_printf("%lld-%llu", offset, offset + size - 1);
    
    struct vkaudiofs_api_response response_data;
    response_data.size = 0;
    response_data.data = g_malloc(size);
    
    curl_easy_reset(audio_file->curl_instance);
    
    curl_easy_setopt(audio_file->curl_instance, CURLOPT_URL, audio_file->url);
    curl_easy_setopt(audio_file->curl_instance, CURLOPT_VERBOSE, FALSE);
    curl_easy_setopt(audio_file->curl_instance, CURLOPT_RANGE, request_range);
    curl_easy_setopt(audio_file->curl_instance, CURLOPT_WRITEFUNCTION, (curl_write_callback)vkaudiofs_write_data);
    curl_easy_setopt(audio_file->curl_instance, CURLOPT_WRITEDATA, &response_data);
    
    response_status = curl_easy_perform(audio_file->curl_instance);
    
    g_free(request_range);
    
    if (response_status == CURLE_OK)
    {
        *buffer = response_data.data;
        
        return response_data.size;
    }
   
    return 0;
}

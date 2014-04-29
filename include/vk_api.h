//
//  vk_api.h
//  VKAudioFS
//
//  Created by Vladimir Smirnov on 4/24/14.
//  Copyright (c) 2014 Vladimir Smirnov. All rights reserved.
//

#ifndef VKAudioFS_vk_api_h
#define VKAudioFS_vk_api_h

enum {
    KEY_HELP,
    KEY_VERSION,
    KEY_OAUTH_URL,
};

enum {
    VKAUDIOFS_API_RESULT_SUCCESS,
    VKAUDIOFS_API_RESULT_ERROR,
};

struct vkaudiofs_config_t {
    gint user_id;
    gchar *access_token;
    gint files_number;
    GHashTable *files_id_table;
    GHashTable *files_name_table;
} vkaudiofs_config;

typedef struct vkaudiofs_audio_file {
    gint id;
    gchar *artist;
    gchar *title;
    gchar *url;
    gchar *name;
    gsize size;
    glong time;
    
    pthread_mutex_t lock;

    CURL *curl_instance;
} vkaudiofs_audio_file;

typedef struct vkaudiofs_api_response {
    gsize size;
    gchar *data;
} vkaudiofs_api_response;

gint vkaudiofs_get_audio_files(struct vkaudiofs_config_t *config);

vkaudiofs_audio_file * vkaudiofs_get_file_by_name(gchar *file_name);

gint vkaudiofs_get_remote_file_size(vkaudiofs_audio_file *audio_file);
gsize vkaudiofs_get_remote_file(vkaudiofs_audio_file *audio_file, gsize size, off_t offset, gchar **buffer);

#endif

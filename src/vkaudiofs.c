//
//  main.c
//  VKAudioFS
//
//  Created by Vladimir Smirnov on 4/24/14.
//  Copyright (c) 2014 Vladimir Smirnov. All rights reserved.
//

#include "config.h"

#include <string.h>
#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include <glib.h>
#include <fuse.h>
#include <json.h>
#include <curl/curl.h>

#include "vk_api.h"

static gint vkaudiofs_oper_getattr(const gchar *path, struct stat *stbuf)
{
    vkaudiofs_audio_file *file_item = NULL;
    gchar *file_name = NULL;
    
    memset(stbuf, 0, sizeof(struct stat));
    
	if (g_strcmp0(path, "/") == 0)
    {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
    else
    {
        file_name = g_path_get_basename(path);
        
        file_item = g_hash_table_lookup(vkaudiofs_config.files_name_table, file_name);
        
        g_free(file_name);
        
        if (file_item == NULL)
        {
            return -ENOENT;
        }

		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
        stbuf->st_size = file_item->size;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        
        stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);
    }
    
    return 0;
}

static gint vkaudiofs_oper_release(const gchar *path, struct fuse_file_info *fi)
{
    (void) fi;
    
    gchar *file_name = g_path_get_basename(path);
    
    vkaudiofs_audio_file *file_item = vkaudiofs_get_file_by_name(file_name);
    
    g_free(file_name);
    
    if (file_item == NULL)
    {
        return -ENOENT;
    }
    
    pthread_mutex_lock(&file_item->lock);
    
    if (file_item->curl_instance != NULL)
    {
        curl_easy_cleanup(file_item->curl_instance);
        file_item->curl_instance = NULL;
    }
    
    pthread_mutex_unlock(&file_item->lock);
    
    return 0;
}

static gint vkaudiofs_oper_open(const gchar *path, struct fuse_file_info *fi)
{
    (void) fi;
   
    gchar *file_name = g_path_get_basename(path);
    
    vkaudiofs_audio_file *file_item = vkaudiofs_get_file_by_name(file_name);
    
    g_free(file_name);
    
    if (file_item == NULL)
    {
        return -ENOENT;
    }
    
    pthread_mutex_lock(&file_item->lock);
    
    if(file_item->curl_instance == NULL)
    {
        file_item->curl_instance = curl_easy_init();
    }
    
    pthread_mutex_unlock(&file_item->lock);

	if ((fi->flags & 3) != O_RDONLY)
    {
		return -EACCES;
    }
    
	return 0;
}

static gint vkaudiofs_oper_readdir(const gchar *path, gpointer buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;
    
    GList *files = NULL;
    GList *iterator = NULL;
    vkaudiofs_audio_file *audio_file = NULL;
    
    if (g_strcmp0(path, "/"))
    {
        return -ENOENT;
    }
    
    filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
    
    files = g_hash_table_get_values(vkaudiofs_config.files_id_table);
    
    for(iterator = files; iterator; iterator = iterator->next)
    {
        audio_file = iterator->data;
        
        filler(buf, audio_file->file_name, NULL, 0);
    }
    
    g_free(iterator);

    return 0;
}

static gint vkaudiofs_oper_read(const gchar* path, gchar* rbuf, gsize size, off_t offset, struct fuse_file_info *fi)
{
    (void) fi;
    
    gchar *buffer = NULL;
    gchar *file_name = g_path_get_basename(path);
    gsize bytes_received = 0;
    vkaudiofs_audio_file *file_item = vkaudiofs_get_file_by_name(file_name);
    
    g_free(file_name);
    
    if (file_item == NULL)
    {
        return -EIO;
    }
    
    pthread_mutex_lock(&file_item->lock);
    
    bytes_received = vkaudiofs_get_remote_file(file_item, size, offset, &buffer);
    
    memcpy(rbuf, buffer, bytes_received);
    
    pthread_mutex_unlock(&file_item->lock);
    
    g_free(buffer);
    
    return (gint)bytes_received;
}

static struct fuse_operations vkaudiofs_oper = {
    .read = vkaudiofs_oper_read,
    .getattr = vkaudiofs_oper_getattr,
    .readdir = vkaudiofs_oper_readdir,
    .open = vkaudiofs_oper_open,
    .release = vkaudiofs_oper_release
};

static struct fuse_opt vkaudiofs_opts[] = {
    {"--user_id %i", offsetof(struct vkaudiofs_config_t, user_id), 0},
    {"--access_token %s", offsetof(struct vkaudiofs_config_t, access_token), 0},

    FUSE_OPT_KEY("--oauth", KEY_OAUTH_URL),
    
    FUSE_OPT_KEY("-V", KEY_VERSION),
    FUSE_OPT_KEY("--version", KEY_VERSION),
    FUSE_OPT_KEY("-h", KEY_HELP),
    FUSE_OPT_KEY("--help", KEY_HELP),
    
    FUSE_OPT_END
};

static int vkaudiofs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
    (void) data;
    (void) arg;
    
    switch (key) {
        case KEY_HELP:
            g_print("FUSE virtual file system for VK (VKontakte) audio records.\n"
                    "\n"
                    "Please note the filesystem does not handle remote playlist updates.\nYou have to remount FS manually on adding or removing files from your audio list on VK.\n"
                    "\n"
                    "VKAudioFS options:\n"
                    "    --oauth                get OAuth authentication URL\n"
                    "    --user_id INTEGER      REQUIRED, user_id returned in OAuth response\n"
                    "    --access_token STRING  REQUIRED, access_token returned in OAuth response\n"
                    "\n"
                    );
            
            fuse_opt_add_arg(outargs, "-h");
            fuse_main(outargs->argc, outargs->argv, &vkaudiofs_oper, NULL);
            
            exit(EXIT_SUCCESS);
            
        case KEY_VERSION:
            g_print("VkAudioFS version: %s\n", VK_VERSION);
            
            fuse_opt_add_arg(outargs, "-V");
            fuse_main(outargs->argc, outargs->argv, &vkaudiofs_oper, NULL);
            
            exit(EXIT_SUCCESS);
            
        case KEY_OAUTH_URL:
            g_print("Open the following URL in browser, authenticate and provide --access_token and --user_id in mount options. See --help for details.\n");
            g_print(VK_OAUTH_URL, VK_APP_ID);
            g_print("\n");
            
            exit(EXIT_SUCCESS);
    };
    
    return 1;
}

int main(gint argc, gchar * argv[])
{
    GKeyFile *cache_file = g_key_file_new();
    GList *files = NULL;
    GList *iterator = NULL;
    gint file_number = 0;
    gsize cached_file_size = 0;
    gsize cache_file_data_size = 0;
    gchar *progress = NULL;
    gchar *cache_file_path = g_build_path(G_DIR_SEPARATOR_S, g_get_user_cache_dir(), "vkaudiofs", NULL);
    gchar *size_formatted = NULL;
    gchar *audio_file_id_key = NULL;
    gchar *cache_file_data = NULL;
    vkaudiofs_audio_file *audio_file = NULL;
   
    setlocale(LC_CTYPE, "");
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    memset(&vkaudiofs_config, 0, sizeof(struct vkaudiofs_config_t));
    
    vkaudiofs_config.files_id_table = g_hash_table_new(g_int_hash, g_int_equal);
    vkaudiofs_config.files_name_table = g_hash_table_new(g_str_hash, g_str_equal);
    
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    fuse_opt_parse(&args, &vkaudiofs_config, vkaudiofs_opts, vkaudiofs_opt_proc);
    
    fuse_opt_add_arg(&args, "-r");
    
    if (vkaudiofs_config.user_id == 0)
    {
        g_critical("Please provide --user_id, see --help for details.");
        return EXIT_FAILURE;
    }
    
    if (vkaudiofs_config.access_token == 0)
    {
        g_critical("Please provide --access_token, see --help for details.");
        return EXIT_FAILURE;
    }
    
    if (g_file_test(cache_file_path, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)))
    {
        if (!g_key_file_load_from_file(cache_file, cache_file_path, G_KEY_FILE_NONE, NULL))
        {
            g_critical("Cache file could not be loaded, remove %s and try on more time.", cache_file_path);
            
            return EXIT_FAILURE;
        }
    }
    
    if (vkaudiofs_get_audio_files(&vkaudiofs_config) == VKAUDIOFS_API_RESULT_ERROR)
    {
        g_critical("There was a problem with API request, check your token and try one more time.");
        return EXIT_FAILURE;
    }
    
    if (vkaudiofs_config.files_number == 0)
    {
        g_critical("User don't have any audio records, you can't mount your vacuum.");
        return EXIT_FAILURE;
    }

    g_print("We found %d files in user account with id %d.\n"
            "It may take some time to query remote server for a file size with HEAD request.\n"
            "We cache query results, so next time this will be faster then light.\n\n",
            vkaudiofs_config.files_number, vkaudiofs_config.user_id);
    
    files = g_hash_table_get_values(vkaudiofs_config.files_id_table);

    for(iterator = files; iterator; iterator = iterator->next)
    {
        file_number++;
        
        audio_file = iterator->data;
        
        audio_file_id_key = g_strdup_printf("%d", audio_file->id);
        
        cached_file_size = g_key_file_get_uint64(cache_file, "file_size_cache", audio_file_id_key, NULL);
        
        if (cached_file_size > 0)
        {
            audio_file->size = cached_file_size;
        }
        else
        {
            audio_file->size = vkaudiofs_get_remote_file_size(audio_file->url);
        }
        
        size_formatted = g_format_size(audio_file->size);
        
        progress = g_strdup_printf("[%d of %d] %s - %s : %s", file_number, vkaudiofs_config.files_number, audio_file->artist, audio_file->title, size_formatted);
        g_print("%s\n", progress);
        
        g_key_file_set_uint64(cache_file, "file_size_cache", audio_file_id_key, audio_file->size);
        
        g_free(progress);
        g_free(size_formatted);
        g_free(audio_file_id_key);
    }
    
    cache_file_data = g_key_file_to_data(cache_file, &cache_file_data_size, NULL);
    
    if (!g_file_set_contents(cache_file_path, cache_file_data, cache_file_data_size, NULL))
    {
        g_critical("Cache file could not be saved, check your write permissions for path %s.", cache_file_path);
        
        return EXIT_FAILURE;
    }
    
    g_key_file_free(cache_file);
    
    g_free(cache_file_data);
    g_free(iterator);
    g_free(cache_file_path);

    g_print("\nCongratulations, all is done. Your freshly baked mount point is ready to use.\n");
    
    return fuse_main(args.argc, args.argv, &vkaudiofs_oper, NULL);
}

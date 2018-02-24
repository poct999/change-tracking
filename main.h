#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>

#include <openssl/md5.h>
#include <glib.h> //only for GList

#define MAX_BUF_SIZE 128
#define DEFAUL_DELAY 5

//type of change
#define C_ADDED "ADDED"
#define C_CHANGED "CHANGED"
#define C_DELETED "DELETED"

typedef struct monitoring_unit{
    char path[PATH_MAX]; //relative path

    char md5[MD5_DIGEST_LENGTH + 1]; //md5 for files

    mode_t mode; //stat -> st_mode
    unsigned char watched; //flag for remove check
}monitoring_unit;

int delay; // "t" argument
int work; // flag for threads

/*Handler CTRL+C*/
void ctrlc(int sig);

/*Thread function*/
void *thread_function(void *arg);

/*Get md5*/
int get_md5sum(char *file_path, char *buf);

/*Recursion parse*/
GList *walk_dir(GList *list, char *path, char *parent_dir);

/*GList function*/
monitoring_unit *find_unit_in_list(GList *list, char *path);
GList *check_deleted(GList *list, char* base_path);

/*foreach function*/
void watched_down(gpointer data, gpointer user_data);
void free_unit(gpointer data, gpointer user_data);

/*constructor*/
monitoring_unit *new_monitoring_unit(char* path, char* md5, mode_t mode);

/*Print changes of files*/
void c_print(char* base_path, char* change_type, monitoring_unit* unit);




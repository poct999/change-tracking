#include "main.h"

int main(int argc, char* argv[])
{
    struct sigaction act;
    act.sa_handler = ctrlc;
    sigaction(SIGINT, &act, 0);

    int dir_count = 0;

    //get delay argument
    int opt;
    while ((opt = getopt(argc, argv, ":t:")) != -1) {
        switch(opt) {
            case 't':
                delay = atoi(optarg);
                break;
            default:
                break;
        }
    }
    if (delay <= 0) delay = DEFAUL_DELAY;

    //get dir paths
    if ((dir_count = argc - optind) <= 0){
        printf("Write dirs to monitor!\n");
        exit(EXIT_FAILURE);
    }

    int i = 0;
    char *dir_paths = (char*) calloc(dir_count, MAX_BUF_SIZE * sizeof(char));
    while (optind < argc){
        if (strlen(argv[optind]) > MAX_BUF_SIZE - 1){
            printf("So long argument: %s\n", argv[optind]);
            exit(EXIT_FAILURE);
        }
        strcpy(dir_paths+(i++)*MAX_BUF_SIZE, argv[optind++]);
    }

    //Threads create
    pthread_t *threads = malloc(sizeof(pthread_t) * dir_count);
    if (threads == NULL){
        perror("Malloc error");
        exit(EXIT_FAILURE);
    }
    work = 1;
    for(i = 0; i < dir_count; i++){
        if (pthread_create(threads+i, NULL, thread_function, dir_paths+i*MAX_BUF_SIZE) != 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }
    }
    //Threads join
    for(i = 0; i < dir_count; i++){
        void *thread_result;
         if (pthread_join(*(threads+i), &thread_result) != 0) {
            perror("Thread join failed");
            exit(EXIT_FAILURE);
        }
    }

    printf("Exited...\n");

    free(threads);
    free(dir_paths);
    return 0;
}

monitoring_unit *find_unit_in_list(GList *list, char* path)
{
    GList *tmp = list;
    while (tmp != NULL)
    {
        monitoring_unit *el = (monitoring_unit *) tmp->data;
        if (!strcmp(el->path,path)){
            return el;
        }
        tmp = g_list_next(tmp);
    }
    return NULL;
}

void ctrlc(int sig) {
    printf("\nStopping...\n");
    work = 0;
}

int get_md5sum(char *path, char *res)
{
    int file;
    if ((file = open(path, O_RDONLY)) == -1){
        return -1;
    }

    MD5_CTX c;
    char buf[512] = {0};
    int bytes;

    MD5_Init(&c);
    while((bytes=read(file, buf, 512)) > 0)
    {
        MD5_Update(&c, buf, bytes);
    }
    MD5_Final(res, &c);

    close(file);
    return 0;
}

GList *check_deleted(GList *list, char* base_path)
{
    GList *tmp = list;
    while (tmp != NULL)
    {
        monitoring_unit *el = (monitoring_unit *) tmp->data;
        if (el->watched == 0){
            c_print(base_path, C_DELETED, el);
            tmp = list = g_list_delete_link(list, tmp);
            free(el);
        }
        tmp = g_list_next(tmp);
    }
    return list;
}

void c_print(char* base_path, char* change_type, monitoring_unit* unit)
{
    printf("[%s/]: %s ", base_path, change_type);
    if (S_ISDIR(unit->mode)){
        printf("directory");
    }else{
        printf("file");
    }
    printf(" '%s'\n", unit->path);
}

GList *walk_dir(GList *list, char *path, char *base_path)
{
    if (path == NULL || base_path == NULL) return list;

    char my_realpath[PATH_MAX];
    strcpy(my_realpath, base_path);
    strcat(my_realpath, "/");
    strcat(my_realpath, path);
    strcat(my_realpath, "/");

    DIR *dir;
    struct stat stbuf;
    struct dirent *dp;
    monitoring_unit *el;

    if ((dir = opendir(my_realpath)) == NULL){
        printf("Can`t open directory '%s'.\n", my_realpath);
        return list;
    }

    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            continue;

        char this_realpath[PATH_MAX];
        strcpy(this_realpath, my_realpath);
        strcat(this_realpath, dp->d_name);

        char this_relativepath[PATH_MAX];
        strcpy(this_relativepath, path);
        if (strlen(path)) strcat(this_relativepath, "/");
        strcat(this_relativepath, dp->d_name);

        if (stat(this_realpath, &stbuf) == -1) {
            printf("No access '%s'\n", this_relativepath);
            continue;
        }

        if (S_ISDIR(stbuf.st_mode)) {

            if (!(el = find_unit_in_list(list, this_relativepath))){
                el = new_monitoring_unit(this_relativepath, NULL, stbuf.st_mode);
                list = g_list_append(list, el);
                c_print(base_path, C_ADDED, el);
            }
                list = walk_dir(list, this_relativepath, base_path);
        }else{
            char md5[MD5_DIGEST_LENGTH + 1] = {0};
            if (get_md5sum(this_realpath, md5) < 0){
                printf("Can`t read file: %s\n", this_realpath);
                continue;
            }

            if (!(el = find_unit_in_list(list, this_relativepath))){
                el = new_monitoring_unit(this_relativepath, md5, stbuf.st_mode);
                list = g_list_append(list, el);
                c_print(base_path, C_ADDED, el);
            }else{
                if (strcmp(el->md5, md5)){
                    strcpy(el->md5, md5);
                    c_print(base_path, C_CHANGED, el);
                }
            }
        }
        el->watched = 1;
    }

    closedir(dir);
    return list;
}

void watched_down(gpointer data, gpointer user_data){
    monitoring_unit *el = (monitoring_unit*)data;
    el->watched = 0;
}

void free_unit(gpointer data, gpointer user_data){
    free(data);
}

void *thread_function(void *arg)
{
    int i;
    char *my_path = (char*) arg;
    if (my_path[strlen(my_path)-1] == '/') my_path[strlen(my_path)-1] = '\0';

    struct stat stbuf;
    if ((stat(my_path, &stbuf) == -1) || !(S_ISDIR(stbuf.st_mode))) {
        printf("Can`t open '%s'. Exited.\n", my_path);
        pthread_exit(NULL);
    }else{
        printf("Thread started for directory '%s'\n", my_path);
    }

    GList *list = NULL;

    while (work){
        //printf("********Итерация********\n");

        g_list_foreach(list, (GFunc)watched_down, NULL);
        list = walk_dir(list, "", my_path);
        list = check_deleted(list, my_path);

        for (i = 0; i<delay; i++){
            if (!work) break;
            sleep(1);
        }
    }

    g_list_foreach(list, (GFunc)free_unit, NULL);
    g_list_free(list);

    pthread_exit(NULL);
}

monitoring_unit *new_monitoring_unit(char* path, char* md5, mode_t mode)
{
    monitoring_unit *tmp = (monitoring_unit*) malloc(sizeof(monitoring_unit));

    tmp->mode = mode;
    tmp->watched = 1;

    if (path != NULL) strcpy(tmp->path, path);
        else tmp->path[0] = '\0';

    if (md5 != NULL) strcpy(tmp->md5, md5);
        else tmp->md5[0] = '\0';

    return tmp;
}













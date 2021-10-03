
/**************************
    上传文件后台CGI程序
***************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include "deal_mysql.h"
#include "fdfs_api.h"
#include "fcgi_stdio.h"
#include "fdfs_client.h"
#include "make_log.h" // 日志头文件
#include "cfg.h"
#include "util_cgi.h" // cgi后台通用接口，trim_space(), memstr()

#define UPLOAD_LOG_MODULE "cgi"
#define UPLOAD_LOG_PROC   "upload"

// mysql 数据库配置信息 用户名， 密码， 数据库名称
static char mysql_user[128] = {0};
static char mysql_pwd[128] = {0};
static char mysql_db[128] = {0};

// redis 服务器ip、端口
// static char redis_ip[30] = {0};
// static char redis_port[10] = {0};

// 读取配置信息
void read_cfg()
{
    // 读取mysql数据库配置信息
    get_cfg_value(CFG_PATH, "mysql", "user", mysql_user);
    get_cfg_value(CFG_PATH, "mysql", "password", mysql_pwd);
    get_cfg_value(CFG_PATH, "mysql", "database", mysql_db);
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "mysql:[user=%s,pwd=%s,database=%s]", mysql_user, mysql_pwd, mysql_db);

    // 读取redis配置信息
    // get_cfg_value(CFG_PATH, "redis", "ip", redis_ip);
    // get_cfg_value(CFG_PATH, "redis", "port", redis_port);
    // LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "redis:[ip=%s,port=%s]\n", redis_ip, redis_port);
}


// 解析上传的post数据 保存到本地临时路径同时得到文件上传者、文件名称、文件大小
int recv_save_file(long len, char *user, char *filename, char *md5, long *p_size)
{
    int ret = 0;
    char *file_buf = NULL;
    char *begin = NULL;
    char *p, *q, *k;

    char content_text[TEMP_BUF_MAX_LEN] = {0}; // 文件头部信息
    char boundary[TEMP_BUF_MAX_LEN] = {0};     // 分界线信息

    // 开辟存放文件的内存
    file_buf = (char *)malloc(len);     // 这里分配的是body大小，不是文件大小
    if (file_buf == NULL)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "malloc error! file size is to big!!!!\n");
        return -1;
    }
    
    // 把所有数据读取出来
    int ret2 = fread(file_buf, 1, len, stdin); // 从标准输入(web服务器)读取内容
    if(ret2 == 0)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "fread(file_buf, 1, len, stdin) err\n");
        ret = -1;
        goto END;
    }

    // 开始处理前端发送过来的post数据格式
    begin = file_buf;    // 内存起点
    p = begin;

    /*
       ------WebKitFormBoundary88asdgewtgewx\r\n（起始边界）
       Content-Disposition: form-data; user="ltl"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
       Content-Type: application/octet-stream\r\n
       \r\n
       真正的文件内容\r\n
       ------WebKitFormBoundary88asdgewtgewx（结束边界）
    */

    // get boundary 得到分界线, ------WebKitFormBoundary88asdgewtgewx
    // 1. 跳过分界线
    p = strstr(begin, "\r\n");  // 作用是返回字符串中首次出现子串的地址
    if (p == NULL)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"wrong no boundary!\n");
        ret = -1;
        goto END;
    }

    // 拷贝分界线
    strncpy(boundary, begin, p-begin);      // 缓存分界线
    boundary[p-begin] = '\0';   // 字符串结束符
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"boundary: [%s]\n", boundary);

    p += 2; // 跳过\r\n
    // 已经处理了p-begin的长度
    len -= (p-begin);

    // get content text head
    begin = p;

    // Content-Disposition: form-data; user="……"; filename="……"; md5="……"; size=……\r\n
    // 2. 跳过Content-Disposition
    p = strstr(begin, "\r\n");
    if(p == NULL)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"ERROR: get context text error, no filename?\n");
        ret = -1;
        goto END;
    }
    strncpy(content_text, begin, p-begin);
    content_text[p-begin] = '\0';   // 保存
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"content_text: [%s]\n", content_text);

    p += 2;// \r\n
    len -= (p-begin);

    // 获取文件上传者
    // Content-Disposition: form-data; user="……"; filename="……"; md5="……"; size=……\r\n
    //                                ↑
    q = begin;
    q = strstr(begin, "user=");

    // Content-Disposition: form-data; user="……"; filename="……"; md5="……"; size=……\r\n
    //                                      ↑
    q += strlen("user=");
    q++;    // 跳过第一个"

    // Content-Disposition: form-data; user="……"; filename="……"; md5="……"; size=……\r\n
    //                                         ↑
    k = strchr(q, '"');
    strncpy(user, q, k-q);  // 拷贝用户名
    user[k-q] = '\0';

    // 去掉一个字符串两边的空白字符
    trim_space(user);   // util_cgi.h

    // 获取文件名字
    // "; filename="……"; md5="……"; size=……\r\n
    //   ↑
    begin = k;
    q = begin;
    q = strstr(begin, "filename=");

    // "; filename="……"; md5="……"; size=……\r\n
    //             ↑
    q += strlen("filename=");
    q++;    // 跳过第一个"

    // "; filename="……"; md5="……"; size=……\r\n
    //                ↑
    k = strchr(q, '"');
    strncpy(filename, q, k-q);  // 拷贝文件名
    filename[k-q] = '\0';

    trim_space(filename);   // util_cgi.h

    // 获取文件MD5码
    // "; md5="……"; size=……\r\n
    //   ↑
    begin = k;
    q = begin;
    q = strstr(begin, "md5=");

    // "; md5="……"; size=……\r\n
    //        ↑
    q += strlen("md5=");
    q++;    // 跳过第一个"

    // "; md5="……"; size=……\r\n
    //           ↑
    k = strchr(q, '"');
    strncpy(md5, q, k-q);   // 拷贝文件名
    md5[k-q] = '\0';

    trim_space(md5);    // util_cgi.h

    // ========================================获取文件大小
    // "; size=……\r\n
    //   ↑
    begin = k;
    q = begin;
    q = strstr(begin, "size=");

    // "; size=……\r\n
    //        ↑
    q += strlen("size=");

    // "; size=……\r\n
    //           ↑
    k = strstr(q, "\r\n");
    char tmp[256] = {0};
    strncpy(tmp, q, k-q);   // 内容
    tmp[k-q] = '\0';

    *p_size = strtol(tmp, NULL, 10); // 字符串转long

    begin = p;
    p = strstr(begin, "\r\n");
    p += 4;// \r\n\r\n
    len -= (p-begin);

    // 下面才是文件的真正内容

    // 指向数据起始位置
    begin = p;      
    // find file's end
    p = memstr(begin, len, boundary);// util_cgi.h，在字符串中中查找字符串substr第一次出现的位置
    if (p == NULL)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "memstr(begin, len, boundary) error\n");
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "begin:%s\n", begin);
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "boundary:%s\n", boundary);
        ret = -1;
        goto END;
    }
    else
    {
        p = p - 2;// \r\n
    }

    int fd = 0;
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"start open %s\n", filename);
    fd = open(filename, O_CREAT|O_WRONLY, 0644);
    if (fd < 0)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"open %s error\n", filename);
        ret = -1;
        goto END;
    }

    // ftruncate会将参数fd指定的文件大小改为参数length指定的大小
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"write %s len %d\n", filename, (p-begin));
    
    ftruncate(fd, (p-begin));
    write(fd, begin, (p-begin));
    close(fd);

END:
    free(file_buf);
    return ret;
}

// 上传到fastdfs中（源码）
int upload_to_dstorage_1(char *filename, char *confpath, char *fileid)
{
	char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	ConnectionInfo *pTrackerServer;
	int result;
	int store_path_index;
	ConnectionInfo storageServer;
	
	log_init();
	g_log_context.log_level = LOG_ERR;
	ignore_signal_pipe();

    // 加载配置文件, 并初始化
    const char* conf_file = confpath;
	if ((result=fdfs_client_init(conf_file)) != 0)
	{
		return result;
	}

    // 通过配置文件信息连接tracker, 并得到一个可以访问tracker的句柄
	pTrackerServer = tracker_get_connection();
	if (pTrackerServer == NULL)
	{
		fdfs_client_destroy();
		return errno != 0 ? errno : ECONNREFUSED;
	}

	*group_name = '\0';

    // 通过tracker句柄得到一个可以访问的storage句柄
	if ((result=tracker_query_storage_store(pTrackerServer, \
	                &storageServer, group_name, &store_path_index)) != 0)
	{
		fdfs_client_destroy();
		LOG("fastDFS", "upload_file", "tracker_query_storage fail, error no: %d, error info: %s",
			result, STRERROR(result));
		return result;
	}

    // 通过得到的storage句柄, 上传本地文件
	result = storage_upload_by_filename1(pTrackerServer, \
			&storageServer, store_path_index, \
			filename, NULL, \
			NULL, 0, group_name, fileid);
	if (result == 0)
	{
        LOG("fastDFS", "upload_file", "fileID = %s", fileid);
	}
	else
	{
        LOG("fastDFS", "upload_file", "upload file fail, error no: %d, error info: %s",
			result, STRERROR(result));
	}

    // 断开连接, 释放资源
	tracker_close_connection_ex(pTrackerServer, true);
	fdfs_client_destroy();

	return result;
}


// 将一个本地文件上传到后台分布式文件系统中
int upload_to_dstorage(char *filename, char *fileid)
{
    int ret = 0;

    pid_t pid;
    int fd[2];

    // 管道的创建
    if (pipe(fd) < 0)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"pip error\n");
        ret = -1;
        goto END;
    }

    // 创建子进程，为了运行fdfs_upload_file
    pid = fork();
    if (pid < 0)// 进程创建失败
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"fork error\n");
        ret = -1;
        goto END;
    }

    if(pid == 0) // 子进程
    {
        // 关闭读端
        close(fd[0]);

        // 将标准输出重定向到写管道
        // 因为fdfs_uplode_file成功会在标准输出中返回一个文件保存的路径
        // 往标准输出写的东西都会重定向到fd所指向的文件, 当fileid产生时输出到管道fd[1]，然后父进程从fd[0]中读取
        dup2(fd[1], STDOUT_FILENO); 

        // 读取fdfs client 配置文件的路径
        char fdfs_cli_conf_path[256] = {0};
        // 读配置文件
        get_cfg_value(CFG_PATH, "dfs_path", "client", fdfs_cli_conf_path);

        // execlp会从PATH环境变量所指的目录中查找符合参数file的文件名，找到后便执行该文件，
        // 然后将第二个以后的参数当做该文件的argv[0]、argv[1]……，最后一个参数必须用空指针(NULL)作结束。
        // 所以通过execlp执行fdfs_upload_file后，如果函数调用成功，进程自己的执行代码就会变成加载程序的代码，execlp后边的代码也就不会执行了
        execlp("fdfs_upload_file", "fdfs_upload_file", fdfs_cli_conf_path, filename, NULL);
        // 或者使用下面这种方式条用
        // int temp = upload_to_dstorage_1(filename, fdfs_cli_conf_path, fileid);
        // LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "ret:%d!\n", temp);

        // 执行失败
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "execlp fdfs_upload_file error\n");

        close(fd[1]);
    }
    else // 父进程
    {
        // 关闭写端
        close(fd[1]);

        // 从管道中去读数据，读取文件路径
        read(fd[0], fileid, TEMP_BUF_MAX_LEN);  // 等待管道写入然后读取

        // 去掉一个字符串两边的空白字符
        trim_space(fileid);

        if (strlen(fileid) == 0)
        {
            LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"[upload FAILED!]\n");
            ret = -1;
            goto END;
        }

        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "get [%s]!\n", fileid);

        wait(NULL); // 等待子进程结束，回收其资源
        close(fd[0]);
    }

END:
    return ret;
}


// 封装文件存储在分布式系统中的 完整 url
int make_file_url(char *fileid, char *fdfs_file_url)
{
    int ret = 0;

    char *p = NULL;
    char *q = NULL;
    char *k = NULL;

    char fdfs_file_stat_buf[TEMP_BUF_MAX_LEN] = {0};
    char fdfs_file_host_name[HOST_NAME_LEN] = {0};  // storage所在服务器ip地址

    pid_t pid;
    int fd[2];

    // 无名管道的创建
    if (pipe(fd) < 0)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "pip error\n");
        ret = -1;
        goto END;
    }

    // 创建进程
    pid = fork();
    if (pid < 0)// 进程创建失败
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"fork error\n");
        ret = -1;
        goto END;
    }

    if(pid == 0) // 子进程
    {
        // 关闭读端
        close(fd[0]);

        // 将标准输出 重定向 写管道
        dup2(fd[1], STDOUT_FILENO); // dup2(fd[1], 1);

        // 读取fdfs client 配置文件的路径
        char fdfs_cli_conf_path[256] = {0};
        get_cfg_value(CFG_PATH, "dfs_path", "client", fdfs_cli_conf_path);

        // 和fdfs_uplode_file那些一样，之后解析
        execlp("fdfs_file_info", "fdfs_file_info", fdfs_cli_conf_path, fileid, NULL);

        // 执行失败
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "execlp fdfs_file_info error\n");

        close(fd[1]);
    }
    else // 父进程
    {
        // 关闭写端
        close(fd[1]);

        // 从管道中去读数据
        read(fd[0], fdfs_file_stat_buf, TEMP_BUF_MAX_LEN);
        // LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "get file_ip [%s] succ\n", fdfs_file_stat_buf);

        wait(NULL); // 等待子进程结束，回收其资源
        close(fd[0]);

        // 拼接上传文件的完整url地址--->http:/host_name/group1/M00/00/00/rBT4p2D_4C2ActHRAACPRAVTRKk238.png
        p = strstr(fdfs_file_stat_buf, "source ip address: ");

        q = p + strlen("source ip address: ");
        k = strstr(q, "\n");

        strncpy(fdfs_file_host_name, q, k-q);
        fdfs_file_host_name[k-q] = '\0';

        // printf("host_name:[%s]\n", fdfs_file_host_name);

        // 读取storage_web_server服务器的端口
        char storage_web_server_port[20] = {0};
        get_cfg_value(CFG_PATH, "storage_web_server", "port", storage_web_server_port);
        strcat(fdfs_file_url, "http:// ");
        // 因为这里得到的fdfs_file_host_name为内网ip，所以这里使用cfg.json中的ip
        // strcat(fdfs_file_url, fdfs_file_host_name);
        char storage_web_server_ip[20] = {0};
        get_cfg_value(CFG_PATH, "storage_web_server", "ip", storage_web_server_ip);
        strcat(fdfs_file_url, storage_web_server_ip);
        strcat(fdfs_file_url, ":");
        strcat(fdfs_file_url, storage_web_server_port);
        strcat(fdfs_file_url, "/");
        strcat(fdfs_file_url, fileid);

        // printf("[%s]\n", fdfs_file_url);
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "file url is: %s\n", fdfs_file_url);
    }

END:
    return ret;
}


int store_fileinfo_to_mysql(char *user, char *filename, char *md5, long size, char *fileid, char *fdfs_file_url)
{
    int ret = 0;
    MYSQL *conn = NULL; // 数据库连接句柄

    time_t now;;
    char create_time[TIME_STRING_LEN];
    char suffix[SUFFIX_LEN];
    char sql_cmd[SQL_MAX_LEN] = {0};

    // 连接mysql数据库
    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "msql_conn connect err\n");
        ret = -1;
        goto END;
    }

    // 设置数据库编码
    mysql_query(conn, "set names utf8");

    // 得到文件后缀字符串 如果非法文件后缀,返回"null"
    get_file_suffix(filename, suffix); // mp4, jpg, png

    // sql语句
    sprintf(sql_cmd, "insert into file_info (md5, file_id, url, size, type, count) values ('%s', '%s', '%s', '%ld', '%s', %d)",
            md5, fileid, fdfs_file_url, size, suffix, 1);

    if (mysql_query(conn, sql_cmd) != 0) // 执行sql语句
    {
        // print_error(conn, "插入失败");
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s 插入失败: %s\n", sql_cmd, mysql_error(conn));
        ret = -1;
        goto END;
    }

    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s 文件信息插入成功\n", sql_cmd);

    // 获取当前时间
    now = time(NULL);
    strftime(create_time, TIME_STRING_LEN-1, "%Y-%m-%d %H:%M:%S", localtime(&now));

    // sql语句
    sprintf(sql_cmd, "insert into user_file_list(user, md5, create_time, file_name, shared_status, pv) values ('%s', '%s', '%s', '%s', %d, %d)", user, md5, create_time, filename, 0, 0);
    if(mysql_query(conn, sql_cmd) != 0)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s 操作失败: %s\n", sql_cmd, mysql_error(conn));
        ret = -1;
        goto END;
    }

    // 查询用户文件数量
    sprintf(sql_cmd, "select count from user_file_count where user = '%s'", user);
    int ret2 = 0;
    char tmp[512] = {0};
    int count = 0;
    // 返回值： 0成功并保存记录集，1没有记录集，2有记录集但是没有保存，-1失败
    ret2 = process_result_one(conn, sql_cmd, tmp); // 执行sql语句
    if(ret2 == 1) // 没有记录
    {
        // 插入记录
        sprintf(sql_cmd, " insert into user_file_count (user, count) values('%s', %d)", user, 1);
    }
    else if(ret2 == 0)
    {
        // 更新用户文件数量count字段
        count = atoi(tmp);
        sprintf(sql_cmd, "update user_file_count set count = %d where user = '%s'", count+1, user);
    }

    if(mysql_query(conn, sql_cmd) != 0)
    {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s 操作失败: %s\n", sql_cmd, mysql_error(conn));
        ret = -1;
        goto END;
    }

END:
    if (conn != NULL)
    {
        mysql_close(conn); // 断开数据库连接
    }

    return ret;
}

// 将该文件的FastDFS相关信息存入mysql中
int main()
{
    char filename[FILE_NAME_LEN] = {0};     // 文件名
    char user[USER_NAME_LEN] = {0};         // 文件上传者
    char md5[MD5_LEN] = {0};                // 文件md5码
    long size;  // 文件大小
    char fileid[TEMP_BUF_MAX_LEN] = {0};    // 文件上传到fastDFS后的文件id
    char fdfs_file_url[FILE_URL_LEN] = {0}; // 文件所存放storage的host_name

    // 读取数据库配置信息
    read_cfg();

    while (FCGI_Accept() >= 0)
    {
        char *contentLength = getenv("CONTENT_LENGTH");
        long len;
        int ret = 0;

        printf("Content-type: text/html\r\n\r\n");

        if (contentLength != NULL)
        {
            LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "CONTENT_LENGTH:%s\n", contentLength);
            len = strtol(contentLength, NULL, 10); // 字符串转long， 或者atol
        }
        else
        {
            len = 0;
        }

        if (len <= 0)
        {
            printf("No data from standard input\n");
            LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "len = 0, No data from standard input\n");
            ret = -1;
        }
        else
        {
            LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s开始上传[%s, 大小：%ld]到本地\n", user, filename, len);
            // 得到上传文件
            // 1、保存到本地（使用fastdfs提供的接口需要做这种转换）（fdfs_uplode_file）
            if (recv_save_file(len, user, filename, md5, &size) < 0)
            {
                ret = -1;
                goto END;
            }

            LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s成功上传[%s, 大小：%ld, md5码：%s]到本地\n", user, filename, size, md5);

            // 将该文件存入fastDFS中,并得到文件的file_id
            // 2、上传到fastdfs
            if (upload_to_dstorage(filename, fileid) < 0)
            {
                ret = -1;
                goto END;
            }

            // 删除本地临时存放的上传文件
            unlink(filename);

            // 3、拼接出地址 http地址
            // 得到文件所存放storage的host_name
            // 拼接出完整的http地址
            // file_id:           group1/M00/00/00/rBT4p2D_4C2ActHRAACPRAVTRKk238.png
            if (make_file_url(fileid, fdfs_file_url) < 0) 
            {
                ret = -1;
                goto END;
            }

            // 4、写数据库，user_file_list, file_info
            // 将该文件的FastDFS相关信息存入mysql中
            // 把文件写入file_info
            if (store_fileinfo_to_mysql(user, filename, md5, size, fileid, fdfs_file_url) < 0)
            {
                ret = -1;
                goto END;
            }


END:
            memset(filename, 0, FILE_NAME_LEN);
            memset(user, 0, USER_NAME_LEN);
            memset(md5, 0, MD5_LEN);
            memset(fileid, 0, TEMP_BUF_MAX_LEN);
            memset(fdfs_file_url, 0, FILE_URL_LEN);

            char *out = NULL;
            // 给前端返回，上传情况
            /*
               上传文件：
               成功：{"code":"008"}
               失败：{"code":"009"}
               */
            if(ret == 0) // 成功上传
            {
                out = return_status("008");// common.h
            }
            else// 上传失败
            {
                out = return_status("009");
            }

            if(out != NULL)
            {
                printf(out); // 给前端反馈信息
                free(out);   // 记得释放
            }

        }

    } // while

    return 0;
}

cmake_minimum_required(VERSION 3.0)
project(CLOUD_DISK)

aux_source_directory(../common common)

# 相当于-I，指定头文件的搜索路径
include_directories("../include" "/usr/include/fastdfs" "/usr/include/fastcommon" "/usr/local/include/hiredis/" "/usr/include/mysql/")

# 相当于-L，动态链接库或静态链接库的搜索路径
link_directories("/usr/lib/mysql" "fdfsclient" "fastcommon" "hiredis" "fcgi" "m" "mysqlclient")
link_directories("/usr/lib/mysql")

message("common -> " ${common})

# 找到所有含有mian函数的.c文件，保存在mians里
file(
    GLOB_RECURSE mains RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

# 循环遍历每个含main的文件，分别编译生成exe文件
foreach(mainfile IN LISTS mains)
    # Get file name without directory
    get_filename_component(mainname ${mainfile} NAME_WE)
    set(mainfile ${mainfile} ${common})
    add_executable(${mainname} ${mainfile})
    # message("mainname -> " ${mainname})
    # message("mainfile -> " ${mainfile})
    # 添加库文件目录
    target_link_libraries(${mainname} 
        "fdfsclient" "fastcommon" "hiredis" "fcgi" "m" "mysqlclient" 
        "/usr/lib/x86_64-linux-gnu/libmysqlclient.so" "/usr/local/lib/libhiredis.so" "pthread")
endforeach()
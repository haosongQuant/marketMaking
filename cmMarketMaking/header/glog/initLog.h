#pragma once

#include <string>
#include "glog\logging.h"

inline bool initLog(char * pathprefix, const int severity, int maxSize)
{
	char fullpath[100];
	google::InitGoogleLogging("");
	google::SetStderrLogging(severity); // google::GLOG_WARNING); //设置级别高于 google::INFO 的日志同时输出到屏幕
	FLAGS_colorlogtostderr = true; //设置输出到屏幕的日志显示相应颜色
	memset(fullpath, 0, sizeof(fullpath));
	strncpy(fullpath, pathprefix, sizeof(fullpath));
	strcat(fullpath, "log_fatal_");
	google::SetLogDestination(google::GLOG_FATAL, fullpath); // 设置 google::FATAL 级别的日志存储路径和文件名前缀
	memset(fullpath, 0, sizeof(fullpath));
	strncpy(fullpath, pathprefix, sizeof(fullpath));
	strcat(fullpath, "log_error_");
	google::SetLogDestination(google::GLOG_ERROR, fullpath); //设置 google::ERROR 级别的日志存储路径和文件名前缀
	memset(fullpath, 0, sizeof(fullpath));
	strncpy(fullpath, pathprefix, sizeof(fullpath));
	strcat(fullpath, "log_warn_");
	google::SetLogDestination(google::GLOG_WARNING, fullpath); //设置 google::WARNING 级别的日志存储路径和文件名前缀
	memset(fullpath, 0, sizeof(fullpath));
	strncpy(fullpath, pathprefix, sizeof(fullpath));
	strcat(fullpath, "log_info_");
	google::SetLogDestination(google::GLOG_INFO, fullpath); //设置 google::INFO 级别的日志存储路径和文件名前缀
	FLAGS_logbufsecs = 0; //缓冲日志输出，默认为30秒，此处改为立即输出
	FLAGS_max_log_size = 100; //最大日志大小为 100MB
	FLAGS_stop_logging_if_full_disk = true; //当磁盘被写满时，停止日志输出

	return true;
}

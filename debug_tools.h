#ifndef _DEBUG_TOOLS_H_
#define _DEBUG_TOOLS_H_

#define __DEBUG_TOOLS_ON 0
#define __ENABLE_BREAKS  0

#define _dprint(str)																	\
		if (debug || __DEBUG_TOOLS_ON)																\
			fprintf(stderr, "DEBUG   - %s:%s():%d: " str, __FILE__, __func__, __LINE__);

#define _dprintf(fmt, ...)																\
		if (debug || __DEBUG_TOOLS_ON)																\
			fprintf(stderr, "DEBUG   - %s:%s():%d: " fmt, __FILE__, __func__, __LINE__, __VA_ARGS__);

#define _dbreak(str)												\
		if (debug || __DEBUG_TOOLS_ON){ 										\
			fprintf(stderr, "BREAK   - %s:%s():%d: " str, __FILE__, 	\
							__func__, __LINE__);			 		\
			if(__ENABLE_BREAKS)										\
				raise(SIGINT);										\
		}

#define _dbreakf(fmt, ...)											\
		if (debug || __DEBUG_TOOLS_ON){ 										\
			fprintf(stderr, "BREAK   - %s:%s():%d: " fmt, __FILE__, 	\
							__func__, __LINE__, __VA_ARGS__); 		\
			if(__ENABLE_BREAKS)										\
				raise(SIGINT);										\
		}

#define _print_err(str)																\
		fprintf(stderr, "ERROR   - %s:%s():%d: " str, __FILE__, __func__, __LINE__);

#define _printf_err(fmt, ...)																\
		fprintf(stderr, "ERROR   - %s:%s():%d: " fmt, __FILE__, __func__, __LINE__, __VA_ARGS__);

#define _print_info(str)																\
		fprintf(stderr, "INFO    - %s:%s():%d: " str, __FILE__,	__func__, __LINE__);

#define _printf_info(fmt, ...)																\
		fprintf(stderr, "INFO    - %s:%s():%d: " fmt, __FILE__,	__func__, __LINE__, __VA_ARGS__);

#define _print_warn(str)\
		fprintf(stderr, "WARNING - %s:%s():%d: " str, __FILE__, __func__, __LINE__);

#define _printf_warn(fmt, ...)																\
		fprintf(stderr, "WARNING - %s:%s():%d: " fmt, __FILE__, __func__, __LINE__, __VA_ARGS__);


#endif

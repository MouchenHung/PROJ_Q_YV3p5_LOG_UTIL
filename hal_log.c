/*
	NAME: LOG_UTIL
  	FILE: hal_log.c
  	DESCRIPTION: Provide function to access log settings
  	AUTHOR: MouchenHung
  	DATE/VERSION: 2021.12.13 - v1.0
  	Note: NULL
*/

#include <zephyr.h>
#include <sys/printk.h>
#include <logging/log.h>
#include <logging/log_ctrl.h>
#include "hal_log.h"
#include <init.h>

/* LOG SET */
#include <logging/log.h>
#define LOG_LEVEL LOG_LEVEL_INFO
LOG_MODULE_REGISTER(mc_log);

struct k_thread log_filter_thread;
K_KERNEL_STACK_MEMBER(log_filter_thread_stack, 1000);

static const char * const severity_lvls[] = {
	"none",
	"err",
	"wrn",
	"inf",
	"dbg",
};

/*
  - Name: log_source_id_get
  - Description: Try to get module's id by name
  - Input:
      * name: Module's name
  - Return: 
      * "id", if no error
	  * -1, otherwise
*/
static int16_t log_source_id_get(const char *name)
{
	for (int16_t i = 0; i < log_src_cnt_get(CONFIG_LOG_DOMAIN_ID); i++) {
		if (!strcmp(log_source_name_get(CONFIG_LOG_DOMAIN_ID, i), name)) {
			return i;
		}
	}

	return -1;
}

/*
  - Name: log_filter_wait
  - Description: Try to filter module's log level when each backend ready 
  - Input:
      NULL
  - Return: 
      NULL
*/
static void log_filter_wait(void *arg0, void *arg1, void *arg2){
	int backend_count = log_backend_count_get();
	int source_count = log_sources_count();

	for (int bknd=0; bknd<backend_count; bknd++){
		const struct log_backend *backend = log_backend_get(bknd);
		int i=0;

		LOG_INF("Wait for Log[%d] active...", bknd);
		while (!log_backend_is_active(backend)) {
			if ( !(i%2) )
				LOG_WRN("Log[%d] are halted at check time %d sec.", bknd, i);
			
			k_msleep(1000);
			i++;
		}
		LOG_INF("Log[%d] are active now, start filter!", bknd);
		
		int ret;
		for (int src=0; src<source_count; src++){
			ret = log_filter_set(backend, CONFIG_LOG_DOMAIN_ID, src, LOG_LEVEL_INF);
			if (ret!=LOG_LEVEL_INF)
				LOG_WRN("Log[%d] module[%d] has set to level[%d], not level info!", bknd, src, ret);
			
			//LOG_INF(" + Log[%d] module[%d] has set to level info!", bknd, src);
		}
		LOG_WRN("  + Log[%d] filter complete!\n", bknd);
	}
}

/*
  - Name: util_log_init_filter
  - Description: Thread that do "log_filter_wait()"
  - Input:
      NULL
  - Return: 
      NULL
  - Note: Could be called any time to filter module's log to level-info
*/
void util_log_init_filter(void){
	k_thread_create(&log_filter_thread, log_filter_thread_stack,
                  K_THREAD_STACK_SIZEOF(log_filter_thread_stack),
                  log_filter_wait,
                  NULL, NULL, NULL,
                  K_PRIO_COOP(7), 0, K_NO_WAIT);
}

/*
  - Name: log_status_report
  - Description: To report specific log backend's status and its module list status
  - Input:
      * backend_inst: Log backend instant
  - Return: 
      NULL
*/
static void log_status_report(uint16_t backend_inst)
{
	uint32_t modules_cnt = log_sources_count();
	uint32_t dynamic_lvl;
	uint32_t compiled_lvl;

    const struct log_backend *backend = log_backend_get(backend_inst);

    printk("%s\r\n"
            "\t- Status: %s\r\n"
            "\t- ID: %d\r\n\r\n",
            backend->name,
            backend->cb->active ? "enabled" : "disabled",
            backend->cb->id);

	if (!log_backend_is_active(backend)) {
		printk("Logs are halted!\n");
	}

	printk("%-40s | current | built-in \r\n","module_name");
	printk("----------------------------------------------------------\r\n");

	for (int16_t i = 0U; i < modules_cnt; i++) {
		dynamic_lvl = log_filter_get(backend, CONFIG_LOG_DOMAIN_ID, i, true);
		compiled_lvl = log_filter_get(backend, CONFIG_LOG_DOMAIN_ID, i, false);

		printk("%-40s | %-7s | %s\r\n",
			      log_source_name_get(CONFIG_LOG_DOMAIN_ID, i),
			      severity_lvls[dynamic_lvl],
			      severity_lvls[compiled_lvl]);
	}
    printk("----------------------------------------------------------\r\n\n");
}

/*
  - Name: log_status_report_all
  - Description: Print all backend's modules status
  - Input:
      NULL
  - Return: 
      NULL
  - Note: Only reports backend with auto-start
*/
void log_status_report_all(void)
{
    int backend_count = log_backend_count_get();

    printk("\n======================= LOG BACKEND STATUS =======================\n");
	for (int i=0; i < backend_count; i++) {
        log_status_report(i);
    }
    printk("==================================================================\n\n");
}

//SYS_INIT(util_log_init, APPLICATION, 99);
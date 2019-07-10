/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*---------------------------------------------------------------------------*/
/*#define LOG_CONF_LEVEL_RPL LOG_LEVEL_DBG*/ 
#define LOG_LEVEL_OAM LOG_LEVEL_DBG
#define LOG_LEVEL_CONETSI LOG_LEVEL_INFO
#define LOG_LEVEL_DUMMY LOG_LEVEL_INFO
#define LOG_LEVEL_QSIM_MOD LOG_LEVEL_DBG
#define LOG_LEVEL_NORMAL LOG_LEVEL_INFO
#define LOG_LEVEL_CSMA_MGMT_QLEN LOG_LEVEL_DBG
#define LOG_LEVEL_CSMA_MGMT_RTX LOG_LEVEL_DBG

#define LOG_CONF_LEVEL_6LOWPAN LOG_LEVEL_INFO
/*---------------------------------------------------------------------------*/
#define CONF_DUMMY     0
#define CONF_QSIM_MODULE 1    
#define CONF_CSMA_MGMT 0
/*---------------------------------------------------------------------------*/
#define CONF_CONETSI   0
#define CONF_NORMAL    1
/*---------------------------------------------------------------------------*/
#define CONF_PRIORITY PRIORITY_LINEAR
#define UIP_CONF_UIP_DS6_NOTIFICATIONS 1
/* #define LINKADDR_CONF_SIZE 8 */
/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/

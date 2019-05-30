/*---------------------------------------------------------------------------*/
#ifndef _CSMA_MGMT_QLEN_H_
#define _CSMA_MGMT_QLEN_H_
/*---------------------------------------------------------------------------*/
#ifdef CONF_CSMA_MGMT
#define CSMA_MGMT CONF_CSMA_MGMT
#else
#define CSMA_MGMT 0
#endif /* CONF_CSMA_MGMT */
/*---------------------------------------------------------------------------*/
#if CSMA_MGMT
void csma_mgmt_qlen_init(void);
void csma_mgmt_qlen_record(int len);

void csma_mgmt_rtx_init(void);
void csma_mgmt_rtx_record(int rtx);
#endif /* CSMA_MGMT */
/*---------------------------------------------------------------------------*/
#endif /* _CSMA_MGMT_QLEN_H_ */
/*---------------------------------------------------------------------------*/

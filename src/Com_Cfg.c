#include "Com_Cfg.h"
#include "Com.h"

const ComTxPduConfigType ComTxPduConfigs[] =//配置表
{
    {PDUID_COM_POWERTRAIN_TX, 0x100u, 100u, Com_PackPowertrain},
    {PDUID_COM_BODY_TX, 0x101u, 200u, Com_PackBodyStatus},
    {PDUID_COM_FAULT_TX, 0x102u, 500u, Com_PackFaultStatus}
};

const uint8_t ComTxPduConfigCount =
    (uint8_t)(sizeof(ComTxPduConfigs) / sizeof(ComTxPduConfigs[0]));//计算配置项数量

const ComTxPduConfigType *Com_CfgGetTxPdu(PduIdType pdu_id)//查找函数
{
    uint8_t index;

    for (index = 0u; index < ComTxPduConfigCount; index++)
    {
        if (ComTxPduConfigs[index].pduId == pdu_id)
        {
            return &ComTxPduConfigs[index];
        }
    }

    return (const ComTxPduConfigType *)0;
}

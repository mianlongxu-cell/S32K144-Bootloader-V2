#ifndef DCM_CFG_H_
#define DCM_CFG_H_

#include <stdint.h>

typedef uint8_t (*DcmReadDidFncType)(uint8_t *data, uint16_t *length);

typedef struct
{
    uint16_t did;//DID 编号
    DcmReadDidFncType readFunction;//读取这个 DID 的函数
} DcmDidConfigType;//DID 配置结构体

extern const DcmDidConfigType DcmDidConfigs[];
extern const uint8_t DcmDidConfigCount;
const DcmDidConfigType *Dcm_CfgGetDid(uint16_t did);//DID 查询函数

#endif /* DCM_CFG_H_ */

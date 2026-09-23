#ifndef DIAG_CONFIG_H_
#define DIAG_CONFIG_H_

#define UDS_REQUEST_ID              0x7E0u //诊断仪发0x7E0
#define UDS_RESPONSE_ID             0x7E8u//ECU回复

#define DIAG_MAX_PDU_LENGTH         256u//最大诊断PDU

#define DCM_S3_TIMEOUT_MS           5000u//S3 Server 会话超时为 5000 ms
#define DCM_P2_SERVER_MAX_MS        50u  //P2 Server 最大响应时间为 50 ms
#define DCM_P2_STAR_SERVER_MAX_MS   5000u  //P2* Server 最大响应时间为 5000 ms
#define CANTP_N_BS_TIMEOUT_MS       1000u  //CU 作为多帧发送方，如果 1 秒内没有收到 FC，则认为本次传输超时并中止
#define CANTP_N_CR_TIMEOUT_MS       1000u  //ECU 作为多帧接收方时，如果相邻连续帧CF等待超过 1 秒，则接收超时并中止。

/* Flow Control values sent by this ECU while receiving a long request. */
#define CANTP_RX_BLOCK_SIZE         0u //设置为 0 表示发送方可以连续发送剩余的所有连续帧，中途不需要等待新的流控帧。
#define CANTP_RX_STMIN_MS           0u //这是 ECU 接收长请求时，通过流控帧告诉发送方的最小连续帧间隔 STmin

#define DID_VIN                     0xF190u

/* Project-specific learning DIDs; these are not claimed as standard DIDs. */
#define DID_SW_VERSION              0xF100u //软件版本
#define DID_VEHICLE_SPEED           0x0101u //车速
#define DID_ENGINE_RPM              0x0102u //转速
#define DID_COOLANT_TEMP            0x0103u//水温
#define DID_GEAR                    0x0104u //挡位

#endif /* DIAG_CONFIG_H_ */

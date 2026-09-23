#include <stdint.h>
#include "boot_manager.h"
#include "boot_programming.h"
#include "boot_protocol_cfg.h"
#include "boot_security.h"
#include "boot_uds.h"
#include "boot_update.h"
#include "boot_lifecycle.h"
extern void BootManager_RefreshActiveSlot(void);
static uint8_t session;
static bool reset_pending;
static uint32_t transfer_timeout;
static uint32_t read32(const uint8_t *p)
{ return ((uint32_t)p[0]<<24) | ((uint32_t)p[1]<<16) | ((uint32_t)p[2]<<8) | p[3]; }
static void write32(uint8_t *p, uint32_t v)
{ p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }
static void negative(BootIsoTp_PduType *r, uint8_t sid, uint8_t nrc)
{ r->length=3u; r->data[0]=0x7Fu; r->data[1]=sid; r->data[2]=nrc; }
static bool pending(void)
{
    BootIsoTp_PduType r;
    negative(&r, 0x31u, 0x78u);
    return BootIsoTp_Transmit(&r);
}
void BootUds_Init(void)
{
    session=1u; reset_pending=false; transfer_timeout=0u;
    BootSecurity_Init();
    BootProgramming_Init(BootManager_GetActiveSlot(), BootManager_GetActiveVersion());
}
void BootUds_AbortDownload(void)
{
    if (BootUpdate_JournalValid && BootUpdate_Current.state >= BOOT_UPDATE_PREPARING &&
        BootUpdate_Current.state < BOOT_UPDATE_PENDING_ACTIVATION) {
        (void)BootUpdate_Abort(BOOT_UPDATE_TRANSFER_FAILED);
    }
    BootProgramming_Abort(); transfer_timeout=0u;
}
void BootUds_MainFunction(void)
{
    if (BootProgramming_Context.download_active &&
        ++transfer_timeout >= BOOT_TRANSFER_TIMEOUT_LOOPS) { BootUds_AbortDownload(); }
}
bool BootUds_ProcessRequest(const BootIsoTp_PduType *q, BootIsoTp_PduType *r)
{
    uint8_t sid, nrc=0u;
    uint16_t routine, did, star;
    uint32_t seed;
    if (q == (const BootIsoTp_PduType *)0 || r == (BootIsoTp_PduType *)0 ||
        q->length == 0u || q->length > BOOT_ISOTP_MAX_PDU_LENGTH) { return false; }
    sid=q->data[0]; r->length=1u; r->data[0]=(uint8_t)(sid+0x40u);
    BootProgramming_SetAccess(session==2u, BootSecurity_IsUnlocked());
    switch (sid) {
    case 0x10u:
        if (q->length!=2u) { nrc=0x13u; break; }
        if (q->data[1]!=1u && q->data[1]!=2u) { nrc=0x12u; break; }
        /* Re-entering a session cancels any old download/security authorization. */
        session=q->data[1]; BootSecurity_Init(); BootUds_AbortDownload();
        BootProgramming_SetAccess(session==2u, false);
        star=BOOT_P2_STAR_SERVER_MAX_MS/10u;
        r->length=6u; r->data[1]=session;
        r->data[2]=(uint8_t)(BOOT_P2_SERVER_MAX_MS>>8); r->data[3]=(uint8_t)BOOT_P2_SERVER_MAX_MS;
        r->data[4]=(uint8_t)(star>>8); r->data[5]=(uint8_t)star;
        break;
    case 0x27u:
        if (session!=2u) { nrc=0x22u; break; }
        if (q->length==2u && q->data[1]==1u) {
            seed=BootSecurity_GenerateSeed(); r->length=6u; r->data[1]=1u;
            write32(&r->data[2],seed);
        } else if (q->length==6u && q->data[1]==2u) {
            if (!BootSecurity_ValidateKey(read32(&q->data[2]))) { nrc=0x35u; break; }
            r->length=2u; r->data[1]=2u;
        } else { nrc=0x13u; }
        BootProgramming_SetAccess(session==2u, BootSecurity_IsUnlocked());
        break;
    case 0x22u:
        if (q->length!=3u) { nrc=0x13u; break; }
        did=((uint16_t)q->data[1]<<8)|q->data[2];
        r->data[1]=q->data[1]; r->data[2]=q->data[2];
        if (did==0xF101u || did==0xF103u) {
            BootSlotIdType slot=did==0xF101u ? BootProgramming_Context.active_slot :
                                                    BootProgramming_Context.target_slot;
            r->length=4u; r->data[3]=slot==BOOT_SLOT_ID_UNKNOWN ? 0xFFu : (uint8_t)slot;
        } else if (did==0xF102u) {
            r->length=7u; write32(&r->data[3],BootProgramming_Context.active_version);
        } else if (did==0xF104u) {
            const BootUpdateJournal *j = &BootUpdate_Current;
            /* Payload: ABI:u8, valid:u8, fault:u8, reserved:u8, then 7 BE u32. */
            r->length=35u; r->data[3]=1u; r->data[4]=(uint8_t)BootUpdate_JournalValid;
            r->data[5]=(uint8_t)BootUpdate_StorageFault; r->data[6]=0u;
            write32(&r->data[7],j->transaction_id); write32(&r->data[11],j->state);
            write32(&r->data[15],j->active_slot); write32(&r->data[19],j->target_slot);
            write32(&r->data[23],j->expected_size); write32(&r->data[27],j->committed_size);
            write32(&r->data[31],BootUpdate_StorageFault ? BOOT_UPDATE_JOURNAL_ERROR : j->last_result);
        } else if (did==0xF105u) {
            BootSlotMetadataType a={0},b={0};
            BootMetadataStatus as,bs;
            (void)BootLifecycle_GetMetadata(BOOT_SLOT_ID_A,&a,&as);
            (void)BootLifecycle_GetMetadata(BOOT_SLOT_ID_B,&b,&bs);
            /* Payload ABI 1 (40 bytes): header[4], A[12], B[12], result[12]. */
            r->length=43u;r->data[3]=1u;
            r->data[4]=(uint8_t)((as==BOOT_METADATA_STATUS_VALID?1u:0u)|
                                 (bs==BOOT_METADATA_STATUS_VALID?2u:0u));
            r->data[5]=BootProgramming_Context.active_slot==BOOT_SLOT_ID_UNKNOWN?0xFFu:
                       (uint8_t)BootProgramming_Context.active_slot;
            r->data[6]=(uint8_t)BootLifecycle_GetResetReason();
            r->data[7]=as==BOOT_METADATA_STATUS_VALID?(uint8_t)a.state:
                       (as==BOOT_METADATA_STATUS_EMPTY?0u:0xFEu);
            r->data[8]=as==BOOT_METADATA_STATUS_VALID?(uint8_t)a.boot_attempts:0u;
            r->data[9]=0u;r->data[10]=0u;write32(&r->data[11],a.image_version);write32(&r->data[15],a.sequence);
            r->data[19]=bs==BOOT_METADATA_STATUS_VALID?(uint8_t)b.state:
                        (bs==BOOT_METADATA_STATUS_EMPTY?0u:0xFEu);
            r->data[20]=bs==BOOT_METADATA_STATUS_VALID?(uint8_t)b.boot_attempts:0u;
            r->data[21]=0u;r->data[22]=0u;write32(&r->data[23],b.image_version);write32(&r->data[27],b.sequence);
            r->data[31]=(uint8_t)BootLifecycle_GetLastResult();
            r->data[32]=BootLifecycle_GetFailedSlot()==BOOT_SLOT_ID_UNKNOWN?0xFFu:(uint8_t)BootLifecycle_GetFailedSlot();
            r->data[33]=BootLifecycle_GetRollbackTarget()==BOOT_SLOT_ID_UNKNOWN?0xFFu:(uint8_t)BootLifecycle_GetRollbackTarget();
            r->data[34]=(uint8_t)BootLifecycle_StorageFault();
            write32(&r->data[35],BootLifecycle_GetFailedVersion());
            write32(&r->data[39],BootLifecycle_GetLastAttemptCount());
        } else { nrc=0x31u; }
        break;
    case 0x31u:
        if (q->length<4u) { nrc=0x13u; break; }
        if (q->data[1]!=1u) { nrc=0x12u; break; }
        routine=((uint16_t)q->data[2]<<8)|q->data[3];
        if (routine==BOOT_ROUTINE_ERASE_APP) {
            if (q->length!=12u) { nrc=0x13u; break; }
            nrc=BootProgramming_Erase(read32(&q->data[4]),read32(&q->data[8]),pending);
        } else if (routine==BOOT_ROUTINE_VERIFY_CRC) {
            if (q->length!=4u) { nrc=0x13u; break; }
            nrc=BootProgramming_Verify(pending);
        } else if (routine==0xFF02u) {
            BootSlotIdType active;
            if (q->length!=5u) { nrc=0x13u; break; }
            if (session!=2u) { nrc=0x22u; break; }
            if (!BootSecurity_IsUnlocked()) { nrc=0x33u; break; }
            if (q->data[4]>1u && q->data[4]!=0xFFu) { nrc=0x31u; break; }
            active=q->data[4]==0xFFu ? BOOT_SLOT_ID_UNKNOWN : (BootSlotIdType)q->data[4];
            if (!pending() || !BootUpdate_InitializeJournal(active)) { nrc=0x72u; break; }
            if (!BootLifecycle_Init(BootLifecycle_GetResetReason())) { nrc=0x72u; break; }
            BootManager_RefreshActiveSlot();
            BootProgramming_Init(BootManager_GetActiveSlot(),BootManager_GetActiveVersion());
            BootProgramming_SetAccess(true,true);
        } else { nrc=0x31u; }
        r->length=4u; r->data[1]=1u; r->data[2]=q->data[2]; r->data[3]=q->data[3];
        break;
    case 0x34u:
        if (q->length!=11u) { nrc=0x13u; break; }
        if (q->data[1]!=0u || q->data[2]!=0x44u) { nrc=0x31u; break; }
        nrc=BootProgramming_Download(read32(&q->data[3]),read32(&q->data[7]));
        r->length=4u; r->data[1]=0x20u;
        r->data[2]=(uint8_t)(BootProgramming_Context.max_block_length>>8);
        r->data[3]=(uint8_t)BootProgramming_Context.max_block_length;
        transfer_timeout=0u;
        break;
    case 0x36u:
        if (q->length<3u) { nrc=0x13u; break; }
        nrc=BootProgramming_Transfer(q->data[1],&q->data[2],q->length-2u);
        r->length=2u; r->data[1]=q->data[1];
        if (nrc==0u) { transfer_timeout=0u; }
        break;
    case 0x37u:
        if (q->length!=1u) { nrc=0x13u; break; }
        nrc=BootProgramming_Exit();
        break;
    case 0x11u:
        if (q->length!=2u) { nrc=0x13u; break; }
        if (q->data[1]!=1u) { nrc=0x12u; break; }
        if (session!=2u) { nrc=0x22u; break; }
        if (!BootSecurity_IsUnlocked()) { nrc=0x33u; break; }
        if (!BootProgramming_CanReset()) { nrc=0x24u; break; }
        reset_pending=true; r->length=2u; r->data[1]=1u;
        break;
    case 0x3Eu:
        if (q->length!=2u || q->data[1]!=0u) { nrc=0x13u; break; }
        r->length=2u; r->data[1]=0u; break;
    default: nrc=0x11u; break;
    }
    if (nrc!=0u) { negative(r,sid,nrc); }
    return true;
}
bool BootUds_IsResetPending(void) { return reset_pending; }
bool BootUds_IsProgrammingActive(void) { return session==2u; }

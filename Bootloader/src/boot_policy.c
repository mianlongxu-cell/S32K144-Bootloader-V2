#include "boot_policy.h"
#include "boot_version.h"

static uint32_t lifecycle_priority(BootImageState state)
{
    return state==BOOT_IMAGE_STATE_TRIAL?3u:
          (state==BOOT_IMAGE_STATE_PENDING?2u:
          (state==BOOT_IMAGE_STATE_CONFIRMED?1u:0u));
}

BootTargetType BootPolicy_SelectLifecycle(const BootImageInfoType *a,BootImageState sa,
                                          const BootImageInfoType *b,BootImageState sb)
{
    uint32_t pa,pb;
    if(a==0||b==0){return BOOT_TARGET_PROGRAMMING;}
    pa=a->valid?lifecycle_priority(sa):0u;pb=b->valid?lifecycle_priority(sb):0u;
    if(pa&&!pb){return BOOT_TARGET_SLOT_A;}if(!pa&&pb){return BOOT_TARGET_SLOT_B;}
    if(!pa&&!pb){return BOOT_TARGET_PROGRAMMING;}
    if(pa>pb){return BOOT_TARGET_SLOT_A;}if(pb>pa){return BOOT_TARGET_SLOT_B;}
    return BootVersion_Compare(a->header.software_version,b->header.software_version)==BOOT_VERSION_LESS?
           BOOT_TARGET_SLOT_B:BOOT_TARGET_SLOT_A;
}

BootTargetType BootPolicy_Select(const BootImageInfoType *slot_a,
                                 const BootImageInfoType *slot_b)
{
    if ((slot_a == (const BootImageInfoType *)0) ||
        (slot_b == (const BootImageInfoType *)0))
    {
        return BOOT_TARGET_PROGRAMMING;
    }
    if (slot_a->valid && !slot_b->valid)
    {
        return BOOT_TARGET_SLOT_A;
    }
    if (!slot_a->valid && slot_b->valid)
    {
        return BOOT_TARGET_SLOT_B;
    }
    if (!slot_a->valid && !slot_b->valid)
    {
        return BOOT_TARGET_PROGRAMMING;
    }

    if (BootVersion_Compare(slot_a->header.software_version,
                            slot_b->header.software_version) ==
        BOOT_VERSION_LESS)
    {
        return BOOT_TARGET_SLOT_B;
    }
    /* Slot A wins for a newer or exactly equal version. */
    return BOOT_TARGET_SLOT_A;
}

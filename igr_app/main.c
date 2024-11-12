//
// Based on nox & El_isa's SD2PSXMAN test app & Based_skid's Reboot elf
// See LICENSE for license and warranty information.
//

// sd2psxman
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <loadfile.h>
#include <debug.h>

#include "sd2psxman_rpc.h"
#include "../sd2psxman_common.h"

// reboot
#include <kernel.h>
#include <sifrpc.h>
#include <sbv_patches.h>

#define IRX_DEFINE(mod)               \
    extern unsigned char mod##_irx[]; \
    extern unsigned int size_##mod##_irx

#define IRX_LOAD(mod)                                                        \
    if (SifExecModuleBuffer(mod##_irx, size_##mod##_irx, 0, NULL, NULL) < 0) \
    printf("Could not load ##mod##\n")

#define xprintf(f_, ...)         \
    printf((f_), ##__VA_ARGS__); \
    scr_printf("    ");          \
    scr_printf((f_), ##__VA_ARGS__);
#define GS_BGCOLOUR(x) *((volatile unsigned long int *)0x120000E0) = x

#define MOD_ERR(ID, RET) (ID < 0 || RET == 1)

IRX_DEFINE(sd2psxman);


// not vsynced, but somewhere in the ballpack
// of 60fps
void DelayFrame()
{
    struct timespec tv;
    tv.tv_sec = 0;
    tv.tv_nsec = 16000000;
    nanosleep(&tv, NULL);
}

void err(const char* mod, int id, int ret) {
    scr_setfontcolor(0x0000DD);
    xprintf("\n\n\tERROR ON MODULE %s: id:%d ret:%d", mod, id, ret);
    sleep(20);
    GotoOSDSYS();
}

void MountBootCard(void)
{
    int rv = sd2psxman_get_card(PORT_MEMCARD1, 0);

    if (rv > -1)
    {
        sd2psxman_set_card(PORT_MEMCARD1, 0, SD2PSXMAN_CARD_TYPE_BOOT, SD2PSXMAN_MODE_NUM, 0);

        // 30's enough, but might as well give the
        // card a chance to breathe, flush, etc
        for (int i = 0; i < 120; i++)
        {
            DelayFrame();
        }
    }
}

// Totally codemonkied with minimal understanding
// Thanks Based_skid for this sequence
void ResetIOP()
{
    // Thanks To SP193 For Clarifying This
    SifInitRpc(0);      // Initialize SIFRPC and SIFCMD. Although seemingly unimportant, this will update the addresses on the EE, which can prevent a crash from happening around the IOP reboot.
    SifIopReset("", 0); // Reboot IOP with default modules (empty command line)
    while (!SifIopSync())
    {
    }                  // Wait for IOP to finish rebooting.
    SifInitRpc(0);     // Initialize SIFRPC and SIFCMD.
    SifLoadFileInit(); // Initialize LOADFILE RPC.
    fioInit();         // Initialize FILEIO RPC.
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
}

void GotoOSDSYS()
{
    ResetIOP();
    LoadExecPS2("rom0:OSDSYS", 0, NULL);
}

void LittleWait()
{
    for (int i = 0; i < 1; i++)
    {
        DelayFrame();
    }
}

int main()
{
    int id, ret;
    ResetIOP();
    FlushCache(0);
    FlushCache(2);

    init_scr();

    xprintf("Loading...\n");

    // El_isra: I would be against using ROM0 modules. but since we just wanna send cmds it should b ok
    ret = SifLoadStartModule("rom0:SIO2MAN", 0, NULL, &id);
    LittleWait();
    if (MOD_ERR(id, ret)) err("rom0:SIO2MAN", id, ret);
    
    ret = SifExecModuleBuffer(sd2psxman_irx, size_sd2psxman_irx, 0, NULL, &id);
    LittleWait();
    if (MOD_ERR(id, ret)) err("SD2PSXMAN", id, ret);
    
    sd2psxman_init();
    LittleWait();
    
    ret = SifLoadStartModule("rom0:MCMAN", 0, NULL, &id);
    LittleWait();
    if (MOD_ERR(id, ret)) err("rom0:MCMAN", id, ret);

    xprintf("Mounting boot card...\n");
    MountBootCard();
    xprintf("Rebooting...\n");

    GotoOSDSYS();

    return 0;
}


void _libcglue_timezone_update()
{
}

DISABLE_PATCHED_FUNCTIONS();

PS2_DISABLE_AUTOSTART_PTHREAD();
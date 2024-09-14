#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>
#include <linux/usbdevice_fs.h>
#include <linux/types.h>
#include <net/if.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
typedef int   BOOL;

#define dbg_time(fmt, args...) do { \
    fprintf(stdout,  fmt "\n", ##args); \
        fflush(stdout); \
} while(0)

#define TRUE (1 == 1)
#define FALSE (1 != 1)


#define CM_MAX_PATHLEN 256

#define CM_INVALID_VAL (~((int)0))

struct usb_device_info {
    int idVendor;
    int idProduct;
    int  busnum;
    int devnum;
    int bNumInterfaces;
};

struct usb_interface_info {
    int bNumEndpoints;
    int bInterfaceClass;
    int bInterfaceSubClass;
    int bInterfaceProtocol;
    char driver[32];
};

typedef struct __PROFILE {

    char atport[32];
    char qmapnet_adapter[32];
    char driver_name[32];

//    bool reattach_flag;
    int hardware_interface;
    int software_interface;

    struct usb_device_info usb_dev;
    struct usb_interface_info usb_intf;

} PROFILE_T;

static PROFILE_T s_profile;

/* get first line from file 'fname'
 * And convert the content into a hex number, then return this number */
static int file_get_value(const char *fname, int base)
{
    FILE *fp = NULL;
    long num;
    char buff[32 + 1] = {'\0'};
    char *endptr = NULL;

    fp = fopen(fname, "r");
    if (!fp) goto error;
    if (fgets(buff, sizeof(buff), fp) == NULL)
        goto error;
    fclose(fp);

    num = (int)strtol(buff, &endptr, base);
    if (errno == ERANGE && (num == LONG_MAX || num == LONG_MIN))
        goto error;
    /* if there is no digit in buff */
    if (endptr == buff)
        goto error;

    return (int)num;

error:
    if (fp) fclose(fp);
    return -1;
}

/*
 * This function will search the directory 'dirname' and return the first child.
 * '.' and '..' is ignored by default
 */
static int dir_get_child(const char *dirname, char *buff, unsigned bufsize, const char *prefix)
{
    struct dirent *entptr = NULL;
    DIR *dirptr;

    buff[0] = 0;

    dirptr = opendir(dirname);
    if (!dirptr)
        return -1;

    while ((entptr = readdir(dirptr))) {
        if (entptr->d_name[0] == '.')
            continue;
        if (prefix && strlen(prefix) && strncmp(entptr->d_name, prefix, strlen(prefix)))
            continue;
        snprintf(buff, bufsize, "%.31s", entptr->d_name);
        break;
    }
    closedir(dirptr);

    return 0;
}

static void query_usb_device_info(char *path, struct usb_device_info *p) {
    size_t offset = strlen(path);

    memset(p, 0, sizeof(*p));

    path[offset] = '\0';
    strcat(path, "/idVendor");
    p->idVendor = file_get_value(path, 16);

    if (p->idVendor == CM_INVALID_VAL)
        return;

    path[offset] = '\0';
    strcat(path, "/idProduct");
    p->idProduct = file_get_value(path, 16);

    path[offset] = '\0';
    strcat(path, "/busnum");
    p->busnum = file_get_value(path, 10);

    path[offset] = '\0';
    strcat(path, "/devnum");
    p->devnum = file_get_value(path, 10);

    path[offset] = '\0';
    strcat(path, "/bNumInterfaces");
    p->bNumInterfaces = file_get_value(path, 10);

    path[offset] = '\0';
}

/* To detect the device info of the modem.
 * return:
 *  FALSE -> fail
 *  TRUE -> ok
 */
BOOL atport_detect(char *atport, unsigned bufsize, PROFILE_T *profile) {
    struct dirent* ent = NULL;
    DIR *pDir;
    const char *rootdir = "/sys/bus/usb/devices";
    struct {
        char path[255*2];
    } *pl;
    pl = (typeof(pl)) malloc(sizeof(*pl));
    memset(pl, 0x00, sizeof(*pl));

    pDir = opendir(rootdir);
    if (!pDir) {
        dbg_time("opendir %s failed: %s", rootdir, strerror(errno));
        goto error;
    }

    while ((ent = readdir(pDir)) != NULL)  {
//        char netcard[32+1] = {'\0'};
        char devname[32+5] = {'\0'}; //+strlen("/dev/")
//        int netIntf;
 //       int driver_type;
        int atIntf = -1;

        if (ent->d_name[0] == 'u')
            continue;

        snprintf(pl->path, sizeof(pl->path), "%s/%s", rootdir, ent->d_name);
        query_usb_device_info(pl->path, &profile->usb_dev);
        if (profile->usb_dev.idVendor == CM_INVALID_VAL)
            continue;

        if (profile->usb_dev.idVendor == 0x2c7c || profile->usb_dev.idVendor == 0x05c6 
		|| profile->usb_dev.idVendor == 0x3763) {
            dbg_time("Find %s/%s idVendor=0x%x idProduct=0x%x, bus=0x%03x, dev=0x%03x",
                rootdir, ent->d_name, profile->usb_dev.idVendor, profile->usb_dev.idProduct,
                profile->usb_dev.busnum, profile->usb_dev.devnum);
        }

        if (1)
        {

            if (profile->usb_dev.idVendor == 0x2c7c) { //Quectel
                switch (profile->usb_dev.idProduct) { //EC200U
                case 0x0901: //EC200U
                case 0x8101: //RG801H
                    atIntf = 2;
                break;
                case 0x0900: //RG500U
                    atIntf = 4;
                break;
                case 0x6026: //EC200T
                case 0x6005: //EC200A
                case 0x6002: //EC200S
                case 0x6001: //EC100Y
                    atIntf = 3;
                break;
                default:
                   dbg_time("unknow at interface for USB idProduct:%04x\n", profile->usb_dev.idProduct);
                break;
                }
            }

            if (atIntf != -1) {
                snprintf(pl->path, sizeof(pl->path), "%s/%s:1.%d", rootdir, ent->d_name, atIntf);
                dir_get_child(pl->path, devname, sizeof(devname), "tty");
                if (devname[0] && !strcmp(devname, "tty")) {
                    snprintf(pl->path, sizeof(pl->path), "%s/%s:1.%d/tty", rootdir, ent->d_name, atIntf);
                    dir_get_child(pl->path, devname, sizeof(devname), "tty");
                }
            }
        }
        
        if ( devname[0]) {
            if (devname[0] == '/')
                snprintf(atport, bufsize, "%s", devname);
            else
                snprintf(atport, bufsize, "/dev/%s", devname);
            dbg_time("Auto find atport = %s", atport);
            break;
        }
    }
    closedir(pDir);

    if (atport[0] == '\0') {
        dbg_time(" AT port '%s' is not exist", atport);
        goto error;
    }
    free(pl);
    return TRUE;
error:
    free(pl);
    return FALSE;
}




BOOL ppp_port_detect(char *ppp_port, unsigned bufsize, PROFILE_T *profile) {
    struct dirent* ent = NULL;
    DIR *pDir;
    const char *rootdir = "/sys/bus/usb/devices";
    struct {
        char path[255*2];
    } *pl;
    pl = (typeof(pl)) malloc(sizeof(*pl));
    memset(pl, 0x00, sizeof(*pl));

    pDir = opendir(rootdir);
    if (!pDir) {
        dbg_time("opendir %s failed: %s", rootdir, strerror(errno));
        goto error;
    }

    while ((ent = readdir(pDir)) != NULL)  {
        char devname[32+5] = {'\0'}; //+strlen("/dev/")
//        int driver_type;

        if (ent->d_name[0] == 'u')
            continue;

        snprintf(pl->path, sizeof(pl->path), "%s/%s", rootdir, ent->d_name);
        query_usb_device_info(pl->path, &profile->usb_dev);
        if (profile->usb_dev.idVendor == CM_INVALID_VAL)
            continue;

        if (profile->usb_dev.idVendor == 0x2c7c || profile->usb_dev.idVendor == 0x05c6 ) {
            dbg_time("Find %s/%s idVendor=0x%x idProduct=0x%x, bus=0x%03x, dev=0x%03x",
                rootdir, ent->d_name, profile->usb_dev.idVendor, profile->usb_dev.idProduct,
                profile->usb_dev.busnum, profile->usb_dev.devnum);
        }

        if (1)
        {
            int atIntf = -1;

            if (profile->usb_dev.idVendor == 0x2c7c) { //Quectel
                switch (profile->usb_dev.idProduct) { //EC200U
                case 0x0901: //EC200U
                case 0x8101: //RG801H
                    atIntf = 8;
                break;
                case 0x0900: //RG500U
                    atIntf = 5;
                break;
                case 0x6026: //EC200T
                case 0x6005: //EC200A
                case 0x6002: //EC200S
                case 0x6001: //EC100Y
                    atIntf = 4;
                break;
                default:
                   dbg_time("unknow at interface for USB idProduct:%04x\n", profile->usb_dev.idProduct);
                break;
                }
            }

            if (atIntf != -1) {
                snprintf(pl->path, sizeof(pl->path), "%s/%s:1.%d", rootdir, ent->d_name, atIntf);
                dir_get_child(pl->path, devname, sizeof(devname), "tty");
                if (devname[0] && !strcmp(devname, "tty")) {
                    snprintf(pl->path, sizeof(pl->path), "%s/%s:1.%d/tty", rootdir, ent->d_name, atIntf);
                    dir_get_child(pl->path, devname, sizeof(devname), "tty");
                }
            }
        }
        
        if ( devname[0]) {
            if (devname[0] == '/')
                snprintf(ppp_port, bufsize, "%s", devname);
            else
                snprintf(ppp_port, bufsize, "/dev/%s", devname);
            dbg_time("Auto find ppp port = %s", ppp_port);
            break;
        }
    }
    closedir(pDir);

    if (ppp_port[0] == '\0') {
        dbg_time(" ppp port '%s' is not exist", ppp_port);
        goto error;
    }
    free(pl);
    return TRUE;
error:
    free(pl);
    return FALSE;
}


int main()
{
//    int ret;
    PROFILE_T *profile = &s_profile;
    char atport[32] = {'\0'};
    char ppp_port[32] = {'\0'};

    dbg_time("Quectel auto find the ttyUSB and interface");

    atport_detect(atport,sizeof(atport), profile);
    ppp_port_detect(ppp_port,sizeof(ppp_port), profile);

    return 0;
}

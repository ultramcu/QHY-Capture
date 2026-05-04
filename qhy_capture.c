/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 MaIII Themd
 */

/******************************************************************
 * qhy_capture -- minimal single-frame capture demo for QHYCCD cameras
 *
 * Connects to the first QHY CCD/CMOS camera detected on the USB bus,
 * configures gain / offset / exposure, takes one full-resolution
 * single-frame exposure, and writes the result to disk as a PGM
 * (mono) or PPM (color) image.
 *
 * Build : see README.md and the accompanying Makefile.
 * Run   : ./qhy_capture [-e exposure_us] [-g gain] [-o out_file]
 *
 * Origin: extracted and cleaned from the 2016 app_qhy_opencv demo
 *         (Qt-Creator project, OpenCV / C++ artefacts dropped).
 ******************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <libqhy/qhyccd.h>

/* Defaults (overridable via CLI). */
static const char  *g_out_path     = "frame.pgm";
static unsigned int g_exposure_us  = 20000;   /* 20 ms */
static unsigned int g_gain         = 30;
static unsigned int g_offset       = 140;
static unsigned int g_usb_traffic  = 30;
static unsigned int g_bin_x        = 1;
static unsigned int g_bin_y        = 1;
static unsigned int g_bits         = 16;

static void usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [-e exposure_us] [-g gain] [-o out_file] [-h]\n"
        "  -e N   exposure time in microseconds (default %u)\n"
        "  -g N   gain (default %u)\n"
        "  -o F   output file path (default \"%s\")\n"
        "         file format follows the captured frame:\n"
        "           1 channel  -> PGM (P5)\n"
        "           3 channels -> PPM (P6)\n"
        "  -h     show this help\n",
        argv0, g_exposure_us, g_gain, g_out_path);
}

static int parse_args(int argc, char *argv[])
{
    int c;
    while ((c = getopt(argc, argv, "e:g:o:h")) != -1) {
        switch (c) {
        case 'e': g_exposure_us = (unsigned)strtoul(optarg, NULL, 10); break;
        case 'g': g_gain        = (unsigned)strtoul(optarg, NULL, 10); break;
        case 'o': g_out_path    = optarg;                              break;
        case 'h': usage(argv[0]); return -1;
        default:  usage(argv[0]); return -1;
        }
    }
    return 0;
}

/* Write the captured frame to `path` as PGM (mono) or PPM (color).
 * For 16-bit data, bytes are swapped to big-endian (PNM spec).
 * Returns 0 on success, -1 on I/O failure. */
static int save_frame(const char  *path,
                      unsigned int w, unsigned int h,
                      unsigned int bpp, unsigned int channels,
                      const uint8_t *data)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror(path);
        return -1;
    }

    int          magic  = (channels == 3) ? 6 : 5;       /* P6 RGB / P5 mono */
    unsigned int maxval = (bpp <= 8) ? 255u : 65535u;
    fprintf(fp, "P%d\n%u %u\n%u\n", magic, w, h, maxval);

    size_t pixels  = (size_t)w * (size_t)h;
    size_t samples = pixels * (size_t)channels;
    int    rc      = 0;

    if (bpp <= 8) {
        if (fwrite(data, 1, samples, fp) != samples) {
            rc = -1;
        }
    } else {
        /* PNM 16-bit is big-endian; QHY SDK delivers little-endian. */
        const uint16_t *src = (const uint16_t *)data;
        for (size_t i = 0; i < samples && rc == 0; i++) {
            uint16_t v   = src[i];
            uint8_t  be[2] = { (uint8_t)(v >> 8), (uint8_t)(v & 0xFFu) };
            if (fwrite(be, 1, 2, fp) != 2) rc = -1;
        }
    }

    fclose(fp);
    return rc;
}

int main(int argc, char *argv[])
{
    qhyccd_handle *cam      = NULL;
    uint8_t       *img      = NULL;
    int            sdk_init = 0;
    int            rc       = 1;
    int            ret;
    char           id[32];

    if (parse_args(argc, argv) < 0) {
        return 1;
    }

    ret = InitQHYCCDResource();
    if (ret != QHYCCD_SUCCESS) {
        fprintf(stderr, "InitQHYCCDResource failed (%d)\n", ret);
        goto out;
    }
    sdk_init = 1;
    printf("SDK initialised.\n");

    int num = ScanQHYCCD();
    if (num <= 0) {
        fprintf(stderr, "No QHY camera found. Check USB cable / power.\n");
        goto out;
    }
    printf("Found %d QHY camera(s).\n", num);

    int found = 0;
    memset(id, 0, sizeof(id));
    for (int i = 0; i < num; i++) {
        if (GetQHYCCDId(i, id) == QHYCCD_SUCCESS) {
            found = 1;
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "GetQHYCCDId failed for all detected cameras.\n");
        goto out;
    }
    printf("Using camera id: %s\n", id);

    cam = OpenQHYCCD(id);
    if (cam == NULL) {
        fprintf(stderr, "OpenQHYCCD failed.\n");
        goto out;
    }

    /* 0 = single-frame mode, 1 = live (continuous) mode */
    if (SetQHYCCDStreamMode(cam, 0) != QHYCCD_SUCCESS) {
        fprintf(stderr, "SetQHYCCDStreamMode(single-frame) failed.\n");
        goto out;
    }

    if (InitQHYCCD(cam) != QHYCCD_SUCCESS) {
        fprintf(stderr, "InitQHYCCD failed.\n");
        goto out;
    }

    double       chipw, chiph, pixelw, pixelh;
    unsigned int max_w, max_h, max_bpp;
    if (GetQHYCCDChipInfo(cam, &chipw, &chiph,
                          &max_w, &max_h,
                          &pixelw, &pixelh, &max_bpp) != QHYCCD_SUCCESS) {
        fprintf(stderr, "GetQHYCCDChipInfo failed.\n");
        goto out;
    }
    printf("Chip: %.2f x %.2f mm, pixel %.2f x %.2f um, "
           "max resolution %u x %u @ %u bpp\n",
           chipw, chiph, pixelw, pixelh, max_w, max_h, max_bpp);

    /* If the camera reports a Bayer pattern, enable in-SDK debayer
     * and a neutral starting white balance. */
    int bayer = IsQHYCCDControlAvailable(cam, CAM_COLOR);
    if (bayer == BAYER_GB || bayer == BAYER_GR ||
        bayer == BAYER_BG || bayer == BAYER_RG) {
        printf("Color sensor (bayer code %d) -- enabling debayer.\n", bayer);
        SetQHYCCDDebayerOnOff(cam, true);
        SetQHYCCDParam(cam, CONTROL_WBR, 20);
        SetQHYCCDParam(cam, CONTROL_WBG, 20);
        SetQHYCCDParam(cam, CONTROL_WBB, 20);
    }

    if (IsQHYCCDControlAvailable(cam, CONTROL_USBTRAFFIC) == QHYCCD_SUCCESS) {
        SetQHYCCDParam(cam, CONTROL_USBTRAFFIC, g_usb_traffic);
    }
    if (IsQHYCCDControlAvailable(cam, CONTROL_GAIN) == QHYCCD_SUCCESS) {
        SetQHYCCDParam(cam, CONTROL_GAIN, g_gain);
    }
    if (IsQHYCCDControlAvailable(cam, CONTROL_OFFSET) == QHYCCD_SUCCESS) {
        SetQHYCCDParam(cam, CONTROL_OFFSET, g_offset);
    }

    if (SetQHYCCDParam(cam, CONTROL_EXPOSURE, g_exposure_us) != QHYCCD_SUCCESS) {
        fprintf(stderr, "SetQHYCCDParam(EXPOSURE=%u) failed.\n", g_exposure_us);
        goto out;
    }

    if (SetQHYCCDResolution(cam, 0, 0, max_w, max_h) != QHYCCD_SUCCESS) {
        fprintf(stderr, "SetQHYCCDResolution failed.\n");
        goto out;
    }
    if (SetQHYCCDBinMode(cam, g_bin_x, g_bin_y) != QHYCCD_SUCCESS) {
        fprintf(stderr, "SetQHYCCDBinMode(%u, %u) failed.\n", g_bin_x, g_bin_y);
        goto out;
    }
    if (IsQHYCCDControlAvailable(cam, CONTROL_TRANSFERBIT) == QHYCCD_SUCCESS) {
        if (SetQHYCCDBitsMode(cam, g_bits) != QHYCCD_SUCCESS) {
            fprintf(stderr, "SetQHYCCDBitsMode(%u) failed.\n", g_bits);
            goto out;
        }
    }

    printf("Exposing for %u us...\n", g_exposure_us);
    ret = ExpQHYCCDSingleFrame(cam);
    if (ret == QHYCCD_ERROR) {
        fprintf(stderr, "ExpQHYCCDSingleFrame failed.\n");
        goto out;
    }
    if (ret != QHYCCD_READ_DIRECTLY) {
        /* Wait long enough for exposure + a small readout margin. */
        usleep((useconds_t)g_exposure_us + 100000u);
    }

    uint32_t length = GetQHYCCDMemLength(cam);
    if (length == 0) {
        fprintf(stderr, "GetQHYCCDMemLength returned 0.\n");
        goto out;
    }
    img = (uint8_t *)calloc(1, length);
    if (img == NULL) {
        fprintf(stderr, "calloc(%u) failed.\n", length);
        goto out;
    }

    unsigned int got_w = 0, got_h = 0, got_bpp = 0, got_ch = 0;
    if (GetQHYCCDSingleFrame(cam, &got_w, &got_h,
                             &got_bpp, &got_ch, img) != QHYCCD_SUCCESS) {
        fprintf(stderr, "GetQHYCCDSingleFrame failed.\n");
        goto out;
    }
    printf("Captured frame: %u x %u, %u bpp, %u channel(s), %u bytes\n",
           got_w, got_h, got_bpp, got_ch, length);

    if (save_frame(g_out_path, got_w, got_h, got_bpp, got_ch, img) < 0) {
        fprintf(stderr, "Saving \"%s\" failed.\n", g_out_path);
        goto out;
    }
    printf("Saved \"%s\" (P%c, %s).\n",
           g_out_path,
           got_ch == 3 ? '6' : '5',
           got_bpp <= 8 ? "8-bit" : "16-bit big-endian");

    rc = 0;

out:
    free(img);
    if (cam != NULL) {
        CancelQHYCCDExposingAndReadout(cam);
        CloseQHYCCD(cam);
    }
    if (sdk_init) {
        ReleaseQHYCCDResource();
    }
    return rc;
}

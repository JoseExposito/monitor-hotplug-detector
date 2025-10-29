// SPDX-License-Identifier: GPL-3.0+

/*
 * monitor-hotplug-detector.c
 *
 * Simple program to detect hot-plug and unplug events using netlink sockets.
 *
 * For more information about netlink sockets:
 * https://docs.kernel.org/userspace-api/netlink/intro.html
 *
 * Compile with:
 *
 *   $ gcc monitor-hotplug-detector.c -o monitor-hotplug-detector
 *
 * And run with:
 *
 *   $ sudo systemctl isolate multi-user.target
 *   $ sudo ./monitor-hotplug-detector
 *
 * Useful command for validation:
 *
 *   $ sudo udevadm monitor --kernel --subsystem-match=drm
 */

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <linux/limits.h>
#include <linux/netlink.h>

#define DRM_PATH "/sys/class/drm"

bool query_connectors(void) {
    DIR *dir = opendir(DRM_PATH);
    if (!dir) {
        printf("Error opening %s\n", DRM_PATH);
        return false;
    }

    printf("Connector status:\n");

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        const char *prefix = "card";
        size_t prefix_len = strlen(prefix);

        bool starts_with_prefix = strncmp(entry->d_name, prefix, prefix_len) == 0;
        bool followed_by_dash = strcspn(entry->d_name, "-") > prefix_len;

        if (starts_with_prefix && followed_by_dash) {
            char status_path[PATH_MAX];
            char status[32];

            snprintf(status_path, sizeof(status_path), "%s/%s/status",
                     DRM_PATH, entry->d_name);

            FILE *status_file = fopen(status_path, "r");
            if (status_file) {
                if (fgets(status, sizeof(status), status_file)) {
                    printf("  %s: %s", entry->d_name, status);
                }
                fclose(status_file);
            }
        }
    }

    closedir(dir);
    return true;
}

int main(void) {
    int ret = 0;

    // Create the netlink socket
    int sock = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (sock < 0) {
        printf("Error opening netlink socket\n");
        ret = -1;
        goto exit;
    }

    // Listen to all multicast events
    struct sockaddr_nl sa;
    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    sa.nl_pid = getpid();
    sa.nl_groups = 1;

    ret = bind(sock, (struct sockaddr*)&sa, sizeof(sa));
    if (ret < 0) {
        printf("Error binding netlink socket\n");
        goto exit;
    }

    printf("Listening for monitor hot-plug/unplug events...\n");    
    for (;;) {
        char buf[2048];

        if (!query_connectors()) {
            printf("Error querying connectors\n");
            ret = -1;
            goto exit;
        }

        ssize_t len = recv(sock, buf, sizeof(buf) - 1, 0);
        if (len < 0) {
            printf("Error receiving message\n");
            ret = -1;
            goto exit;
        }

        buf[len] = '\0';

        printf("Message received:\n");
        printf("%s\n\n", buf);
    }

exit:
    close(sock);
    return ret;
}

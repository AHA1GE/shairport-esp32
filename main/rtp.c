/*
 * Apple RTP protocol handler. This file is part of Shairport.
 * Copyright (c) James Laird 2013
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "common.h"
#include "player.h"
#include "esp_log.h"

static const char* TAG = "RTP";

// only one RTP session can be active at a time.


static SOCKADDR rtp_client;
static int sock = -1;

// Keep the receive packet buffer off the RTP task stack.
static uint8_t rtp_packet[2048];

static TaskHandle_t rtp_thread;
static TaskHandle_t rtp_owner_thread;
static SemaphoreHandle_t rtp_stop_sem;

static void ensure_rtp_stop_sem(void) {
    if (rtp_stop_sem == NULL)
        rtp_stop_sem = xSemaphoreCreateBinary();
}

static void rtp_receiver(void* arg) {
    ESP_LOGD(TAG, "Starting RTP thread");
    // we inherit the signal mask (SIGUSR1)
    uint8_t *pktp;

    ssize_t nread;
    while (1) {
        if (ulTaskNotifyTake(pdTRUE, 0))
            break;
        nread = recv(sock, rtp_packet, sizeof(rtp_packet), 0);
        if (nread < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                continue;
            if (errno != ENOTCONN && errno != ECONNRESET && errno != ECONNABORTED && errno != EBADF)
                ESP_LOGD(TAG, "recv failure: %s", strerror(errno));
            break;
        }

        ssize_t plen = nread;
        uint8_t type = rtp_packet[1] & ~0x80;
        if (type == 0x54) // sync
            continue;
        if (type == 0x60 || type == 0x56) {   // audio data / resend
            pktp = rtp_packet;
            if (type==0x56) {
                pktp += 4;
                plen -= 4;
            }
            seq_t seqno = ntohs(*(unsigned short *)(pktp+2));

            pktp += 12;
            plen -= 12;

            // check if packet contains enough content to be reasonable
            if (plen >= 16) {
                player_put_packet(seqno, pktp, plen);
                continue;
            }
            if (type == 0x56 && seqno == 0) {
                ESP_LOGV(TAG, "resend-related request packet received, ignoring.\n");
                continue;
            }
            ESP_LOGV(TAG, "Unknown RTP packet of type 0x%02X length %d seqno %d\n", type, nread, seqno);
            continue;
        }
        ESP_LOGW(TAG, "Unknown RTP packet of type 0x%02X length %d", type, nread);
    }

    ESP_LOGD(TAG, "RTP thread interrupted. terminating.\n");
    if (sock >= 0) {
        close(sock);
        sock = -1;
    }
    rtp_thread = NULL;
    if (rtp_stop_sem != NULL)
        xSemaphoreGive(rtp_stop_sem);
    vTaskDelete(NULL);
}

static int bind_port(SOCKADDR *remote) {
    struct addrinfo hints, *info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = remote->SAFAMILY;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    int ret = getaddrinfo(NULL, "0", &hints, &info);

    //if (ret < 0)
    //    ESP_LOGE(TAG, "failed to get usable addrinfo?! %s", gai_strerror(ret));

    sock = socket(remote->SAFAMILY, SOCK_DGRAM, IPPROTO_UDP);
    ret = bind(sock, info->ai_addr, info->ai_addrlen);

    struct timeval recv_timeout = {
        .tv_sec = 1,
        .tv_usec = 0,
    };
    if (sock >= 0)
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

    freeaddrinfo(info);

    if (ret < 0)
        ESP_LOGE(TAG, "could not bind a UDP port!");

    int sport;
    SOCKADDR local;
    socklen_t local_len = sizeof(local);
    getsockname(sock, (struct sockaddr*)&local, &local_len);
#ifdef AF_INET6
    if (local.SAFAMILY == AF_INET6) {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6*)&local;
        sport = htons(sa6->sin6_port);
    } else
#endif
    {
        struct sockaddr_in *sa = (struct sockaddr_in*)&local;
        sport = htons(sa->sin_port);
    }

    return sport;
}


int rtp_setup(SOCKADDR *remote, int cport, int tport) {
    TaskHandle_t current_thread = xTaskGetCurrentTaskHandle();

    ensure_rtp_stop_sem();

    if (rtp_thread != NULL) {
        rtp_owner_thread = current_thread;
        xSemaphoreTake(rtp_stop_sem, 0);
        xTaskNotifyGive(rtp_thread);
        if (xSemaphoreTake(rtp_stop_sem, pdMS_TO_TICKS(2000)) != pdTRUE) {
            ESP_LOGW(TAG, "timed out waiting for RTP thread shutdown");
            return 0;
        }
        if (rtp_thread != NULL) {
            ESP_LOGW(TAG, "RTP thread still running during setup handoff");
            return 0;
        }
    }
    ESP_LOGD(TAG, "rtp_setup: cport=%d tport=%d\n", cport, tport);

    // we do our own timing and ignore the timing port.
    // an audio perfectionist may wish to learn the protocol.

    memcpy(&rtp_client, remote, sizeof(rtp_client));
#ifdef AF_INET6
    if (rtp_client.SAFAMILY == AF_INET6) {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6*)&rtp_client;
        sa6->sin6_port = htons(cport);
    } else
#endif
    {
        struct sockaddr_in *sa = (struct sockaddr_in*)&rtp_client;
        sa->sin_port = htons(cport);
    }

    int sport = bind_port(remote);

    ESP_LOGD(TAG, "rtp listening on port %d\n", sport);

    rtp_owner_thread = current_thread;
    BaseType_t ret = xTaskCreate(rtp_receiver, "RTP Receiver", 8192, NULL, 3, &rtp_thread);
    assert(ret == pdPASS);

    return sport;
}

void rtp_shutdown(void) {
    //if (!running)
    //    ESP_LOGE(TAG, "rtp_shutdown called without active stream!");
    if (rtp_thread != NULL && rtp_owner_thread == xTaskGetCurrentTaskHandle()) {
        ESP_LOGD(TAG, "shutting down RTP thread\n");
        ensure_rtp_stop_sem();
        xSemaphoreTake(rtp_stop_sem, 0);
        xTaskNotifyGive(rtp_thread);
        if (xSemaphoreTake(rtp_stop_sem, pdMS_TO_TICKS(2000)) != pdTRUE)
            ESP_LOGW(TAG, "timed out waiting for RTP thread shutdown");
        rtp_owner_thread = NULL;
    }
}

void rtp_request_resend(seq_t first, seq_t last) {
//    if (!running) {
//        ESP_LOGE(TAG, "rtp_request_resend called without active stream!");
//    }

    ESP_LOGD(TAG, "requesting resend on %d packets (%04X:%04X)\n",
         seq_diff(first,last) + 1, first, last);

    char req[8];    // *not* a standard RTCP NACK
    req[0] = 0x80;
    req[1] = 0x55|0x80;  // Apple 'resend'
    *(unsigned short *)(req+2) = htons(1);  // our seqnum
    *(unsigned short *)(req+4) = htons(first);  // missed seqnum
    *(unsigned short *)(req+6) = htons(last-first+1);  // count

    sendto(sock, req, sizeof(req), 0, (struct sockaddr*)&rtp_client, sizeof(rtp_client));
}

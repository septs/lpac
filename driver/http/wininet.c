#include "wininet.h"

#include <cjson/cJSON_ex.h>
#include <euicc/interface.h>
#include <lpac/utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <wininet.h>

#ifdef interface
#    undef interface
#endif

static WINBOOL parse_url(const char *url, URL_COMPONENTS *urlComponents) {
    ZeroMemory(urlComponents, sizeof(URL_COMPONENTS));
    urlComponents->dwStructSize = sizeof(URL_COMPONENTS);
    urlComponents->dwHostNameLength = 256;
    urlComponents->lpszHostName = calloc(urlComponents->dwHostNameLength, 1);
    urlComponents->dwUrlPathLength = 256;
    urlComponents->lpszUrlPath = calloc(urlComponents->dwUrlPathLength, 1);
    urlComponents->nPort = INTERNET_DEFAULT_HTTPS_PORT;
    return InternetCrackUrlA(url, strlen(url), ICU_DECODE, urlComponents);
}

static char *build_request_headers(const char **headers) {
    size_t n = 0;
    for (int index = 0; headers[index] != NULL; index++) {
        n += strlen(headers[index]) + 2;
    }
    char text[n + 1];
    size_t offset = 0;
    for (int index = 0; headers[index] != NULL; index++) {
        strcat(text + offset, headers[index]);
        offset += strlen(headers[index]);
        strcat(text + offset, "\r\n");
        offset += 2;
    }
    memset(&text[offset - 2], 0, n - offset);
    return strdup(text);
}

static WINBOOL setup_https_options(const HINTERNET hRequest) {
    DWORD dwFlags;
    DWORD dwBuffLen = sizeof(dwFlags);
    if (!InternetQueryOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, &dwBuffLen))
        return FALSE;
    dwFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA;
    return InternetSetOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, dwBuffLen);
}

static WINBOOL query_status_code(const HINTERNET hConnect, uint32_t *rcode) {
    DWORD dwStatusCode = 0;
    DWORD dwSize = sizeof(dwStatusCode);
    const DWORD dwInfoLevel = HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER;
    if (!HttpQueryInfoA(hConnect, dwInfoLevel, &dwStatusCode, &dwSize, NULL))
        return FALSE;
    *rcode = dwStatusCode;
    return TRUE;
}

static int http_interface_transmit(struct euicc_ctx *ctx, const char *url, uint32_t *rcode, uint8_t **rx,
                                   uint32_t *rx_len, const uint8_t *tx, const uint32_t tx_len, const char **headers) {
    *rx = NULL;
    *rx_len = 0;
    URL_COMPONENTS urlComponents;
    int fret = 0;
    _cleanup_(InternetCloseHandle) HINTERNET hInternet;
    _cleanup_(InternetCloseHandle) HINTERNET hConnect;
    _cleanup_(InternetCloseHandle) HINTERNET hRequest;
    _cleanup_free_ uint8_t *buffer;
    DWORD bytesRead;

    if (!parse_url(url, &urlComponents))
        goto err;
    hInternet = InternetOpenA("gsma-rsp-lpad", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (hInternet == NULL)
        goto err;
    hConnect = InternetConnectA(hInternet, urlComponents.lpszHostName, urlComponents.nPort, "", "",
                                INTERNET_SERVICE_HTTP, 0, 0);
    if (hConnect == NULL)
        goto err;
    hRequest = HttpOpenRequestA(hConnect, "POST", urlComponents.lpszUrlPath, NULL, NULL, NULL,
                                INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD, 0);
    if (hRequest == NULL)
        goto err;
    if (!setup_https_options(hRequest))
        goto err;
    if (!HttpSendRequestA(hRequest, build_request_headers(headers), -1, &tx, tx_len))
        goto err;
    if (!HttpEndRequestA(hRequest, 0, 0, 0))
        goto err;
    if (!query_status_code(hConnect, rcode))
        goto err;

    buffer = calloc(BUFSIZ + 1, 1);
    *rx = NULL;
    *rx_len = 0;
    while (InternetReadFile(hRequest, buffer, BUFSIZ, &bytesRead) && bytesRead > 0) {
        if (*rx == NULL)
            *rx = calloc(bytesRead, 1);
        else
            *rx = realloc(*rx, *rx_len + bytesRead + 1);
        memcpy(*rx + *rx_len, buffer, bytesRead);
        *rx_len += bytesRead;
    }
    free(buffer);

    goto exit;

err:
    fret = -1;
    fprintf(stderr, "WinInet Error Code: %lu\n", GetLastError());
exit:
    return fret;
}

static int libhttpinterface_init(struct euicc_http_interface *ifstruct) {
    memset(ifstruct, 0, sizeof(struct euicc_http_interface));

    ifstruct->transmit = http_interface_transmit;

    return 0;
}

const struct euicc_driver driver_http_wininet = {
    .type = DRIVER_HTTP,
    .name = "wininet",
    .init = (int (*)(void *))libhttpinterface_init,
    .main = NULL,
    .fini = NULL,
};

#include "uart2_rx_engine.h"
#include <string.h>

/* ===== UART2 RX ENGINE DEBUG =====
 * Set U2RX_DEBUG to 1 to enable verbose diagnostics.
 * Turn OFF once stable (printing inside RX path is expensive).
 */
#ifndef U2RX_DEBUG
#define U2RX_DEBUG 0
#endif

#if U2RX_DEBUG
#define U2RX_DBG_PRINT(...)  print(__VA_ARGS__)
#else
#define U2RX_DBG_PRINT(...)  do{}while(0)
#endif


static inline uint32_t rb_mask(uint32_t x) { return x % U2_RING_SZ; }

void u2rx_init(uart2_rx_t *u2, UART_HandleTypeDef *huart, osSemaphoreId_t sem)
{
    memset(u2, 0, sizeof(*u2));
    u2->huart = huart;
    u2->rxSem = sem;
}

HAL_StatusTypeDef u2rx_start(uart2_rx_t *u2)
{
    if (!u2 || !u2->huart) return HAL_ERROR;

    HAL_StatusTypeDef st = HAL_UARTEx_ReceiveToIdle_DMA(u2->huart, u2->dma_buf, U2_DMA_RX_SZ);
    if (st != HAL_OK) return st;

    /* Reduce IRQ rate */
    if (u2->huart->hdmarx) {
        __HAL_DMA_DISABLE_IT(u2->huart->hdmarx, DMA_IT_HT);
    }
    return HAL_OK;
}

void u2rx_on_idle_event(uart2_rx_t *u2, uint16_t size)
{
    if (!u2 || size == 0) return; // Removed redundant rearm here


#if U2RX_DEBUG
    /* Quick visibility of incoming DMA chunk */
    U2RX_DBG_PRINT("[U2RX][IDLE] size=%u avail_before=%lu\r\n",
                   (unsigned)size, (unsigned long)u2rx_available(u2));
    /* Print ASCII (escaped) for small chunks */
    if (size <= 64) {
        U2RX_DBG_PRINT("[U2RX][IDLE] ASCII: ");
        for (uint16_t k = 0; k < size; k++) {
            uint8_t c = u2->dma_buf[k];
            if (c == '\r') U2RX_DBG_PRINT("\\r");
            else if (c == '\n') U2RX_DBG_PRINT("\\n");
            else if (c >= 32 && c <= 126) U2RX_DBG_PRINT("%c", c);
            else U2RX_DBG_PRINT(".");
        }
        U2RX_DBG_PRINT("\r\n");
    }
#endif

/* Copy DMA chunk into ring */
    for (uint16_t i = 0; i < size; i++) {
        u2->ring[rb_mask(u2->wr)] = u2->dma_buf[i];
        u2->wr++;

        if ((u2->wr - u2->rd) > (U2_RING_SZ - 16)) {
            u2->rd = u2->wr - (U2_RING_SZ / 2);
        }
    }

    if (u2->rxSem) {
        osSemaphoreRelease(u2->rxSem);
    }
    // REMOVED: HAL_UARTEx_ReceiveToIdle_DMA call from here
}

size_t u2rx_available(uart2_rx_t *u2)
{
    return (size_t)(u2->wr - u2->rd);
}

size_t u2rx_read(uart2_rx_t *u2, uint8_t *out, size_t max)
{
    size_t n = u2rx_available(u2);
    if (n > max) n = max;

    for (size_t i = 0; i < n; i++) {
        out[i] = u2->ring[rb_mask(u2->rd)];
#if U2RX_DEBUG
        /* Very selective debug to avoid flooding: print only control chars and 'O' 'K' */
        if (max == 1) {
            uint8_t cdbg = out[i];
            if (cdbg == '\n' || cdbg == '\r' || cdbg == 'O' || cdbg == 'K' || cdbg == '+') {
                U2RX_DBG_PRINT("[U2RX][POP] ch=0x%02X '%c' rd=%lu wr=%lu avail=%lu task=%s\r\n",
                               (unsigned)cdbg,
                               (cdbg >= 32 && cdbg <= 126) ? cdbg : '.',
                               (unsigned long)u2->rd,
                               (unsigned long)u2->wr,
                               (unsigned long)u2rx_available(u2),
                               osThreadGetName(osThreadGetId()));
            }
        }
#endif
        u2->rd++;
    }
    return n;
}

void u2rx_drop_all(uart2_rx_t *u2)
{
    u2->rd = u2->wr;
}

/* Read a CRLF-terminated line into out (without CRLF). Returns length or -1 timeout. */
int u2rx_read_line(uart2_rx_t *u2, char *out, int out_sz, uint32_t timeout_ms)
{
    if (!u2 || !out || out_sz <= 1) return 0;

    uint32_t t0 = HAL_GetTick();
    int idx = 0;
    out[0] = 0;

#if U2RX_DEBUG
    U2RX_DBG_PRINT("[RL] start timeout=%lu avail=%lu\r\n",
                   (unsigned long)timeout_ms, (unsigned long)u2rx_available(u2));
#endif

    while ((HAL_GetTick() - t0) < timeout_ms) {

        /* If nothing available, block briefly on semaphore (efficient) */
        if (u2rx_available(u2) == 0) {
            if (u2->rxSem) (void)osSemaphoreAcquire(u2->rxSem, 50);
            else osDelay(1);
            continue;
        }

        uint8_t ch;
        size_t r = u2rx_read(u2, &ch, 1);
        if (r == 0) {
            osDelay(1);
            continue;
        }

#if U2RX_DEBUG
        if (ch == '\n' || ch == '\r' || ch == 'O' || ch == 'K' || ch == '+') {
            U2RX_DBG_PRINT("[RL] ch=0x%02X '%c' idx=%d\r\n",
                           (unsigned)ch, (ch >= 32 && ch <= 126) ? ch : '.', idx);
        }
#endif

        if (ch == '\r') continue;

        if (ch == '\n') {
            if (idx == 0) continue;  /* skip empty lines */
            out[idx] = 0;
#if U2RX_DEBUG
            U2RX_DBG_PRINT("[RL] EOL return len=%d text='%s'\r\n", idx, out);
#endif
            return idx;
        }

        if (idx < out_sz - 1) out[idx++] = (char)ch;
    }

    /* IMPORTANT: return partial line if we collected something */
    if (idx > 0) {
        out[idx] = 0;
#if U2RX_DEBUG
        U2RX_DBG_PRINT("[RL] TIME return PARTIAL len=%d text='%s'\r\n", idx, out);
#endif
        return idx;
    }

#if U2RX_DEBUG
    U2RX_DBG_PRINT("[RL] TIME return 0\r\n");
#endif
    return 0;
}

/* Wait until needle appears anywhere in stream (scans a moving window). */
int u2rx_wait_for(uart2_rx_t *u2, const char *needle, uint32_t timeout_ms)
{
    if (!needle || !needle[0]) return 1;

    const size_t N = strlen(needle);
    char win[96];
    if (N >= sizeof(win)) return 0;

    memset(win, 0, sizeof(win));
    size_t wlen = 0;

    uint32_t t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < timeout_ms) {

        if (u2rx_available(u2) == 0) {
            if (u2->rxSem) (void)osSemaphoreAcquire(u2->rxSem, 50);
            continue;
        }

        uint8_t ch;
        u2rx_read(u2, &ch, 1);

        if (wlen < sizeof(win) - 1) {
            win[wlen++] = (char)ch;
            win[wlen] = 0;
        } else {
            memmove(win, win + 1, sizeof(win) - 2);
            win[sizeof(win) - 2] = (char)ch;
            win[sizeof(win) - 1] = 0;
        }

        if (strstr(win, needle)) return 1;
    }

    return 0;
}

/* Utility: wait for start_tag then copy bytes until end_tag into out */
int u2rx_find_and_copy_between(uart2_rx_t *u2,
                              const char *start_tag,
                              const char *end_tag,
                              char *out, int out_sz,
                              uint32_t timeout_ms)
{
    if (!out || out_sz <= 1) return 0;
    out[0] = 0;

    if (!u2rx_wait_for(u2, start_tag, timeout_ms)) return 0;

    uint32_t t0 = HAL_GetTick();
    int idx = 0;

    /* Now copy until end_tag seen */
    char tail[64] = {0};
    int  tail_len = 0;

    while ((HAL_GetTick() - t0) < timeout_ms) {
        if (u2rx_available(u2) == 0) {
            if (u2->rxSem) (void)osSemaphoreAcquire(u2->rxSem, 50);
            continue;
        }

        uint8_t ch;
        u2rx_read(u2, &ch, 1);

        /* track tail to detect end_tag */
        if (tail_len < (int)sizeof(tail) - 1) {
            tail[tail_len++] = (char)ch;
            tail[tail_len] = 0;
        } else {
            memmove(tail, tail + 1, sizeof(tail) - 2);
            tail[sizeof(tail) - 2] = (char)ch;
            tail[sizeof(tail) - 1] = 0;
        }

        if (idx < out_sz - 1) out[idx++] = (char)ch;
        out[idx] = 0;

        if (end_tag && strstr(tail, end_tag)) {
            return 1;
        }
    }

    return 0;
}

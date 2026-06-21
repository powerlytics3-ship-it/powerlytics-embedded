//#include "mqtt_config.h"
//#include <string.h>
//#include <stdio.h>
//#include "debug_uart.h"
//
//
///* ---- OPTIONAL VALIDATOR HOOK ----
// * If you already have a validator (cJSON-based, etc.), declare it here.
// * For now we provide a weak stub that just checks it's non-empty JSON-ish.
// */
//static bool ExtractJsonSlice(const char *json, size_t len,
//                             const char **out_ptr, size_t *out_len)
//{
//    if (!json) return false;
//
//    // If len wasn't reliable, fall back to strlen (buffer is NUL-terminated in your code)
//    if (len == 0) {
//        len = strnlen(json, 1024*16); // safety cap
//    }
//
//    const char *start = NULL;
//    const char *end   = NULL;
//
//    // Find first '{'
//    for (size_t i = 0; i < len; i++) {
//        if (json[i] == '{') { start = &json[i]; break; }
//    }
//    if (!start) return false;
//
//    // Find last '}'
//    for (size_t i = len; i > 0; i--) {
//        if (json[i-1] == '}') { end = &json[i-1]; break; }
//    }
//    if (!end || end < start) return false;
//
//    // Optional: check braces balance between start..end
//    int depth = 0;
//    for (const char *p = start; p <= end; ++p) {
//        if (*p == '{') depth++;
//        else if (*p == '}') depth--;
//        if (depth < 0) return false;
//    }
//    if (depth != 0) return false;
//
//    *out_ptr = start;
//    *out_len = (size_t)(end - start + 1);
//    return true;
//}
//
///* Replace the previous weak validator with this actual implementation */
//bool ValidateConfigJSON(const char *json, size_t len)
//{
//    const char *slice = NULL;
//    size_t slice_len = 0;
//
//    if (!ExtractJsonSlice(json, len, &slice, &slice_len)) {
//        print("[ValidateConfigJSON] Could not extract JSON slice.\r\n");
//        return false;
//    }
//
//    // At this point we know start/end braces are sane.
//    // If you later link cJSON, you can do a real parse here.
//    (void)slice; (void)slice_len;
//    return true;
//}
//
//bool MQTT_Config_HandleMessage(const char *payload, size_t len)
//{
//    print("[MQTT_Config_HandleMessage] Received config payload\r\n");
//    if (!payload) return false;
//
//    // Extract clean JSON slice
//    const char *slice = NULL;
//    size_t slice_len = 0;
//    if (!ExtractJsonSlice(payload, len, &slice, &slice_len)) {
//        print("[MQTT_Config_HandleMessage] JSON extract FAILED\r\n");
//        return false;
//    }
//
//    // Persist to flash
//    if (!FlashStorage_WriteJSON((const uint8_t*)slice, (uint32_t)slice_len)) {
//        print("[MQTT_Config_HandleMessage] Flash write FAILED\r\n");
//        return false;
//    }
//    print("[MQTT_Config_HandleMessage] Flash write OK (%u bytes)\r\n", (unsigned)slice_len);
//
//    // Chunked read-back (no giant RAM buffer needed)
//    uint32_t total = 0;
//    if (!FlashStorage_GetStoredLength(&total)) {
//        print("[MQTT_Config_HandleMessage] Flash read-back FAILED (len)\r\n");
//        return false;
//    }
//    print("[MQTT_Config_HandleMessage] Read-back total=%lu bytes\r\n", (unsigned long)total);
//
//    uint8_t chunk[256 + 1];  // +1 for NUL when printing
//    uint32_t off = 0, n = 0;
//    while (off < total) {
//        if (!FlashStorage_ReadJSONChunk(off, chunk, 256, &n)) {
//            print("[MQTT_Config_HandleMessage] Chunk read FAILED at off=%lu\r\n", (unsigned long)off);
//            return false;
//        }
//        if (n == 0) break;
//        chunk[n] = '\0';  // safe for print()
//        print("%s", (const char*)chunk);
//        off += n;
//    }
//    print("\r\n");
//
//    return true;
//}
//
//
//bool MQTT_Config_GetLast(char *out_buf, size_t out_buf_size, size_t *out_len)
//{
//    if (!out_buf || out_buf_size == 0) return false;
//
//    uint32_t total = 0;
//    if (!FlashStorage_GetStoredLength(&total)) {
//        print("[MQTT_Config_GetLast] No valid config in flash\r\n");
//        if (out_len) *out_len = 0;
//        return false;
//    }
//
//    size_t need = (size_t)total + 1U; // +1 for NUL
//    size_t written = 0;
//
//    if (out_buf_size < need) {
//        // Copy as much as fits-1, NUL-terminate, return false to indicate truncation
//        uint32_t off = 0, n = 0;
//        while (off < total && written + 1 < out_buf_size) {
//            size_t room = out_buf_size - 1 - written;
//            uint32_t want = (room > 256) ? 256U : (uint32_t)room;
//            if (!FlashStorage_ReadJSONChunk(off, (uint8_t*)out_buf + written, want, &n)) {
//                print("[MQTT_Config_GetLast] Chunk read FAILED at off=%lu\r\n", (unsigned long)off);
//                if (out_len) *out_len = written;
//                out_buf[written] = '\0';
//                return false;
//            }
//            if (n == 0) break;
//            off += n;
//            written += n;
//        }
//        out_buf[written] = '\0';
//        if (out_len) *out_len = written;
//        print("[MQTT_Config_GetLast] Buffer too small (have=%lu need=%lu). Returned truncated JSON (%lu bytes)\r\n",
//              (unsigned long)out_buf_size, (unsigned long)need, (unsigned long)written);
//        return false; // signal truncation
//    }
//
//    // Enough room: copy full JSON
//    uint32_t off = 0, n = 0;
//    while (off < total) {
//        uint32_t want = (total - off > 256U) ? 256U : (total - off);
//        if (!FlashStorage_ReadJSONChunk(off, (uint8_t*)out_buf + written, want, &n)) {
//            print("[MQTT_Config_GetLast] Chunk read FAILED at off=%lu\r\n", (unsigned long)off);
//            if (out_len) *out_len = written;
//            out_buf[written] = '\0';
//            return false;
//        }
//        if (n == 0) break;
//        off += n;
//        written += n;
//    }
//    out_buf[written] = '\0';
//    if (out_len) *out_len = (size_t)total;
//    print("[MQTT_Config_GetLast] Returned last config (%lu bytes)\r\n", (unsigned long)total);
//    return true;
//}
//
//

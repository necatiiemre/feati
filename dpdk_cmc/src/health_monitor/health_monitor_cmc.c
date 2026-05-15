#include "health_monitor.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

// ============================================================================
// ANSI Renk ve Format Kodları (Terminal Okunabilirliği İçin)
// ============================================================================
#define C_RESET  "\x1b[0m"
#define C_CYAN   "\x1b[36m"
#define C_BOLD   "\x1b[1m"
#define C_YELLOW "\x1b[33m"
#define C_DIM    "\x1b[2m"

// Toplam tablo genişliği (Çerçeveler hariç iç alan)
#define HM_TABLE_WIDTH 112

// ============================================================================
// VL-ID → kullanıcı dostu etiket
// ============================================================================
static const char *hm_vl_label(uint16_t vl_id)
{
    switch (vl_id) {
        case 2021: return "DPM-1";
        case 2042: return "DPM-2";
        case 2063: return "DPM-3";
        case 2084: return "DPM-4";
        case 2105: return "DPM-5";
        case 8009: return "DSM-A";
        case 8109: return "DSM-B";
        default:   return "VL?";
    }
}

// ============================================================================
// Decode yardımcıları (her biri kendi static buffer'ında string döner;
// aynı printf'te 2 kez kullanılabilir diye 2-slot ring)
// ============================================================================
static const char *fmt_ver(uint64_t v)
{
    static char rings[2][16];
    static int idx = 0;
    char *buf = rings[idx];
    idx = (idx + 1) & 1;
    unsigned a = (unsigned)((v >> 16) & 0xFF);
    unsigned b = (unsigned)((v >>  8) & 0xFF);
    unsigned c = (unsigned)( v        & 0xFF);
    snprintf(buf, 16, "%u.%u.%u", a, b, c);
    return buf;
}

static const char *fmt_temp(uint32_t v)
{
    static char buf[16];
    snprintf(buf, sizeof(buf), "%u.%02u C", v / 100u, v % 100u);
    return buf;
}

static const char *fmt_vcc(uint32_t v)
{
    static char buf[16];
    snprintf(buf, sizeof(buf), "%u.%04u V", v / 10000u, v % 10000u);
    return buf;
}

static const char *port_speed_str(uint64_t v)
{
    switch (v) {
        case 0: return "10M";
        case 1: return "100M";
        case 2: return "1G";
        default: return "?";
    }
}

static const char *ptp_dev_str(uint8_t v)
{
    switch (v) {
        case 0: return "slave";
        case 1: return "master";
        case 3: return "GM";
        default: return "?";
    }
}

static void banner(uint16_t vl_id, const char *title, unsigned packets)
{
    char buffer[128];
    if (packets > 1) {
        snprintf(buffer, sizeof(buffer), "[%s] %s  " C_YELLOW "(×%u packets)" C_CYAN, hm_vl_label(vl_id), title, packets);
    } else {
        snprintf(buffer, sizeof(buffer), "[%s] %s", hm_vl_label(vl_id), title);
    }

    printf("\n" C_CYAN C_BOLD);
    printf("╔════════════════════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    
    if (packets > 1) {
        char raw_buf[128];
        snprintf(raw_buf, sizeof(raw_buf), "[%s] %s  (×%u packets)", hm_vl_label(vl_id), title, packets);
        int pad = HM_TABLE_WIDTH - strlen(raw_buf);
        printf("║ %s%*s ║\n", buffer, (pad > 0 ? pad : 0), "");
    } else {
        int pad = HM_TABLE_WIDTH - (strlen(hm_vl_label(vl_id)) + strlen(title) + 3);
        printf("║ %s%*s ║\n", buffer, (pad > 0 ? pad : 0), "");
    }
    
    printf("╠════════════════════════════════════════════════════════════════════════════════════════════════════════════════╣\n");
    printf(C_RESET);
}

static void hr(void)
{
    printf(C_DIM);
    printf("╟────────────────────────────────────────────────────────────────────────────────────────────────────────────────╢\n");
    printf(C_RESET);
}

static void table_footer(void)
{
    printf(C_CYAN C_BOLD);
    printf("╚════════════════════════════════════════════════════════════════════════════════════════════════════════════════╝\n");
    printf(C_RESET "\n");
}

// ============================================================================
// Pcs_profile_stats
// ============================================================================
void print_pcs_profile_stats(const Pcs_profile_stats *d, uint16_t vl_id, unsigned packets)
{
    if (d == NULL) return;
    
    banner(vl_id, "PCS PROFILE STATS", packets);

    // 2 + 20 + 3 + 87 = 112
    printf("║  %-20s : %-87" PRIu64 " ║\n", "Sample Count", d->sample_count);
    printf("║  %-20s : %-84" PRIu64 " ns ║\n", "Latest Read Time", d->latest_read_time);
    printf("║  %-20s : %-84" PRIu64 " ns ║\n", "Total Run Time", d->total_run_time);

    hr();
    // 2 + 16 + 3 + 20 + 3 + 68 = 112
    printf("║  " C_BOLD "%-16s" C_RESET " │ " C_BOLD "%-20s" C_RESET " │ " C_BOLD "%-68s" C_RESET " ║\n", 
           "CPU EXEC TIME", "Percentage (%)", "Usage (ns)");
    printf("║  %-16s │ %-20u │ %-68" PRIu64 " ║\n", "Minimum", 
           d->cpu_exec_time.min_exec_time.percentage, d->cpu_exec_time.min_exec_time.usage);
    printf("║  %-16s │ %-20u │ %-68" PRIu64 " ║\n", "Maximum", 
           d->cpu_exec_time.max_exec_time.percentage, d->cpu_exec_time.max_exec_time.usage);
    printf("║  %-16s │ %-20u │ %-68" PRIu64 " ║\n", "Average", 
           d->cpu_exec_time.avg_exec_time.percentage, d->cpu_exec_time.avg_exec_time.usage);
    printf("║  %-16s │ %-20u │ %-68" PRIu64 " ║\n", "Latest", 
           d->cpu_exec_time.last_exec_time.percentage, d->cpu_exec_time.last_exec_time.usage);

    hr();
    // 2 + 16 + 3 + 20 + 3 + 20 + 3 + 45 = 112
    printf("║  " C_BOLD "%-16s" C_RESET " │ " C_BOLD "%-20s" C_RESET " │ " C_BOLD "%-20s" C_RESET " │ " C_BOLD "%-45s" C_RESET " ║\n", 
           "MEMORY PROFILE", "Total (B)", "Used (B)", "Max Used (B)");
    printf("║  %-16s │ %-20zu │ %-20zu │ %-45zu ║\n", "Heap", 
           d->heap_mem.total_size, d->heap_mem.used_size, d->heap_mem.max_used_size);
    printf("║  %-16s │ %-20zu │ %-20zu │ %-45zu ║\n", "Stack", 
           d->stack_mem.total_size, d->stack_mem.used_size, d->stack_mem.max_used_size);

    table_footer();
}

// ============================================================================
// COUNTERS_DPM / COUNTERS_DSM
// ============================================================================
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
static void print_counters_body(const uint64_t *send, const uint64_t *send_fail,
                                const uint64_t *receive, const uint64_t *crc_pass,
                                const uint64_t *crc_fail, const uint64_t *drop,
                                unsigned rows, const char *row_label)
{
    printf("║  " C_BOLD "%-4s" C_RESET " │ " C_BOLD "%-15s" C_RESET " │ " C_BOLD "%-12s" C_RESET " │ " 
           C_BOLD "%-15s" C_RESET " │ " C_BOLD "%-15s" C_RESET " │ " C_BOLD "%-12s" C_RESET " │ " 
           C_BOLD "%-19s" C_RESET " ║\n",
           row_label, "Send", "Send Fail", "Receive", "CRC Pass", "CRC Fail", "Pkg Drop");
    hr();

    unsigned shown = 0;
    for (unsigned i = 0; i < rows; i++) {
        uint64_t s = send[i], sf = send_fail[i], r = receive[i];
        uint64_t cp = crc_pass[i], cf = crc_fail[i], pd = drop[i];
        
        if ((s | sf | r | cp | cf | pd) == 0) continue;
        
        printf("║  %-4u │ %-15" PRIu64 " │ %-12" PRIu64 " │ %-15" PRIu64 
               " │ %-15" PRIu64 " │ %-12" PRIu64 " │ %-19" PRIu64 " ║\n",
               i, s, sf, r, cp, cf, pd);
        shown++;
    }
    
    if (shown == 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "(Tüm %u bağlantı noktası için sayaçlar sıfır)", rows);
        printf("║  " C_DIM "%-110s" C_RESET " ║\n", msg);
    }
    
    table_footer();
}
#pragma GCC diagnostic pop

void print_counters_dpm(const COUNTERS_DPM *d, uint16_t vl_id, unsigned packets)
{
    if (d == NULL) return;
    banner(vl_id, "INTER-LRM COUNTERS DPM", packets);
    print_counters_body(d->send_count, d->send_fail_count, d->receive_count,
                        d->crc_pass_count, d->crc_fail_count, d->pkg_drop_count,
                        SRC_MAX_CONN, "SRC");
}

void print_counters_dsm(const COUNTERS_DSM *d, uint16_t vl_id, unsigned packets)
{
    if (d == NULL) return;
    banner(vl_id, "INTER-LRM COUNTERS DSM", packets);
    print_counters_body(d->send_count, d->send_fail_count, d->receive_count,
                        d->crc_pass_count, d->crc_fail_count, d->pkg_drop_count,
                        DST_MAX_CONN, "DST");
}
#pragma GCC diagnostic pop

// ============================================================================
// tA664ESMonitoring
// ============================================================================
void print_dtn_es_monitoring(const tA664ESMonitoring *d, uint16_t vl_id, unsigned packets)
{
    if (d == NULL) return;
    banner(vl_id, "DTN ES MONITORING", packets);

    // Karmaşık formatlamalar (string birleştirmeler) için ve 107 limitini 
    // korumak için kullanacağımız geçici tampon:
    char buf[128];

    // ========================================================================
    // [ IDENTITY ]
    // ========================================================================
    printf("║  " C_BOLD C_CYAN "%-107s" C_RESET " ║\n", "[ IDENTITY ]");
    
    snprintf(buf, sizeof(buf), "%s   (raw=%" PRIu64 ")", fmt_ver(d->A664_ES_FW_VER), d->A664_ES_FW_VER);
    printf("║  %-30s : %-74s ║\n", "FW_VER", buf);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "DEV_ID", d->A664_ES_DEV_ID);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "MODE", d->A664_ES_MODE);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "CONFIG_ID", d->A664_ES_CONFIG_ID);
    
    snprintf(buf, sizeof(buf), "%" PRIu64 "   (%s)", d->A664_ES_BIT_STATUS, d->A664_ES_BIT_STATUS == 1 ? "PASS" : "FAIL");
    printf("║  %-30s : %-74s ║\n", "BIT_STATUS", buf);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "CONFIG_STATUS", d->A664_ES_CONFIG_STATUS);
    
    snprintf(buf, sizeof(buf), "0x%02X   (ES=%c SW=%c SW-ES=%c)",
           d->A664_BSP_CONFIG_STATUS,
           (d->A664_BSP_CONFIG_STATUS & 0x1) ? 'Y' : '-',
           (d->A664_BSP_CONFIG_STATUS & 0x2) ? 'Y' : '-',
           (d->A664_BSP_CONFIG_STATUS & 0x4) ? 'Y' : '-');
    printf("║  %-30s : %-74s ║\n", "BSP_CONFIG_STATUS", buf);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "VENDOR_TYPE", d->A664_ES_VENDOR_TYPE);
    printf("║  %-30s : %-74d ║\n", "SW_ES_ENABLE", (int)d->A664_SW_ES_ENABLE);

    // ========================================================================
    // [ PTP ]
    // ========================================================================
    hr();
    printf("║  " C_BOLD C_CYAN "%-107s" C_RESET " ║\n", "[ PTP ]");
    printf("║  %-30s : %-74u ║\n", "CONFIG_ID", d->A664_PTP_CONFIG_ID);
    snprintf(buf, sizeof(buf), "%u   (%s)", d->A664_PTP_DEVICE_TYPE, ptp_dev_str(d->A664_PTP_DEVICE_TYPE));
    printf("║  %-30s : %-74s ║\n", "DEVICE_TYPE", buf);
    printf("║  %-30s : %-74u ║\n", "RC_STATUS", d->A664_PTP_RC_STATUS);
    printf("║  %-30s : %-74u ║\n", "PORT_A_SYNC", d->A664_PTP_PORT_A_SYNC);
    printf("║  %-30s : %-74u ║\n", "PORT_B_SYNC", d->A664_PTP_PORT_B_SYNC);
    printf("║  %-30s : %-74u ║\n", "SYNC_VL_ID", d->A664_PTP_SYNC_VL_ID);
    printf("║  %-30s : %-74u ║\n", "REQ_VL_ID", d->A664_PTP_REQ_VL_ID);
    printf("║  %-30s : %-74u ║\n", "RES_VL_ID", d->A664_PTP_RES_VL_ID);
    printf("║  %-30s : %-74u ║\n", "TOD_NETWORK", d->A664_PTP_TOD_NETWORK);

    // ========================================================================
    // [ HW & LINK ]
    // ========================================================================
    hr();
    printf("║  " C_BOLD C_CYAN "%-107s" C_RESET " ║\n", "[ HW & LINK ]");
    snprintf(buf, sizeof(buf), "%s   (raw=%u)", fmt_temp(d->A664_ES_HW_TEMP), d->A664_ES_HW_TEMP);
    printf("║  %-30s : %-74s ║\n", "HW_TEMP", buf);
    
    snprintf(buf, sizeof(buf), "%s   (raw=%u)", fmt_vcc(d->A664_ES_HW_VCC_INT), d->A664_ES_HW_VCC_INT);
    printf("║  %-30s : %-74s ║\n", "HW_VCC_INT", buf);
    
    snprintf(buf, sizeof(buf), "%" PRIu64 "   (%s)", d->A664_ES_PORT_SPEED, port_speed_str(d->A664_ES_PORT_SPEED));
    printf("║  %-30s : %-74s ║\n", "PORT_SPEED", buf);
    
    snprintf(buf, sizeof(buf), "%" PRIu64 "   (%s)", d->A664_ES_PORT_A_STATUS, d->A664_ES_PORT_A_STATUS ? "UP" : "DOWN");
    printf("║  %-30s : %-74s ║\n", "PORT_A_STATUS", buf);
    
    snprintf(buf, sizeof(buf), "%" PRIu64 "   (%s)", d->A664_ES_PORT_B_STATUS, d->A664_ES_PORT_B_STATUS ? "UP" : "DOWN");
    printf("║  %-30s : %-74s ║\n", "PORT_B_STATUS", buf);

    // ========================================================================
    // [ TX COUNTERS ]
    // ========================================================================
    hr();
    printf("║  " C_BOLD C_CYAN "%-107s" C_RESET " ║\n", "[ TX COUNTERS ]");
    printf("║  %-30s : %-74" PRIu64 " ║\n", "TX_INCOMING", d->A664_ES_TX_INCOMING_COUNT);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "TX_A_OUTGOING", d->A664_ES_TX_A_OUTGOING_COUNT);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "TX_B_OUTGOING", d->A664_ES_TX_B_OUTGOING_COUNT);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "TX_VLID_DROP", d->A664_ES_TX_VLID_DROP_COUNT);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "TX_LMIN_LMAX_DROP", d->A664_ES_TX_LMIN_LMAX_DROP_COUNT);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "TX_MAX_JITTER_DROP", d->A664_ES_TX_MAX_JITTER_DROP_COUNT);

    // ========================================================================
    // [ RX COUNTERS ]
    // ========================================================================
    hr();
    printf("║  " C_BOLD C_CYAN "%-107s" C_RESET " ║\n", "[ RX COUNTERS ]");
    printf("║  %-30s : %-74" PRIu64 " ║\n", "RX_A_INCOMING", d->A664_ES_RX_A_INCOMING_COUNT);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "RX_B_INCOMING", d->A664_ES_RX_B_INCOMING_COUNT);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "RX_OUTGOING", d->A664_ES_RX_OUTGOING_COUNT);
    
    // --- Port A Drops (Taşmayı önlemek için 3 sütunlu ızgara yapısı) ---
    // ( 8 + 19 ) + ( 11 + 19 ) + ( 7 + 20 ) = 84 < 107 Güvenli!
    printf("║  " C_YELLOW "%-107s" C_RESET " ║\n", "Port A Drops:");
    printf("║      vlid    : %-19" PRIu64 " lmin/lmax : %-19" PRIu64 " net    : %-20" PRIu64 " ║\n",
           d->A664_ES_RX_A_VLID_DROP_COUNT, d->A664_ES_RX_A_LMIN_LMAX_DROP_COUNT, d->A664_ES_RX_A_NET_ERR_COUNT);
    printf("║      seq     : %-19" PRIu64 " crc       : %-19" PRIu64 " ipchk  : %-20" PRIu64 " ║\n",
           d->A664_ES_RX_A_SEQ_ERR_COUNT, d->A664_ES_RX_A_CRC_ERROR_COUNT, d->A664_ES_RX_A_IP_CHECKSUM_ERROR_COUNT);

    // --- Port B Drops ---
    printf("║  " C_YELLOW "%-107s" C_RESET " ║\n", "Port B Drops:");
    printf("║      vlid    : %-19" PRIu64 " lmin/lmax : %-19" PRIu64 " net    : %-20" PRIu64 " ║\n",
           d->A664_ES_RX_B_VLID_DROP_COUNT, d->A664_ES_RX_B_LMIN_LMAX_DROP_COUNT, d->A664_ES_RX_B_NET_ERR_COUNT);
    printf("║      seq     : %-19" PRIu64 " crc       : %-19" PRIu64 " ipchk  : %-20" PRIu64 " ║\n",
           d->A664_ES_RX_B_SEQ_ERR_COUNT, d->A664_ES_RX_B_CRC_ERROR_COUNT, d->A664_ES_RX_B_IP_CHECKSUM_ERROR_COUNT);

    // ========================================================================
    // [ BSP (Driver) ]
    // ========================================================================
    hr();
    printf("║  " C_BOLD C_CYAN "%-107s" C_RESET " ║\n", "[ BSP (DRIVER) ]");
    snprintf(buf, sizeof(buf), "%s   (raw=%" PRIu64 ")", fmt_ver(d->A664_BSP_VER), d->A664_BSP_VER);
    printf("║  %-30s : %-74s ║\n", "VER", buf);
    
    snprintf(buf, sizeof(buf), "pkts=%-15" PRIu64 " bytes=%-15" PRIu64 " errs=%-15" PRIu64, 
             d->A664_BSP_TX_PACKET_COUNT, d->A664_BSP_TX_BYTE_COUNT, d->A664_BSP_TX_ERROR_COUNT);
    printf("║  %-30s : %-74s ║\n", "TX", buf);
    
    snprintf(buf, sizeof(buf), "pkts=%-15" PRIu64 " bytes=%-15" PRIu64 " errs=%-15" PRIu64, 
             d->A664_BSP_RX_PACKET_COUNT, d->A664_BSP_RX_BYTE_COUNT, d->A664_BSP_RX_ERROR_COUNT);
    printf("║  %-30s : %-74s ║\n", "RX (Success)", buf);
    
    printf("║  %-30s : %-74" PRIu64 " ║\n", "RX (Missed Frames)", d->A664_BSP_RX_MISSED_FRAME_COUNT);
    
    // Değişken adı tam 30 karakter. Kalıbımıza %100 oturuyor.
    printf("║  %-30s : %-74" PRIu64 " ║\n", "ES_BSP_QUEUING_RX_VL_PORT_DROP", d->A664_ES_BSP_QUEUING_RX_VL_PORT_DROP_COUNT);

    table_footer();
}

// ============================================================================
// tA664SWMonitoring
// ============================================================================
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
void print_dtn_sw_monitoring(const tA664SWMonitoring *d, uint16_t vl_id, unsigned packets)
{
    if (d == NULL) return;
    banner(vl_id, "DTN SW MONITORING", packets);

    const a664SWMonitoringStatus *s = &d->status;
    char buf[128];

    // ========================================================================
    // [ STATUS ]
    // ========================================================================
    printf("║  " C_BOLD C_CYAN "%-107s" C_RESET " ║\n", "[ STATUS ]");
    
    snprintf(buf, sizeof(buf), "%s   (raw=%" PRIu64 ")", fmt_ver(s->A664_SW_VERSION), s->A664_SW_VERSION);
    printf("║  %-30s : %-74s ║\n", "SW_VERSION", buf);
    
    snprintf(buf, sizeof(buf), "%s   (raw=%" PRIu64 ")", fmt_ver(s->A664_SW_ES_VERSION), s->A664_SW_ES_VERSION);
    printf("║  %-30s : %-74s ║\n", "SW_ES_VERSION", buf);
    
    printf("║  %-30s : %-74u ║\n", "DEVICE_ID", s->A664_SW_DEVICE_ID);
    printf("║  %-30s : %-74u ║\n", "PORT_NUM", s->A664_SW_PORT_NUM);
    printf("║  %-30s : %-74u ║\n", "CONFIG_ID", s->A664_SW_CONFIG_ID);
    printf("║  %-30s : %-74u ║\n", "HEARTBEAT", s->A664_SW_HEARTBEAT);
    printf("║  %-30s : %-74u ║\n", "CURRENT_MODE", s->A664_SW_CURRENT_MODE);
    printf("║  %-30s : %-74u ║\n", "TOKEN_BUCKET", s->A664_SW_TOKEN_BUCKET_STATUS);
    printf("║  %-30s : %-74u ║\n", "AUTOMAC_UPDATE", s->A664_SW_AUTOMAC_UPDATE_STATUS);
    printf("║  %-30s : %-74u ║\n", "UPSTREAM_MODE", s->A664_SW_UPSTREAM_MODE_STATUS);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "VENDOR_TYPE", (uint64_t)s->A664_SW_VENDOR_TYPE);
    
    snprintf(buf, sizeof(buf), "%s   (raw=%u)", fmt_temp((uint32_t)s->A664_SW_TEMPERATURE), s->A664_SW_TEMPERATURE);
    printf("║  %-30s : %-74s ║\n", "TEMPERATURE", buf);
    
    snprintf(buf, sizeof(buf), "%s   (raw=%u)", fmt_vcc((uint32_t)s->A664_SW_INTERNAL_VOLTAGE), s->A664_SW_INTERNAL_VOLTAGE);
    printf("║  %-30s : %-74s ║\n", "INTERNAL_VOLTAGE", buf);
    
    printf("║  %-30s : %-74" PRIu64 " ║\n", "TRANSCEIVER_TEMP", s->A664_SW_TRANSCEIVER_TEMP);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "SHARED_XCVR_TEMP", s->A664_SW_SHARED_TRANSCEIVER_TEMP);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "TX_TOTAL", s->A664_SW_TOT_TX_DATA_NUM);
    printf("║  %-30s : %-74" PRIu64 " ║\n", "RX_TOTAL", s->A664_SW_TOT_RX_DATA_NUM);
    
    snprintf(buf, sizeof(buf), "%" PRIu64 " s + %" PRIu64 " ns", s->A664_SW_TIME_OF_DAY_S, s->A664_SW_TIME_OF_DAY_NS);
    printf("║  %-30s : %-74s ║\n", "TIME_OF_DAY", buf);

    // ========================================================================
    // [ PORTS ] (Alt Tablo)
    // ========================================================================
    hr();
    snprintf(buf, sizeof(buf), "[ PORTS ]  (link=0 ve tüm sayaçları sıfır olanlar gizlendi)");
    printf("║  " C_BOLD C_CYAN "%-107s" C_RESET " ║\n", buf);
    
    // Alt Tablo Başlıkları
    printf("║  " C_BOLD "%-4s" C_RESET " │ " C_BOLD "%-4s" C_RESET " │ " C_BOLD "%-3s" C_RESET " │ " C_BOLD "%-3s" C_RESET " │ "
           C_BOLD "%-20s" C_RESET " │ " C_BOLD "%-20s" C_RESET " │ " C_BOLD "%-16s" C_RESET " │ " C_BOLD "%-16s" C_RESET " ║\n",
           "Port", "ID", "Lnk", "BIT", "TX Frames", "RX Frames", "CRC Errors", "Max Delay Errs");
    
    // İnce ara çizgi
    printf("║ ──────┼──────┼─────┼─────┼──────────────────────┼──────────────────────┼──────────────────┼────────────────── ║\n");

    int any = 0;
    for (unsigned i = 0; i < (unsigned)A664_SW_MAX_PORT_COUNT; i++) {
        const a664SWMonitoringPort *p = &d->port[i];
        
        // Port boşta mı kontrolü (Ağ trafiği yoksa ve link down ise çizme)
        uint64_t sum = p->A664_SW_CRC_ERR_CNT | p->A664_SW_TX_FRAME_CNT |
                       p->A664_SW_RX_FRAME_CNT | p->A664_SW_MAC_ERR_CNT |
                       p->A664_SW_TOKEN_ERR_CNT | p->A664_SW_MAX_DELAY_ERR_CNT;
                       
        if (p->A664_SW_PORT_LINK == 0 && sum == 0) continue;
        any = 1;
        
        // Port verilerini tablo hücresine oturtuyoruz
        printf("║  P%-3u │ %-4" PRIu64 " │ %-3u │ %-3u │ %-20" PRIu64 " │ %-20" PRIu64 " │ %-16" PRIu64 " │ %-16" PRIu64 " ║\n",
               i, p->A664_SW_PORT_ID, p->A664_SW_PORT_LINK, p->A664_SW_BIT_STATUS,
               p->A664_SW_TX_FRAME_CNT, p->A664_SW_RX_FRAME_CNT,
               p->A664_SW_CRC_ERR_CNT, p->A664_SW_MAX_DELAY_ERR_CNT);
    }
    
    if (!any) {
        printf("║  " C_DIM "%-107s" C_RESET " ║\n", "(All 12 ports idle)");
    }
    
    table_footer();
}
#pragma GCC diagnostic pop

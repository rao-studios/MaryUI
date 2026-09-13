/* System Settings' readers, over text the way the system prints it: an arm64
 * cpuinfo that names no processor (a Pi 5, and the Mac's virtual one), an x86
 * one, meminfo, key=value output, `ip -brief address`, `wpctl get-volume`, and
 * iwctl's coloured network list with its dimmed bars. Runs anywhere. */
#include <string.h>
#include "lp_test.h"
#include "maryui/lp_sysinfo.h"

LP_TEST(names_the_processor_and_counts_its_cores) {
    const char *pi5 =
        "processor\t: 0\nBogoMIPS\t: 108.00\nCPU implementer\t: 0x41\nCPU architecture: 8\nCPU part\t: 0xd0b\n\n"
        "processor\t: 1\nCPU implementer\t: 0x41\nCPU part\t: 0xd0b\n\nprocessor\t: 2\n\nprocessor\t: 3\n\n"
        "Revision\t: d04170\nModel\t\t: Raspberry Pi 5 Model B Rev 1.0\n";
    char cpu[128];
    int cores;
    lp_sysinfo_parse_cpuinfo(pi5, cpu, sizeof cpu, &cores);
    LP_ASSERT_EQ(cores, 4);
    LP_ASSERT_STR(cpu, "Raspberry Pi 5 Model B Rev 1.0");   /* the board's Model line wins when there is one */
    const char *vm = "processor\t: 0\nCPU implementer\t: 0x61\nCPU part\t: 0x000\n\nprocessor\t: 1\n";
    lp_sysinfo_parse_cpuinfo(vm, cpu, sizeof cpu, &cores);
    LP_ASSERT_EQ(cores, 2);
    LP_ASSERT_STR(cpu, "Apple silicon (virtual)");
    const char *a76 = "processor\t: 0\nCPU implementer\t: 0x41\nCPU part\t: 0xd0b\n";
    lp_sysinfo_parse_cpuinfo(a76, cpu, sizeof cpu, &cores);
    LP_ASSERT_STR(cpu, "Cortex-A76");
    const char *x86 = "processor\t: 0\nvendor_id\t: GenuineIntel\nmodel name\t: Intel(R) Core(TM) i7-8700 CPU @ 3.20GHz\n";
    lp_sysinfo_parse_cpuinfo(x86, cpu, sizeof cpu, &cores);
    LP_ASSERT_STR(cpu, "Intel(R) Core(TM) i7-8700 CPU @ 3.20GHz");
    LP_ASSERT(lp_sysinfo_parse_memtotal("MemTotal:        8123456 kB\nMemFree: 1 kB\n") == 8123456ULL * 1024);
    LP_ASSERT(lp_sysinfo_parse_memtotal("nothing") == 0);
}

LP_TEST(reads_key_value_output) {
    const char *show = "Timezone=America/New_York\nLocalRTC=no\nNTP=yes\nNTPSynchronized=no\nPRETTY_NAME=\"MaryOS 0.0 (Liquid Platinum)\"\n";
    char value[128];
    LP_ASSERT(lp_sysinfo_value(show, "Timezone", value, sizeof value));
    LP_ASSERT_STR(value, "America/New_York");
    LP_ASSERT(lp_sysinfo_value(show, "NTP", value, sizeof value));
    LP_ASSERT_STR(value, "yes");                               /* not NTPSynchronized */
    LP_ASSERT(lp_sysinfo_value(show, "PRETTY_NAME", value, sizeof value));
    LP_ASSERT_STR(value, "MaryOS 0.0 (Liquid Platinum)");
    LP_ASSERT(!lp_sysinfo_value(show, "RTC", value, sizeof value));
}

LP_TEST(lists_links_but_not_loopback) {
    const char *brief =
        "lo               UNKNOWN        127.0.0.1/8 ::1/128 \n"
        "enp0s1           UP             192.168.64.2/24 fe80::5054:ff:fe12:3456/64 \n"
        "wlan0            DOWN           \n"
        "veth0@if3        UP             10.0.0.1/24 \n";
    lp_net_link links[8];
    int n = lp_sysinfo_parse_links(brief, links, 8);
    LP_ASSERT_EQ(n, 3);
    LP_ASSERT_STR(links[0].name, "enp0s1");
    LP_ASSERT_STR(links[0].state, "UP");
    LP_ASSERT_STR(links[0].addresses, "192.168.64.2/24 fe80::5054:ff:fe12:3456/64");
    LP_ASSERT(!links[0].wireless);
    LP_ASSERT_STR(links[1].name, "wlan0");
    LP_ASSERT(links[1].wireless);
    LP_ASSERT_STR(links[1].addresses, "");
    LP_ASSERT_STR(links[2].name, "veth0");
}

LP_TEST(reads_wifi_networks_from_iwctl) {
    const char *networks =
        "                               Available networks                             \n"
        "--------------------------------------------------------------------------------\n"
        "      Network name                      Security            Signal\n"
        "--------------------------------------------------------------------------------\n"
        "  \x1b[0m> \x1b[0mHome Network                    psk                 ****\x1b[0m\n"
        "      Cafe                              open                **\x1b[1;90m**\x1b[0m\n"
        "      Office 5G                         8021x               *\x1b[1;90m***\x1b[0m\n"
        "\n";
    lp_wifi_network list[8];
    int n = lp_sysinfo_parse_wifi(networks, list, 8);
    LP_ASSERT_EQ(n, 3);
    LP_ASSERT_STR(list[0].ssid, "Home Network");
    LP_ASSERT_STR(list[0].security, "psk");
    LP_ASSERT_EQ(list[0].signal, 4);
    LP_ASSERT(list[0].connected);
    LP_ASSERT_STR(list[1].ssid, "Cafe");
    LP_ASSERT_STR(list[1].security, "open");
    LP_ASSERT_EQ(list[1].signal, 2);                            /* the dimmed bars are the ones it lacks */
    LP_ASSERT(!list[1].connected);
    LP_ASSERT_STR(list[2].ssid, "Office 5G");
    LP_ASSERT_EQ(list[2].signal, 1);
    LP_ASSERT_EQ(lp_sysinfo_parse_wifi("No station on device: 'wlan0'\n", list, 8), 0);
}

LP_TEST(reads_the_volume_and_mute) {
    double volume = -1;
    int muted = -1;
    LP_ASSERT(lp_sysinfo_parse_volume("Volume: 0.40\n", &volume, &muted));
    LP_ASSERT_NEAR(volume, 0.40, 1e-9);
    LP_ASSERT_EQ(muted, 0);
    LP_ASSERT(lp_sysinfo_parse_volume("Volume: 1.00 [MUTED]\n", &volume, &muted));
    LP_ASSERT_EQ(muted, 1);
    LP_ASSERT(!lp_sysinfo_parse_volume("Translate ID error: '@DEFAULT_AUDIO_SINK@' is not a valid ID\n", &volume, &muted));
}

LP_TEST(reads_this_machine_for_the_about_pane) {
    lp_about about;
    lp_sysinfo_about(&about);
    LP_ASSERT(about.os[0] != 0);
    LP_ASSERT(about.hostname[0] != 0);
}

int main(void) {
    LP_RUN(names_the_processor_and_counts_its_cores);
    LP_RUN(reads_key_value_output);
    LP_RUN(lists_links_but_not_loopback);
    LP_RUN(reads_wifi_networks_from_iwctl);
    LP_RUN(reads_the_volume_and_mute);
    LP_RUN(reads_this_machine_for_the_about_pane);
    LP_TEST_MAIN_END();
}

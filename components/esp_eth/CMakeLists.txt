set(esp_eth_srcs "src/esp_eth.c"
                 "src/esp_eth_phy_dp83848.c"
                 "src/esp_eth_phy_ip101.c"
                 "src/esp_eth_phy_lan8720.c"
                 "src/esp_eth_phy_rtl8201.c")

if(CONFIG_IDF_TARGET_ESP32)
    list(APPEND esp_eth_srcs "src/esp_eth_mac_esp32.c")
endif()

if(CONFIG_ETH_SPI_ETHERNET_DM9051)
    list(APPEND esp_eth_srcs "src/esp_eth_mac_dm9051.c"
                             "src/esp_eth_phy_dm9051.c")
endif()

idf_component_register(SRCS "${esp_eth_srcs}"
                       INCLUDE_DIRS "include"
                       LDFRAGMENTS "linker.lf"
                       REQUIRES "esp_event"
                       PRIV_REQUIRES "tcpip_adapter" "driver" "log")

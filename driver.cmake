list (APPEND dependencies
esp_wifi
bt
mqtt
esp_https_ota
utility		#os，dev_desc
)

list (APPEND src_list
"misc/ping.cpp"
"misc/ble_secure.cpp"
"misc/ota.cpp"
"misc/ota_fw.cpp"
"misc/topic_helper.cpp"

"link/wifi_info_store.cpp"
"link/wifi2.cpp"
"link/wifi_scan2.cpp"
"link/ble42.cpp"

"protocol/mqtt.cpp"
"protocol/tcp_socket.cpp"
"protocol/jsonrpc.cpp"
)

#!/bin/bash

# Run gNB with selected configuration
read -rp "Run WITHOUT the core network: [true/false]: " core

case "$core" in
	true) cn=true ;;
	t) cn=true ;;
	T) cn=true ;;
	1) cn=true ;;
	false) cn=false ;;
	f) cn=false ;;
	F) cn=false ;;
	0) cn=false ;;
	*) echo "Running by default with core network"; cn=false ;;
esac

echo "Enter the bandwidth that you want to test:"
read -rp "Enter choice [20/40/60/80/100]: " choice

case "$choice" in
	20) config=20 ;;
	40) config=40 ;;
	60) config=60 ;;
	80) config=80 ;;
	100) config=100 ;; 
	*) echo "Running by default with 100MHz bandwidth"; config=60 ;;
esac

echo "config file at ./srsRAN_Project/build/apps/gnb/gnb_e2_n3xx_100MHz_2x2.yml"

if [ -z "$usrp_ip" ]; then
    echo "Warning: No USRP ip addr found! Searching for serial number"
	usrp_ip=$(uhd_find_devices 2>/dev/null | awk '/serial:/ {print $2; exit}')
	if [ -z "$usrp_ip" ]; then
		echo "Warning: No USRP ip or serial found! Using default IP 192.168.20.4"
    	usrp_ip="192.168.20.4"
		usrp_args="addr=$usrp_ip"
		usrp_srate="122.88"
	else
		echo "Found USRP serial: $usrp_ip"
		usrp_args="serial=$usrp_ip"
		usrp_srate="30.72"
	fi
else
    echo "Found USRP at IP: $usrp_ip"
	usrp_args="addr=$usrp_ip"
	usrp_srate="122.88"
fi

cd build/apps/gnb

if ! strings -a ./gnb | grep -q "Parsed SrsDefaultsControlRequest"; then
    echo "ERROR: gnb binary does not include the updated SRS defaults control handler."
    echo "Rebuild with: cmake --build /home/ubuntu22/Documents/localization_oran/srsRAN_Project/build --target gnb -j\$(nproc)"
    exit 1
fi

echo "Using gnb binary: $(readlink -f ./gnb)"

echo "Running gnb with $config"
sudo sysctl -w net.core.wmem_max=33554432
sudo sysctl -w net.core.rmem_max=33554432
sudo sysctl -w net.core.wmem_default=33554432
sudo sysctl -w net.core.rmem_default=33554432
for ((i=0;i<$(nproc --all);i++)); do sudo cpufreq-set -c $i -r -g performance; done

sudo iptables -I FORWARD --in-interface enp7s0 --out-interface ogstun -j ACCEPT
sudo iptables -I FORWARD --in-interface ogstun --out-interface enp7s0 -j ACCEPT

sudo ./gnb -c gnb_e2_n3xx_100MHz_2x2.yml cu_cp amf --no_core "$cn" cell_cfg --channel_bandwidth_MHz "$config" ru_sdr --device_args "$usrp_args" --srate "$usrp_srate" e2 --addr="10.0.2.10" --bind_addr="10.0.2.1" --e2sm_srs_enabled=true
